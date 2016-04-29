#ifndef HNAP_SSDP_H_
#define HNAP_SSDP_H_

#include <netinet/in.h>
#include <net/if.h>

#ifdef ENABLE_IPV6
/* ipv6 address used for HTTP */
char ipv6_addr_for_http_with_brackets[64];
#endif

/* structure and list for storing lan addresses
 * with ascii representation and mask */
struct lan_addr_s {
	char ifname[IFNAMSIZ];	/* example: eth0 */
#ifdef ENABLE_IPV6
	unsigned int index;		/* use if_nametoindex() */
#endif
	char str[16];	/* example: 192.168.0.1 */
	struct in_addr addr, mask;	/* ip/mask */
#ifdef MULTIPLE_EXTERNAL_IP
	char ext_ip_str[16];
	struct in_addr ext_ip_addr;
#endif
};

struct lan_addr_s *lan_addr;

#ifdef __cplusplus
extern "C"{
#endif

/*
 *Function:OpenAndConfSSDPReceiveSocket()
 *Description:Open and configure the socket listening for
 * SSDP udp packets sent on 239.255.255.250 port 1900
 * SSDP v6 udp packets sent on FF02::C, or FF05::C, port 1900 
 *
 *Input Parameters:
 *	ipv6:1:for IP v6
 *		 0:for IP v4
 *
 *Return Values:
 *	-1:error
 *	 s:the socket file descriptor
 */
int
OpenAndConfSSDPReceiveSocket(int ipv6);

/*
* Function:OpenAndConfSSDPNotifySockets()
* Description:open socket for sending notifications
* Input Parameters:
*	sockets: a pointer to socket file description
* Return Values:
*   -1:error
*   0:success
*/
int
OpenAndConfSSDPNotifySockets(int *sockets);

/*
* Function:SendSSDPNotifies2()
* Description:send SSDP NOTIFY messages
* Input Parameters:
*	sockets:socket file descriptor
*   port: for LOCATION  header of NOTIFY message
*   lifetime: for CACHE-CONTROL header of NOTIFY message
*
* return: 0 succ
*      -1 fail
*/
int
SendSSDPNotifies2(int * sockets, const char * nt, const char * suffix, int ssl,
                unsigned short port, const char * path,
                const char * uuid, unsigned int lifetime);

/* 
 * Function:ProcessSSDPRequest()
 * Description:process SSDP M-SEARCH requests and responds to them 
 * Input Parameters:
 * 	s:the socket file descriptor
 *  port: for LOCATION header of reponse message to SSDP M-SEARCH requests
 *
 * return: 0 succ
 *      -1 fail
 */
int
ProcessSSDPRequest(int s, const char * nt, const char * suffix, int ssl,
                unsigned short port, const char * path,
                const char * uuid, unsigned int lifetime);


#ifdef __cplusplus
}
#endif
#endif
