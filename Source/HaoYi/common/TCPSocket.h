
#pragma once

#include "Socket.h"

class TCPSocket : public Socket
{
public:
	TCPSocket();
	~TCPSocket();
public:
	GM_Error 	Open(BOOL bIsIPV6) { return Socket::Open(SOCK_STREAM, bIsIPV6); }
	GM_Error	ConnectV4(UInt32 inRemoteAddr, UInt16 inRemotePort);
	GM_Error	Connect(LPCTSTR lpszRemoteAddr, int inRemotePort);
private:
	GM_Error	Connect(SOCKADDR * lpConAddr, int nAddrLen);
};