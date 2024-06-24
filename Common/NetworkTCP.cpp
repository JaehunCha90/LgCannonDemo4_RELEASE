//------------------------------------------------------------------------------------------------
// File: NetworkTCP.cpp
// Project: LG Exec Ed Program
// Versions:
// 1.0 April 2017 - initial version
// Provides the ability to send and recvive TCP byte streams for both Window and linux platforms
//------------------------------------------------------------------------------------------------
#include <iostream>
#include <new>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "NetworkTCP.h"

#if 1 //TLS_USED
#include <openssl/ssl.h>
#include <openssl/err.h>

extern pthread_mutex_t        TCP_Mutex;
SSL *connected_ssl;
SSL_CTX *ctx;
#endif
//-----------------------------------------------------------------
// OpenTCPListenPort - Creates a Listen TCP port to accept
// connection requests
//-----------------------------------------------------------------
TTcpListenPort *OpenTcpListenPort(short localport)
{
  TTcpListenPort *TcpListenPort;
  TcpListenPort= new (std::nothrow) TTcpListenPort;  
  
  if (TcpListenPort==NULL)
     {
      fprintf(stderr, "TUdpPort memory allocation failed\n");
      return(NULL);
     }
  TcpListenPort->ListenFd=BAD_SOCKET_FD;

 #if 1 //TLS_USED
    struct sockaddr_in addr;
   
    const SSL_METHOD* method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX_set_ecdh_auto(ctx, 1);
    // Load certificate and private key
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    TcpListenPort->ListenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (TcpListenPort->ListenFd < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    int option = 1;
    if (setsockopt(TcpListenPort->ListenFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(localport);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(TcpListenPort->ListenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }

    if (listen(TcpListenPort->ListenFd, 1) < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }

    return(TcpListenPort);
 #else
  TcpListenPort= new (std::nothrow) TTcpListenPort;  
  
  if (TcpListenPort==NULL)
     {
      fprintf(stderr, "TUdpPort memory allocation failed\n");
      return(NULL);
     }
  TcpListenPort->ListenFd=BAD_SOCKET_FD;
  #if  defined(_WIN32) || defined(_WIN64)
  WSADATA wsaData;
  int     iResult;
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) 
    {
     delete TcpListenPort;
     printf("WSAStartup failed: %d\n", iResult);
     return(NULL);
    }
#endif
  // create a socket
  if ((TcpListenPort->ListenFd= socket(AF_INET, SOCK_STREAM, 0)) == BAD_SOCKET_FD)
     {
      CloseTcpListenPort(&TcpListenPort);
      perror("socket failed");
      return(NULL);  
     }
  int option = 1; 

   if(setsockopt(TcpListenPort->ListenFd,SOL_SOCKET,SO_REUSEADDR,(char*)&option,sizeof(option)) < 0)
     {
      CloseTcpListenPort(&TcpListenPort);
      perror("setsockopt failed");
      return(NULL);
     }

  // bind it to all local addresses and pick any port number
  memset((char *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(localport);

  if (bind(TcpListenPort->ListenFd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
    {
      CloseTcpListenPort(&TcpListenPort);
      perror("bind failed");
      return(NULL); 
    }
   
 
  if (listen(TcpListenPort->ListenFd,5)< 0)
  {
      CloseTcpListenPort(&TcpListenPort);
      perror("bind failed");
      return(NULL);	  
  }
  return(TcpListenPort);
  #endif
}
//-----------------------------------------------------------------
// END OpenTCPListenPort
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// CloseTcpListenPort - Closes the specified TCP listen port
//-----------------------------------------------------------------
void CloseTcpListenPort(TTcpListenPort **TcpListenPort)
{
#if 1 //TLS_USED
  if ((*TcpListenPort)==NULL) return;
  if ((*TcpListenPort)->ListenFd!=BAD_SOCKET_FD)  
     {
      CLOSE_SOCKET((*TcpListenPort)->ListenFd);
      (*TcpListenPort)->ListenFd=BAD_SOCKET_FD;
     }
   delete (*TcpListenPort);
  (*TcpListenPort)=NULL;
  SSL_shutdown(connected_ssl);
  SSL_free(connected_ssl);
  SSL_CTX_free(ctx);  
  EVP_cleanup();
#else
  if ((*TcpListenPort)==NULL) return;
  if ((*TcpListenPort)->ListenFd!=BAD_SOCKET_FD)  
     {
      CLOSE_SOCKET((*TcpListenPort)->ListenFd);
      (*TcpListenPort)->ListenFd=BAD_SOCKET_FD;
     }
   delete (*TcpListenPort);
  (*TcpListenPort)=NULL;
#if  defined(_WIN32) || defined(_WIN64)
   WSACleanup();
#endif
#endif
}
//-----------------------------------------------------------------
// END CloseTcpListenPort
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// AcceptTcpConnection -Accepts a TCP Connection request from a 
// Listening port
//-----------------------------------------------------------------
TTcpConnectedPort *AcceptTcpConnection(TTcpListenPort *TcpListenPort, 
                       struct sockaddr_in *cli_addr,socklen_t *clilen)
{
  TTcpConnectedPort *TcpConnectedPort;

  TcpConnectedPort= new (std::nothrow) TTcpConnectedPort;  
  
  if (TcpConnectedPort==NULL)
     {
      fprintf(stderr, "TUdpPort memory allocation failed\n");
      return(NULL);
     }

  #if 1 //TLS_USED
  struct sockaddr_in addr;
  uint len = sizeof(addr);
  TcpConnectedPort->ConnectedFd = accept(TcpListenPort->ListenFd, (struct sockaddr*)&addr, &len);
  if (TcpConnectedPort->ConnectedFd < 0) {
      printf("Unable to accept\n");
      exit(EXIT_FAILURE);
  }

  connected_ssl = SSL_new(ctx); 
  SSL_set_mode(connected_ssl, SSL_MODE_AUTO_RETRY);

  if (connected_ssl == NULL) {
    printf("SSL_new() failed.\n");
  } else {
    printf("SSL_new() succeeded.\n");
  }
  SSL_state_string_long(connected_ssl);
  SSL_state_string(connected_ssl);

  SSL_set_fd(connected_ssl, TcpConnectedPort->ConnectedFd);

  if (SSL_accept(connected_ssl) <= 0) {
      printf("SSL_accept failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
      exit(EXIT_FAILURE);
  }

  int flags = fcntl(TcpConnectedPort->ConnectedFd, F_GETFL, 0);
  if (flags == -1) {
	  perror("fcntl F_GETFL");
	  exit(EXIT_FAILURE);
  }
  if (fcntl(TcpConnectedPort->ConnectedFd, F_SETFL, flags | O_NONBLOCK) == -1) {
	  perror("fcntl O_NONBLOCK");
	  exit(EXIT_FAILURE);
  }
 
  return TcpConnectedPort;
  #else

  TcpConnectedPort->ConnectedFd= accept(TcpListenPort->ListenFd,
                      (struct sockaddr *) cli_addr,clilen);
					  
  if (TcpConnectedPort->ConnectedFd== BAD_SOCKET_FD) 
  {
	perror("ERROR on accept");
	delete TcpConnectedPort;
	return NULL;
  }
  
 int bufsize = 200 * 1024;
 if (setsockopt(TcpConnectedPort->ConnectedFd, SOL_SOCKET, 
                 SO_RCVBUF, (char *)&bufsize, sizeof(bufsize)) == -1)
	{
         CloseTcpConnectedPort(&TcpConnectedPort);
         perror("setsockopt SO_SNDBUF failed");
         return(NULL);
	}
 if (setsockopt(TcpConnectedPort->ConnectedFd, SOL_SOCKET, 
                 SO_SNDBUF, (char *)&bufsize, sizeof(bufsize)) == -1)
	{
         CloseTcpConnectedPort(&TcpConnectedPort);
         perror("setsockopt SO_SNDBUF failed");
         return(NULL);
	}


 return TcpConnectedPort;
 #endif
}
//-----------------------------------------------------------------
// END AcceptTcpConnection
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// OpenTCPConnection - Creates a TCP Connection to a TCP port
// accepting connection requests
//-----------------------------------------------------------------
TTcpConnectedPort *OpenTcpConnection(const char *remotehostname, const char * remoteportno)
{
  TTcpConnectedPort *TcpConnectedPort;
  struct sockaddr_in myaddr;
  int                s;
  struct addrinfo   hints;
  struct addrinfo   *result = NULL;

  TcpConnectedPort= new (std::nothrow) TTcpConnectedPort;  
  
  if (TcpConnectedPort==NULL)
     {
      fprintf(stderr, "TUdpPort memory allocation failed\n");
      return(NULL);
     }
  TcpConnectedPort->ConnectedFd=BAD_SOCKET_FD;
  #if  defined(_WIN32) || defined(_WIN64)
  WSADATA wsaData;
  int     iResult;
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) 
    {
     delete TcpConnectedPort;
     printf("WSAStartup failed: %d\n", iResult);
     return(NULL);
    }
#endif
  // create a socket
   memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  
  s = getaddrinfo(remotehostname, remoteportno, &hints, &result);
  if (s != 0) 
    {
	  delete TcpConnectedPort;
      fprintf(stderr, "getaddrinfo: Failed\n");
      return(NULL);
    }
  if ( result==NULL)
    {
	  delete TcpConnectedPort;
      fprintf(stderr, "getaddrinfo: Failed\n");
      return(NULL);
    }
  if ((TcpConnectedPort->ConnectedFd= socket(AF_INET, SOCK_STREAM, 0)) == BAD_SOCKET_FD)
     {
      CloseTcpConnectedPort(&TcpConnectedPort);
	  freeaddrinfo(result);
      perror("socket failed");
      return(NULL);  
     }

  int bufsize = 200 * 1024;
  if (setsockopt(TcpConnectedPort->ConnectedFd, SOL_SOCKET,
                 SO_SNDBUF, (char *)&bufsize, sizeof(bufsize)) == -1)
	{
         CloseTcpConnectedPort(&TcpConnectedPort);
         perror("setsockopt SO_SNDBUF failed");
         return(NULL);
	}
  if (setsockopt(TcpConnectedPort->ConnectedFd, SOL_SOCKET, 
                 SO_RCVBUF, (char *)&bufsize, sizeof(bufsize)) == -1)
	{
         CloseTcpConnectedPort(&TcpConnectedPort);
         perror("setsockopt SO_SNDBUF failed");
         return(NULL);
	}
	 
  if (connect(TcpConnectedPort->ConnectedFd,result->ai_addr,result->ai_addrlen) < 0) 
          {
	    CloseTcpConnectedPort(&TcpConnectedPort);
	    freeaddrinfo(result);
            perror("connect failed");
            return(NULL);
	  }
  freeaddrinfo(result);	 
  return(TcpConnectedPort);
}
//-----------------------------------------------------------------
// END OpenTcpConnection
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// CloseTcpConnectedPort - Closes the specified TCP connected port
//-----------------------------------------------------------------
void CloseTcpConnectedPort(TTcpConnectedPort **TcpConnectedPort)
{
#if 1 //TLS_USED
  if ((*TcpConnectedPort)==NULL) return;
  if ((*TcpConnectedPort)->ConnectedFd!=BAD_SOCKET_FD)  
     {
      CLOSE_SOCKET((*TcpConnectedPort)->ConnectedFd);
      (*TcpConnectedPort)->ConnectedFd=BAD_SOCKET_FD;
     }
   delete (*TcpConnectedPort);
  (*TcpConnectedPort)=NULL;
  SSL_shutdown(connected_ssl);
  SSL_free(connected_ssl);
  SSL_CTX_free(ctx);  
  EVP_cleanup();
#else
  if ((*TcpConnectedPort)==NULL) return;
  if ((*TcpConnectedPort)->ConnectedFd!=BAD_SOCKET_FD)  
     {
      CLOSE_SOCKET((*TcpConnectedPort)->ConnectedFd);
      (*TcpConnectedPort)->ConnectedFd=BAD_SOCKET_FD;
     }
   delete (*TcpConnectedPort);
  (*TcpConnectedPort)=NULL;
#if  defined(_WIN32) || defined(_WIN64)
   WSACleanup();
#endif
#endif
}
//-----------------------------------------------------------------
// END CloseTcpListenPort
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// ReadDataTcp - Reads the specified amount TCP data 
//-----------------------------------------------------------------
ssize_t ReadDataTcp(TTcpConnectedPort *TcpConnectedPort,unsigned char *data, size_t length)
{
#if 1 //TLS_USED
	int total_bytes_read = 0;
	unsigned int retry_count = 0;
	while (total_bytes_read < length) {
		pthread_mutex_lock(&TCP_Mutex);
		int bytes_read = SSL_read(connected_ssl,
				(char*)(data + total_bytes_read),
				(int)(length - total_bytes_read));
		if (bytes_read <= 0) {
			int err = SSL_get_error(connected_ssl, bytes_read);
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
				pthread_mutex_unlock(&TCP_Mutex);
				usleep(100*1000);
				continue;
			} else if (err == SSL_ERROR_SYSCALL) {
				pthread_mutex_unlock(&TCP_Mutex);
				fprintf(stderr, "Error: SSL_read(), errno=%d, errmsg=%s\n", err, strerror(errno));
				exit(EXIT_FAILURE);
			}
			pthread_mutex_unlock(&TCP_Mutex);
			return -1;
		}
		pthread_mutex_unlock(&TCP_Mutex);

		total_bytes_read += bytes_read;
	}
	return total_bytes_read;
#else
	ssize_t bytes;

	for (size_t i = 0; i < length; i += bytes) {
		if (i > length) {
			printf("Potential integer overflow detected.\n");
			return (-1);
		}

		if ((bytes = recv(TcpConnectedPort->ConnectedFd, (char *)(data+i), length  - i,0)) == -1) {
			return (-1);
		}
	}
	return(length);
#endif
}
//-----------------------------------------------------------------
// END ReadDataTcp
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// WriteDataTcp - Writes the specified amount TCP data 
//-----------------------------------------------------------------
ssize_t WriteDataTcp(TTcpConnectedPort *TcpConnectedPort,unsigned char *data, size_t length)
{
#if 1 //TLS_USED
	size_t total_bytes_written = 0;
	while (total_bytes_written < length) {
		int chunk_size = 8 * 1024;
		if (chunk_size > length - total_bytes_written) {
			chunk_size = length - total_bytes_written;
		}
		int ret = SSL_write(connected_ssl,(char *)(data+total_bytes_written), chunk_size);
		if (ret <= 0) {
			int err = SSL_get_error(connected_ssl, ret);
			fprintf(stderr, "Error: SSL_write(), errno=%d\n", err);
			if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
				usleep(3 * 1000);
				continue;
			} else if (err == SSL_ERROR_SYSCALL) {
				fprintf(stderr, "Error: SSL_write(), errno=%d, errmsg=%s\n", err, strerror(errno));
				exit(EXIT_FAILURE);
			}

			return -1;
		}
		total_bytes_written += ret;
	}
	return (ssize_t) total_bytes_written;
#else
	ssize_t total_bytes_written = 0;
	ssize_t bytes_written;
	while (total_bytes_written != length)
	{
		bytes_written = send(TcpConnectedPort->ConnectedFd,
				(char *)(data+total_bytes_written),
				length - total_bytes_written,0);
		if (bytes_written == -1)
		{
			return(-1);
		}
		total_bytes_written += bytes_written;
	}
	return(total_bytes_written);
#endif
}
//-----------------------------------------------------------------
// END WriteDataTcp
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------


