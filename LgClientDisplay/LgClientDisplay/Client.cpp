#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <atlstr.h>
#include <cstdlib>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\opencv.hpp>
#include <openssl\ssl.h>
#include <openssl\err.h>
#include "Message.h"
#include "Client.h"
#include "LgClientDisplay.h"
#include "TcpSendRecv.h"
#include "DisplayImage.h"
#include "AuditLog.h"

#define NETWORK_FAIL_5S      (5000)

enum InputMode { MsgHeader, Msg };
static  std::vector<uchar> sendbuff;//buffer for coding
static HANDLE hClientEvent = INVALID_HANDLE_VALUE;
static HANDLE hEndClientEvent = INVALID_HANDLE_VALUE;
static SOCKET Client = INVALID_SOCKET;
static SSL_CTX* ClientCtx = nullptr;
static SSL* ClientSsl = nullptr;
static cv::Mat ImageIn;
static DWORD ThreadClientID;
static HANDLE hThreadClient = INVALID_HANDLE_VALUE;

static DWORD WINAPI ThreadClient(LPVOID ivalue);
static void ClientSetExitEvent(void);
static void ClientCleanup(void);
static void CleanupOpenssl();

static inline int TLSErrorNo(void) {
	return SSL_get_error(ClientSsl, SOCKET_ERROR);
}

static inline int ReadData(unsigned char* data, int length)
{
	return ReadDataTls(ClientSsl, data, length);
}

static inline int WriteData(unsigned char* data, int length)
{
	return WriteDataTls(ClientSsl, data, length);
}

static void ClientSetExitEvent(void)
{
	if (hEndClientEvent != INVALID_HANDLE_VALUE)
		SetEvent(hEndClientEvent);
}

static void ClientCleanup(void)
{
	std::cout << "ClientCleanup" << std::endl;

	CleanupOpenssl();

	if (hClientEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(hClientEvent);
		hClientEvent = INVALID_HANDLE_VALUE;
	}

	if (hEndClientEvent != INVALID_HANDLE_VALUE) {
		CloseHandle(hEndClientEvent);
		hEndClientEvent = INVALID_HANDLE_VALUE;
	}
}

LoginState_t LoginToServer(const char* p_username, const char* p_password, bool with_new_password, const char* p_newpassword)
{
	TMesssageLoginRequest MsgLoginReq;
	TMesssageLoginState   MsgLoginRes;

	int msglen = sizeof(MsgLoginReq);

	memset(&MsgLoginReq, 0, sizeof(MsgLoginReq));
	memset(&MsgLoginRes, 0, sizeof(MsgLoginRes));

	//printf("TMesssageLogin len %d\n", msglen);
	//int body_len = msglen - sizeof(MsgLoginReq.Hdr);

	MsgLoginReq.Hdr.Len = htonl(msglen - sizeof(MsgLoginReq.Hdr));
	MsgLoginReq.Hdr.Type = htonl(MT_LOGIN_REQ);

	memcpy(MsgLoginReq.UserName, p_username, sizeof(MsgLoginReq.UserName));
	memcpy(MsgLoginReq.Password, p_password, sizeof(MsgLoginReq.Password));

	if (with_new_password && p_newpassword != nullptr) {
		MsgLoginReq.is_change_password = htonl(1);
		memcpy(MsgLoginReq.NewPassword, p_newpassword, sizeof(MsgLoginReq.NewPassword));
	} else {
		MsgLoginReq.is_change_password = htonl(0);
	}

	if (WriteData((unsigned char*)&MsgLoginReq, msglen) == msglen) {
		// recv login result
		if (ReadData((unsigned char*)&MsgLoginRes, sizeof(MsgLoginRes)) == sizeof(MsgLoginRes)) {
			if (ntohl(MsgLoginRes.Hdr.Type) == MT_LOGIN_RES) {
				return (LoginState_t)ntohl(MsgLoginRes.Login);
			}
		}
	}
	if (TLSErrorNo() == SSL_ERROR_SYSCALL) {
		std::cout << __func__ << "() Error -> Disconnect" << std::endl;
		PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
	}

	// default fail
	return LOGIN_FAIL;
}

bool SendCodeToServer(unsigned char Code)
{
	if (IsClientConnected()) {
		TMesssageCommands MsgCmd;
		const int msglen = sizeof(MsgCmd);
		MsgCmd.Hdr.Len = htonl(msglen - sizeof(MsgCmd.Hdr));
		MsgCmd.Hdr.Type = htonl(MT_COMMANDS);
		MsgCmd.Commands = Code;
		if (WriteData((unsigned char*)&MsgCmd, msglen) == msglen) {
			return true;
		}
		if (TLSErrorNo() == SSL_ERROR_SYSCALL) {
			std::cout << __func__ << "() Error -> Disconnect" << std::endl;
			PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
		}
	}
	return false;
}

bool SendCalibToServer(unsigned char Code)
{
	if (IsClientConnected()) {
		TMesssageCalibCommands MsgCmd;
		const int msglen = sizeof(MsgCmd);
		MsgCmd.Hdr.Len = htonl(msglen - sizeof(MsgCmd.Hdr));
		MsgCmd.Hdr.Type = htonl(MT_CALIB_COMMANDS);
		MsgCmd.Commands = Code;
		if (WriteData((unsigned char*)&MsgCmd, msglen) == msglen) {
			return true;
		}
		if (TLSErrorNo() == SSL_ERROR_SYSCALL) {
			std::cout << __func__ << "() Error -> Disconnect" << std::endl;
			PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
		}
	}
	return false;
}

bool SendTargetOrderToServer(char* TargetOrder)
{
	if (IsClientConnected()) {
		TMesssageTargetOrder MsgTargetOrder;
		int msglen = sizeof(TMesssageHeader) + (int)strlen((const char*)TargetOrder) + 1;
		MsgTargetOrder.Hdr.Len = htonl((int)strlen((const char*)TargetOrder) + 1);
		MsgTargetOrder.Hdr.Type = htonl(MT_TARGET_SEQUENCE);
		strcpy_s((char*)MsgTargetOrder.FiringOrder, sizeof(MsgTargetOrder.FiringOrder), TargetOrder);
		if (WriteData((unsigned char*)&MsgTargetOrder, msglen) == msglen) {
			return true;
		}
		if (TLSErrorNo() == SSL_ERROR_SYSCALL) {
			std::cout << __func__ << "() Error -> Disconnect" << std::endl;
			PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
		}
	}
	return false;
}

bool SendPreArmCodeToServer(char* Code)
{
	if (IsClientConnected()) {
		TMesssagePreArm MsgPreArm;
		int msglen = sizeof(TMesssageHeader) + (int)strlen(Code) + 1;
		MsgPreArm.Hdr.Len = htonl((int)strlen(Code) + 1);
		MsgPreArm.Hdr.Type = htonl(MT_PREARM);
		strcpy_s((char*)MsgPreArm.Code, sizeof(MsgPreArm.Code), Code);
		if (WriteData((unsigned char*)&MsgPreArm, msglen) == msglen) {
			return true;
		}
		if (TLSErrorNo() == SSL_ERROR_SYSCALL) {
			std::cout << __func__ << "() Error -> Disconnect" << std::endl;
			PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
		}
	}
	return false;
}

bool SendStateChangeRequestToServer(SystemState_t State)
{
	if (IsClientConnected()) {
		TMesssageChangeStateRequest MsgChangeStateRequest;
		int msglen = sizeof(MsgChangeStateRequest);
		MsgChangeStateRequest.Hdr.Len = htonl(msglen - sizeof(MsgChangeStateRequest.Hdr));
		MsgChangeStateRequest.Hdr.Type = htonl(MT_STATE_CHANGE_REQ);
		MsgChangeStateRequest.State = (SystemState_t)htonl(State);
		if (WriteData((unsigned char*)&MsgChangeStateRequest, msglen) == msglen) {
			return true;
		}
		if (TLSErrorNo() == SSL_ERROR_SYSCALL) {
			std::cout << __func__ << "() Error -> Disconnect" << std::endl;
			PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
		}
	}
	return false;
}

bool SendChangeCAMStateRequestToServer(CAMState_t State)
{
	if (IsClientConnected()) {
		TMesssageChangeCAMStateRequest MsgChangeCAMStateRequest;
		int msglen = sizeof(MsgChangeCAMStateRequest);
		MsgChangeCAMStateRequest.Hdr.Len = htonl(msglen - sizeof(MsgChangeCAMStateRequest.Hdr));
		MsgChangeCAMStateRequest.Hdr.Type = htonl(MT_CAM_STATE_CHANGE_REQ);
		MsgChangeCAMStateRequest.State = (CAMState_t)htonl(State);
		if (WriteData((unsigned char*)&MsgChangeCAMStateRequest, msglen) == msglen) {
			return true;
		}
		if (TLSErrorNo() == SSL_ERROR_SYSCALL) {
			std::cout << __func__ << "() Error -> Disconnect" << std::endl;
			PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
		}
	}
	return false;
}

void closeServerSocket()
{
	if (Client != INVALID_SOCKET) {
		closesocket(Client);
		Client = INVALID_SOCKET;
	}
}

void InitializeOpenssl()
{
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();
}

void CleanupOpenssl()
{
	if (ClientSsl) {
		SSL_free(ClientSsl);
		ClientSsl = nullptr;
	}

	if (ClientCtx) {
		SSL_CTX_free(ClientCtx);
		ClientCtx = nullptr;
	}

	EVP_cleanup();

	if (Client != INVALID_SOCKET) {
		closesocket(Client);
		Client = INVALID_SOCKET;
	}
}

bool CreateClientContext()
{
	const SSL_METHOD* method = TLS_client_method();
	ClientCtx = SSL_CTX_new(method);

	if (!ClientCtx) {
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		return false;
	}

	return true;
}

bool ConfigureClientContext()
{
	// Load the trust store
	SSL_CTX_set_verify(ClientCtx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, nullptr);
	if (SSL_CTX_load_verify_locations(ClientCtx, "server.crt", nullptr) <= 0) {
		ERR_print_errors_fp(stderr);
		return false;
	}
	return true;
}

bool _TlsConnectToServer(const char* hostname, int port)
{
	int iResult;
	struct addrinfo  hints;
	struct addrinfo* ipAddr = NULL;
	char remotePortNo[128];

	sprintf_s(remotePortNo, sizeof(remotePortNo), "%d", port);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(hostname, remotePortNo, &hints, &ipAddr);
	if (iResult != 0) {
		std::cout << "getaddrinfo: Failed" << std::endl;
		return false;
	}

	if (ipAddr == NULL) {
		std::cout << "getaddrinfo: Failed" << std::endl;
		return false;
	}

	if ((Client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		freeaddrinfo(ipAddr);
		std::cout << "video client socket() failed with error " << WSAGetLastError() << std::endl;
		return false;
	}

	// Connect to server.
	iResult = connect(Client, ipAddr->ai_addr, (int)ipAddr->ai_addrlen);
	freeaddrinfo(ipAddr);
	if (iResult == SOCKET_ERROR) {
		std::cout << "connect function failed with error : " << WSAGetLastError() << std::endl;
		iResult = closesocket(Client);
		Client = INVALID_SOCKET;
		if (iResult == SOCKET_ERROR)
			std::cout << "closesocket function failed with error :" << WSAGetLastError() << std::endl;
		return false;
	}

	ClientSsl = SSL_new(ClientCtx);
	SSL_set_mode(ClientSsl, SSL_MODE_AUTO_RETRY);
	SSL_set_fd(ClientSsl, (int)Client);

	if (SSL_connect(ClientSsl) <= 0) {
		SSL_free(ClientSsl);
		ClientSsl = nullptr;
		ERR_print_errors_fp(stderr);
		closesocket(Client);
		Client = INVALID_SOCKET;
		return false;
	}

	std::cout << "Connected with " << SSL_get_cipher(ClientSsl) << " encryption" << std::endl;
	return true;
}


bool TlsConnectToServer(const char* remotehostname, unsigned short remoteport)
{
	InitializeOpenssl();
	if (!CreateClientContext())
		goto err;

	if (!ConfigureClientContext())
		goto err;

	if (!_TlsConnectToServer(remotehostname, remoteport))
		goto err;

	return true;

err:
	CleanupOpenssl();

	return false;
}

static inline bool HasTlsConnection(void)
{
	return ClientSsl != nullptr;
}

bool ConnectToServer(const char* remotehostname, unsigned short remoteport)
{
	if (HasTlsConnection())
		return true;

	return TlsConnectToServer(remotehostname, remoteport);
}

bool StartClient(void)
{
	hThreadClient = CreateThread(NULL, 0, ThreadClient, NULL, 0, &ThreadClientID);
	return true;
}

bool StopClient(void)
{
	ClientSetExitEvent();
	if (hThreadClient != INVALID_HANDLE_VALUE) {
		WaitForSingleObject(hThreadClient, INFINITE);
		CloseHandle(hThreadClient);
		hThreadClient = INVALID_HANDLE_VALUE;
	}
	return true;
}

bool IsClientConnected(void)
{
	if (hThreadClient == INVALID_HANDLE_VALUE) {
		return false;
	}
	else return true;
}


void ProcessMessage(char* MsgBuffer)
{
	TMesssageHeader* MsgHdr;
	MsgHdr = (TMesssageHeader*)MsgBuffer;
	MsgHdr->Len = ntohl(MsgHdr->Len);
	MsgHdr->Type = ntohl(MsgHdr->Type);

	switch (MsgHdr->Type) {
	case MT_IMAGE:
	{
		cv::imdecode(cv::Mat(MsgHdr->Len, 1, CV_8UC1, MsgBuffer + sizeof(TMesssageHeader)), cv::IMREAD_COLOR, &ImageIn);
		ProcessImage(ImageIn);

		//PostMessage(hWndMain, WM_MT_IMAGE, CAM_ON, 0);
	}
	break;

	case MT_TEXT:
	{
		CStringW cstring(MsgBuffer + sizeof(TMesssageHeader));
		PRINT(_T("%s\r\n"), cstring);
	}
	break;

	case MT_STATE:
	{
		TMesssageSystemState* MsgState;
		MsgState = (TMesssageSystemState*)MsgBuffer;
		MsgState->State = (SystemState_t)ntohl(MsgState->State);
		PostMessage(hWndMain, WM_SYSTEM_STATE, MsgState->State, 0);
	}
	break;

	case MT_CAM_STATE:
	{
		// TODO:
		/*
		TMesssageCAMState* MsgCAMState;
		MsgCAMState = (TMesssageCAMState*)MsgBuffer;
		MsgCAMState->State = (CAMState_t)ntohl(MsgCAMState->State);
		PostMessage(hWndMain, WM_CAM_STATE, MsgCAMState->State, 0);
		*/
	}
	break;

	case MT_HEARTBEAT:
	{
		static int count = 0;
		SetTimer(hWndMain, 1, NETWORK_FAIL_5S, NULL);
		// auditlog("Heart Beat!");
		// PRINT(_T("Heart Beat %d\r\n"), count++);
	}
	break;

	default:
	{
		printf("unknown message\n");
	}
	break;
	}
}

static DWORD WINAPI ThreadClient(LPVOID ivalue)
{
	HANDLE ghEvents[2];
	int NumEvents;
	InputMode Mode = MsgHeader;
	unsigned int InputBytesNeeded = sizeof(TMesssageHeader);
	TMesssageHeader MsgHdr;
	char* InputBuffer = NULL;
	char* InputBufferWithOffset = NULL;
	unsigned int CurrentInputBufferSize = 1024 * 10;

	InputBuffer = (char*)std::realloc(InputBuffer, CurrentInputBufferSize);
	InputBufferWithOffset = InputBuffer;

	if (InputBuffer == NULL) {
		std::cout << "InputBuffer Realloc failed" << std::endl;
		ExitProcess(0);
		return 1;
	}

	hClientEvent = WSACreateEvent();
	hEndClientEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (WSAEventSelect(Client, hClientEvent, FD_READ | FD_CLOSE) == SOCKET_ERROR) {
		std::cout << "WSAEventSelect() failed with error " << WSAGetLastError() << std::endl;
		int iResult = closesocket(Client);
		Client = INVALID_SOCKET;
		if (iResult == SOCKET_ERROR)
			std::cout << "closesocket function failed with error : " << WSAGetLastError() << std::endl;
		return 4;
	}

	ghEvents[0] = hEndClientEvent;
	ghEvents[1] = hClientEvent;
	NumEvents = 2;

	while (1) {
		DWORD dwEvent = WaitForMultipleObjects(
			NumEvents,			// number of objects in array
			ghEvents,			// array of objects
			FALSE,				// wait for any object
			INFINITE);			// INFINITE) wait

		if (dwEvent == WAIT_OBJECT_0) break;
		else if (dwEvent == WAIT_OBJECT_0 + 1) {
			WSANETWORKEVENTS NetworkEvents;
			if (SOCKET_ERROR == WSAEnumNetworkEvents(Client, hClientEvent, &NetworkEvents)) {
				std::cout << "WSAEnumNetworkEvent: " << WSAGetLastError() << "dwEvent " << dwEvent << " lNetworkEvent " << std::hex << NetworkEvents.lNetworkEvents << std::endl;
				NetworkEvents.lNetworkEvents = 0;
			} else {
				if (NetworkEvents.lNetworkEvents & FD_READ) {
					if (NetworkEvents.iErrorCode[FD_READ_BIT] != 0) {
						std::cout << "FD_READ failed with error " << NetworkEvents.iErrorCode[FD_READ_BIT] << std::endl;
					} else {
						int iResult;
						iResult = ReadData((unsigned char*)InputBufferWithOffset, InputBytesNeeded);
						if (iResult != SOCKET_ERROR) {
							if (iResult == 0) {
								Mode = MsgHeader;
								InputBytesNeeded = sizeof(TMesssageHeader);
								InputBufferWithOffset = InputBuffer;
								PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
								std::cout << "Connection closed on Recv" << std::endl;
								break;
							} else {
								InputBytesNeeded -= iResult;
								InputBufferWithOffset += iResult;
								if (InputBytesNeeded == 0) {
									if (Mode == MsgHeader) {
										InputBufferWithOffset = InputBuffer + sizeof(TMesssageHeader);
										memcpy(&MsgHdr, InputBuffer, sizeof(TMesssageHeader));
										MsgHdr.Len = ntohl(MsgHdr.Len);
										MsgHdr.Type = ntohl(MsgHdr.Type);
										InputBytesNeeded = MsgHdr.Len;
										Mode = Msg;
										if ((InputBytesNeeded + sizeof(TMesssageHeader)) > CurrentInputBufferSize) {
											CurrentInputBufferSize = InputBytesNeeded + sizeof(TMesssageHeader) + (10 * 1024);
											InputBuffer = (char*)std::realloc(InputBuffer, CurrentInputBufferSize);
											if (InputBuffer == NULL) {
												std::cout << "std::realloc failed " << std::endl;
												ExitProcess(0);
											}
											InputBufferWithOffset = InputBuffer + sizeof(TMesssageHeader);
										}
									} else if (Mode == Msg) {
										ProcessMessage(InputBuffer);
										// Setup for next message
										Mode = MsgHeader;
										InputBytesNeeded = sizeof(TMesssageHeader);
										InputBufferWithOffset = InputBuffer;
									}
								}
							}
						} else {
							switch (TLSErrorNo()) {
							case SSL_ERROR_WANT_READ:
								break;

							case SSL_ERROR_SYSCALL:
								PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
								std::cout << "ReadData() failed: SSL_ERROR_SYSCALL" << std::endl;
								break;

							default:
								std::cout << "ReadData() failed: Error No. " << TLSErrorNo() << std::endl;
								break;
							}
						}
					}

				}
				if (NetworkEvents.lNetworkEvents & FD_WRITE) {
					if (NetworkEvents.iErrorCode[FD_WRITE_BIT] != 0) {
						std::cout << "FD_WRITE failed with error " << NetworkEvents.iErrorCode[FD_WRITE_BIT] << std::endl;
					} else {
						std::cout << "FD_WRITE" << std::endl;
					}
				}

				if (NetworkEvents.lNetworkEvents & FD_CLOSE) {
					if (NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 0) {
						std::cout << "FD_CLOSE failed with error " << NetworkEvents.iErrorCode[FD_CLOSE_BIT] << std::endl;
					} else {
						std::cout << "FD_CLOSE" << std::endl;
						PostMessage(hWndMain, WM_CLIENT_LOST, 0, 0);
						break;
					}
				}
			}

		}
	}

	if (InputBuffer) {
		std::free(InputBuffer);
		InputBuffer = nullptr;
	}

	ClientCleanup();
	std::cout << "Client Exiting" << std::endl;
	return 0;
}

//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------
