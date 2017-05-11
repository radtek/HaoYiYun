
#pragma once

#include "Socket.h"
#include "Ws2tcpip.h"

class UDPSocket : public Socket
{
public:
	UDPSocket();
	~UDPSocket();
public:
	//通用的
	GM_Error 	Open(BOOL bIsIpV6 = FALSE ) { return Socket::Open( SOCK_DGRAM, bIsIpV6 ); }
	void 		SetTtl(UInt16 timeToLive);									// Set the TTL value...
	void		SetRemoteAddr(LPCSTR lpAddr, int nPort);
	//V4
	GM_Error	JoinMulticastForRecv(UINT MultiAddr, UINT InterAddr);		// Join Multicast for Receive...
	GM_Error	JoinMulticastForSend(UINT MultiAddr, UINT InterAddr);		// Join Multicast for Send...

	GM_Error 	LeaveMulticast(UInt32 inRemoteAddr);						// Leave the Multicast group...

	GM_Error	RecvFrom(UInt32* outRemoteAddr, UInt16* outRemotePort, void* ioBuffer, UInt32 inBufLen, UInt32* outRecvLen);
	GM_Error	SendTo(UInt32 inRemoteAddr, UInt16 inRemotePort, void* inBuffer, UInt32 inLength);
	GM_Error	SendTo(void* inBuffer, UInt32 inLength);

	UInt32		GetRemoteAddrV4() { return ntohl(m_MsgAddr.sin_addr.s_addr); }
	UInt16		GetRemotePortV4() { return ntohs(m_MsgAddr.sin_port); }

	GM_Error	RecvFrom(LPWSABUF lpBuf, LPWSAOVERLAPPED lpOver);
	GM_Error	SendTo(UInt32 inRemoteAddr, UInt16 inRemotePort, LPWSABUF lpBuf, LPWSAOVERLAPPED lpOver);
	//
	// 通用的适合 IPV4/IPV6 的链接方式...
	GM_Error	JoinMulticastForRecvCM(LPCTSTR lpszMultiAddr, LPCTSTR lpszInterAddr);
	GM_Error	JoinMulticastForSendCM(LPCTSTR lpszMultiAddr, LPCTSTR lpszInterAddr);
	GM_Error	LeaveMulticastCM(LPCTSTR lpszMultiAddr );
	GM_Error	SendToCM(LPCTSTR lpszMultiAddr, UInt16 inRemotePort, void* inBuffer, UInt32 inLength);
	GM_Error	RecvFromCM(LPCTSTR outRemoteAddr, UInt16* outRemotePort, void* ioBuffer, UInt32 inBufLen, UInt32* outRecvLen);
	GM_Error 	LeaveMulticastCM(UInt32 inRemoteAddr);						// Leave the Multicast group...
	GM_Error	SendToCM(void* inBuffer, UInt32 inLength);
	void		SetTtlCM(int timeToLive);

	GM_Error			BindCM( LPCTSTR lpszInterAddr, int port,int AFamily = AF_INET);
	void				SetRemoteAddrCM(LPCSTR lpAddr, int nPort);
	struct addrinfo *	TransformAddress(char *addr, char *port, int af, int type, int proto);
	int					FormatAddress(SOCKADDR *sa, int salen, char *addrbuf,int addrbuflen, int * iport);

	static int			GetAddrFamily(char *addr);
private:
	ip_mreq		m_IPmr;
	sockaddr_in	m_MsgAddr;
	int			m_nAddrLen;
	sockaddr_in	m_SendAddr;

	ipv6_mreq		m_IPV6mr;
	addrinfo		*m_lpMultiAddr;

	char       m_lpRemoteAddr[256];
	int        m_iPort;
	
};