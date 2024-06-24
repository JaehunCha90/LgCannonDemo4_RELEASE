#include "TcpSendRecv.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <stdio.h>

//-----------------------------------------------------------------
// ReadDataTls - Reads Available TLS data 
//-----------------------------------------------------------------
int ReadDataTls(SSL *ssl, unsigned char *data, int length)
{
	int total_bytes_read = 0;
	unsigned int retry_count = 0;
	while (total_bytes_read < length) {
		int bytes_read = SSL_read(ssl,
			(char*)(data + total_bytes_read),
			(int)(length - total_bytes_read));
		if (bytes_read <= 0) {
			if (SSL_get_error(ssl, bytes_read) == SSL_ERROR_WANT_READ)
				continue;
			return SOCKET_ERROR;
		}

		total_bytes_read += bytes_read;
	}
	return total_bytes_read;
}
//-----------------------------------------------------------------
// END Reads Available TLS data
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// WriteDataTls - Writes the specified amount TLS data 
//-----------------------------------------------------------------
int WriteDataTls(SSL *ssl, unsigned char* data, int length)
{
	int total_bytes_written = 0;
	unsigned int retry_count = 0;
	while (total_bytes_written < length)
	{
		int bytes_written = SSL_write(ssl,
			(char*)(data + total_bytes_written),
			(int)(length - total_bytes_written));
		if (bytes_written <= 0) {
			if (SSL_get_error(ssl, bytes_written) == SSL_ERROR_WANT_WRITE)
				continue;

			return SOCKET_ERROR;
		}

		total_bytes_written += bytes_written;
	}
	return total_bytes_written;
}
//-----------------------------------------------------------------
// END WriteDataTls
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------