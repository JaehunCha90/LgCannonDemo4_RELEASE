#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl\ssl.h>
#include <openssl\err.h>
int ReadDataTls(SSL* ssl, unsigned char* data, int length);
int WriteDataTls(SSL* socket, unsigned char* data, int length);
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------