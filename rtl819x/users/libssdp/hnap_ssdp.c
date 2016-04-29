/*
* hnap_ssdp.c
*
*<December,19,2012> <Logan Guo> <The libs for hanp SSDP module>
*
*/

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>

//#include <linux/in_route.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
//#include <linux/libnfnetlink.h>
#include "hnap_ssdp.h"

/* SSDP ip/port */
//char uuidvalue[] = "uuid:12345678-0000-0000-0000-00000000abcd";

#define SSDP_PORT (1900)
#define SSDP_MCAST_ADDR ("239.255.255.250")
#define LL_SSDP_MCAST_ADDR "FF02::C"
#define SL_SSDP_MCAST_ADDR "FF05::C"

/*
* Function:set_non_blocking
* Description: set the socket non blocking
* Input Parameters:
* 	fd:the socket file descriptor
* Return Parameters:
*	0:error
*	1:success
*/
int
set_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	if(flags < 0)
		return 0;
	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return 0;
	return 1;
}

int
sockaddr_to_string(const struct sockaddr * addr, char * str, size_t size)
{
	char buffer[64];
	unsigned short port = 0;
	int n = -1;

	switch(addr->sa_family)
	{
	case AF_INET6:
		inet_ntop(addr->sa_family,
		          &((struct sockaddr_in6 *)addr)->sin6_addr,
		          buffer, sizeof(buffer));
		port = ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
		n = snprintf(str, size, "[%s]:%hu", buffer, port);
		break;
	case AF_INET:
		inet_ntop(addr->sa_family,
		          &((struct sockaddr_in *)addr)->sin_addr,
		          buffer, sizeof(buffer));
		port = ntohs(((struct sockaddr_in *)addr)->sin_port);
		n = snprintf(str, size, "%s:%hu", buffer, port);
		break;
#ifdef AF_LINK
#if defined(__sun)
		/* solaris does not seem to have link_ntoa */
		/* #define link_ntoa _link_ntoa	*/
#define link_ntoa(x) "dummy-link_ntoa"
#endif
	case AF_LINK:
		{
			struct sockaddr_dl * sdl = (struct sockaddr_dl *)addr;
			n = snprintf(str, size, "index=%hu type=%d %s",
			             sdl->sdl_index, sdl->sdl_type,
			             link_ntoa(sdl));
		}
		break;
#endif
	default:
		n = snprintf(str, size, "unknown address family %d", addr->sa_family);
	}
	return n;
}

int
get_src_for_route_to(const struct sockaddr * dst,
                     void * src, size_t * src_len)
{
	int fd = -1;
	struct nlmsghdr *h;
	int status;
	struct {
		struct nlmsghdr n;
		struct rtmsg r;
		char buf[1024];
	} req;
	struct sockaddr_nl nladdr;
	struct iovec iov = {
		.iov_base = (void*) &req.n,
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	const struct sockaddr_in * dst4;
	const struct sockaddr_in6 * dst6;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_GETROUTE;
	req.r.rtm_family = dst->sa_family;
	req.r.rtm_table = 0;
	req.r.rtm_protocol = 0;
	req.r.rtm_scope = 0;
	req.r.rtm_type = 0;
	req.r.rtm_src_len = 0;
	req.r.rtm_dst_len = 0;
	req.r.rtm_tos = 0;

	{
		char dst_str[128];
		sockaddr_to_string(dst, dst_str, sizeof(dst_str));
		syslog(LOG_DEBUG, "get_src_for_route_to (%s)", dst_str);
	}
	/* add address */
	if(dst->sa_family == AF_INET) {
		dst4 = (const struct sockaddr_in *)dst;
		//nfnl_addattr_l(&req.n, sizeof(req), RTA_DST, &dst4->sin_addr, 4);
		req.r.rtm_dst_len = 32;
	} else {
		dst6 = (const struct sockaddr_in6 *)dst;
		//nfnl_addattr_l(&req.n, sizeof(req), RTA_DST, &dst6->sin6_addr, 16);
		req.r.rtm_dst_len = 128;
	}

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	req.n.nlmsg_seq = 1;
	iov.iov_len = req.n.nlmsg_len;

	status = sendmsg(fd, &msg, 0);

	if (status < 0) {
		syslog(LOG_ERR, "sendmsg(rtnetlink) : %m");
		return -1;
	}

	memset(&req, 0, sizeof(req));

	for(;;) {
		iov.iov_len = sizeof(req);
		status = recvmsg(fd, &msg, 0);
		if(status < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			syslog(LOG_ERR, "recvmsg(rtnetlink) %m");
			goto error;
		}
		if(status == 0) {
			syslog(LOG_ERR, "recvmsg(rtnetlink) EOF");
			goto error;
		}
		for (h = (struct nlmsghdr*)&req.n; status >= (int)sizeof(*h); ) {
			int len = h->nlmsg_len;
			int l = len - sizeof(*h);

			if (l<0 || len>status) {
				if (msg.msg_flags & MSG_TRUNC) {
					syslog(LOG_ERR, "Truncated message");
				}
				syslog(LOG_ERR, "malformed message: len=%d", len);
				goto error;
			}

			if(nladdr.nl_pid != 0 || h->nlmsg_seq != 1/*seq*/) {
				syslog(LOG_ERR, "wrong seq = %d\n", h->nlmsg_seq);
				/* Don't forget to skip that message. */
				status -= NLMSG_ALIGN(len);
				h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len));
				continue;
			}

			if(h->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(h);
				syslog(LOG_ERR, "NLMSG_ERROR %d : %s", err->error, strerror(-err->error));
				goto error;
			}
			if(h->nlmsg_type == RTM_NEWROUTE) {
				struct rtattr * rta;
				int len = h->nlmsg_len;
				len -= NLMSG_LENGTH(sizeof(struct rtmsg));
				for(rta = RTM_RTA(NLMSG_DATA((h))); RTA_OK(rta, len); rta = RTA_NEXT(rta,len)) {
					unsigned char * data = RTA_DATA(rta);
					if(rta->rta_type == RTA_PREFSRC) {
						if(*src_len < RTA_PAYLOAD(rta)) {
							//syslog(LOG_WARNING, "cannot copy src: %u<%lu",
							//       (unsigned)*src_len, RTA_PAYLOAD(rta));
							goto error;
						}
						*src_len = RTA_PAYLOAD(rta);
						memcpy(src, data, RTA_PAYLOAD(rta));
					}
				}
				close(fd);
				return 0;
			}
			status -= NLMSG_ALIGN(len);
			h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len));
		}
	}
	syslog(LOG_WARNING, "get_src_for_route_to() : src not found");
error:
	if(fd >= 0)
		close(fd);
	return -1;
}

/* AddMulticastMembership()
 * param s		socket
 * param ifaddr	ip v4 address
 */
static int
AddMulticastMembership(int s, in_addr_t ifaddr)
{
	struct ip_mreq imr;	/* Ip multicast membership */

    /* setting up imr structure */
    imr.imr_multiaddr.s_addr = inet_addr(SSDP_MCAST_ADDR);
    /*imr.imr_interface.s_addr = htonl(INADDR_ANY);*/
    imr.imr_interface.s_addr = ifaddr;	/*inet_addr(ifaddr);*/

	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&imr, sizeof(struct ip_mreq)) < 0)
	{
        printf("LOG_ERR setsockopt(udp, IP_ADD_MEMBERSHIP)\n");
		return -1;
    }

	return 0;
}

/* AddMulticastMembershipIPv6()
 * param s	socket (IPv6)
 * To be improved to target specific network interfaces */
#ifdef ENABLE_IPV6
static int
AddMulticastMembershipIPv6(int s)
{
	struct ipv6_mreq mr;
	/*unsigned int ifindex;*/

	memset(&mr, 0, sizeof(mr));
	inet_pton(AF_INET6, LL_SSDP_MCAST_ADDR, &mr.ipv6mr_multiaddr);
	/*mr.ipv6mr_interface = ifindex;*/
	mr.ipv6mr_interface = 0; /* 0 : all interfaces */
#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif
	if(setsockopt(s, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mr, sizeof(struct ipv6_mreq)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp, IPV6_ADD_MEMBERSHIP): %m");
		return -1;
	}
	inet_pton(AF_INET6, SL_SSDP_MCAST_ADDR, &mr.ipv6mr_multiaddr);
	if(setsockopt(s, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mr, sizeof(struct ipv6_mreq)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp, IPV6_ADD_MEMBERSHIP): %m");
		return -1;
	}
	return 0;
}
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
OpenAndConfSSDPReceiveSocket(int ipv6)
{
	int s;
	struct sockaddr_storage sockname;
	socklen_t sockname_len;
	
	int j = 1;

	if( (s = socket(ipv6 ? PF_INET6 : PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("Error:%s: socket(udp) error\n",
		       "OpenAndConfSSDPReceiveSocket");
		return -1;
	}

	memset(&sockname, 0, sizeof(struct sockaddr_storage));
	if(ipv6) {
		struct sockaddr_in6 * saddr = (struct sockaddr_in6 *)&sockname;
		saddr->sin6_family = AF_INET6;
		saddr->sin6_port = htons(SSDP_PORT);
		saddr->sin6_addr = in6addr_any;
		sockname_len = sizeof(struct sockaddr_in6);
	} else {
		struct sockaddr_in * saddr = (struct sockaddr_in *)&sockname;
		saddr->sin_family = AF_INET;
		saddr->sin_port = htons(SSDP_PORT);
		/* NOTE : it seems it doesnt work when binding on the specific address */
		/*saddr->sin_addr.s_addr = inet_addr(UPNP_MCAST_ADDR);*/
		saddr->sin_addr.s_addr = htonl(INADDR_ANY);
		/*saddr->sin_addr.s_addr = inet_addr(ifaddr);*/
		sockname_len = sizeof(struct sockaddr_in);
	}

	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(j)) < 0)
	{
		printf("warning:setsockopt(udp, SO_REUSEADDR)\n");
	}

	if(!set_non_blocking(s))
	{
		printf("Warning:%s: set_non_blocking()\n",
		       "OpenAndConfSSDPReceiveSocket");
	}

	if(bind(s, (struct sockaddr *)&sockname, sockname_len) < 0)
	{
		printf("Error:%s: bind(udp%s)\n",
		       "OpenAndConfSSDPReceiveSocket", ipv6 ? "6" : "");
		close(s);
		return -1;
	}

#ifdef ENABLE_IPV6
	if(ipv6)
	{
		AddMulticastMembershipIPv6(s);
	}
	else
#endif
	{
		if(AddMulticastMembership(s, lan_addr->addr.s_addr) < 0)
		{
			printf("Warning:Failed to add multicast membership for interface %s",
				       lan_addr->str ? lan_addr->str : "NULL");
		}
	}

	return s;
}

/* 
 *Function:OpenAndConfSSDPNotifySocket
 *Description:open the UDP socket used to send SSDP notifications to
 * the multicast group reserved for them 
 */
static int
OpenAndConfSSDPNotifySocket(in_addr_t addr)
{
	int s;
	unsigned char loopchar = 0;
	int bcast = 1;
	unsigned char ttl = 2; /* UDA v1.1 says :
		The TTL for the IP packet SHOULD default to 2 and
		SHOULD be configurable. */
	/* TODO: Make TTL be configurable */
	struct in_addr mc_if;
	struct sockaddr_in sockname;

	if( (s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("Error:socket(udp_notify)\n");
		return -1;
	}

	mc_if.s_addr = addr;	/*inet_addr(addr);*/

	if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopchar, sizeof(loopchar)) < 0)
	{
		printf("Error:setsockopt(udp_notify, IP_MULTICAST_LOOP)\n");
		close(s);
		return -1;
	}

	if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (char *)&mc_if, sizeof(mc_if)) < 0)
	{
		printf("Error:setsockopt(udp_notify, IP_MULTICAST_IF)\n");
		close(s);
		return -1;
	}

	if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
	{
		printf("Error:setsockopt(udp_notify, IP_MULTICAST_TTL,)\n");
	}

	if(setsockopt(s, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast)) < 0)
	{
		printf("Error:setsockopt(udp_notify, SO_BROADCAST)\n");
		close(s);
		return -1;
	}

	memset(&sockname, 0, sizeof(struct sockaddr_in));
    sockname.sin_family = AF_INET;
    sockname.sin_addr.s_addr = addr;	/*inet_addr(addr);*/

    if (bind(s, (struct sockaddr *)&sockname, sizeof(struct sockaddr_in)) < 0)
	{
		printf("Error:bind(udp_notify)\n");
		close(s);
		return -1;
    }

	return s;
}

#ifdef ENABLE_IPV6
/* open the UDP socket used to send SSDP notifications to
 * the multicast group reserved for them. IPv6 */
static int
OpenAndConfSSDPNotifySocketIPv6(unsigned int if_index)
{
	int s;
	unsigned int loop = 0;

	s = socket(PF_INET6, SOCK_DGRAM, 0);
	if(s < 0)
	{
		syslog(LOG_ERR, "socket(udp_notify IPv6): %m");
		return -1;
	}
	if(setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_index, sizeof(if_index)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp_notify IPv6, IPV6_MULTICAST_IF, %u): %m", if_index);
		close(s);
		return -1;
	}
	if(setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(udp_notify, IPV6_MULTICAST_LOOP): %m");
		close(s);
		return -1;
	}
	return s;
}
#endif

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
OpenAndConfSSDPNotifySockets(int *sockets)
{
	sockets[0] = OpenAndConfSSDPNotifySocket(lan_addr->addr.s_addr);
	if(sockets[0] < 0)
		goto error;
		
#ifdef ENABLE_IPV6
	sockets[1] = OpenAndConfSSDPNotifySocketIPv6(lan_addr->index);
	if (sockets[2] < 0)
		goto error;
		
#endif

	return 0;
error:

	if (sockets[0]>0)
	{
		close(sockets[0]);
		sockets[0] = -1;
	}
	return -1;
}

/* not really an SSDP "announce" as it is the response
 * to a SSDP "M-SEARCH"
 *
 * return: 0 succ
 *      -1 fail
 */
static int
SendSSDPAnnounce2(int s, const struct sockaddr * addr,
                  const char * st, const char * suffix, int ssl,
                  const char * host, unsigned short port, const char * path,
                  const char * uuid, unsigned int lifetime)
{
    int retval = 0;
	int l, n;
	char buf[512];
	char addr_str[64];
	socklen_t addrlen;

	l = snprintf(buf, sizeof(buf), "HTTP/1.1 200 OK\r\n"
		"Ext:\r\n"
		"Cache-Control: no-cache=\"Ext\", max-age=%u\r\n"
		"ST: %s%s\r\n"
		"USN: uuid:%s\r\n"
		"Location: %s://%s:%u%s\r\n"
		"\r\n",
		lifetime,
		st, suffix,
		uuid, ssl? "https":"http",
		host, (unsigned int)port, path);
	addrlen = (addr->sa_family == AF_INET6)
	          ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
	//printf("[%s]buf:%s", __FUNCTION__, buf);//test
	n = sendto(s, buf, l, 0,
	           addr, addrlen);
	sockaddr_to_string(addr, addr_str, sizeof(addr_str));
	//printf("INFO:SSDP Announce %d bytes to %s ST: %.*s",n,
     //  		addr_str,l, buf);//test
	if(n < 0)
	{
		/* XXX handle EINTR, EAGAIN, EWOULDBLOCK */
		printf("Error:[%s]sendto(udp) \n", __FUNCTION__ );
        retval = -1;
	}
    return retval;
}

//static char *nt_type[]={
//	"hnap:WECBSynced",
//	NULL
//};
/*
 * return: 0 succ
 *      -1 fail
 */
static int
SendSSDPNotifies(int s, const char * nt, const char * suffix, int ssl,
                 const char * host, unsigned short port, const char * path,
                 const char * uuid, unsigned int lifetime, int ipv6)
{
    int retval = -1;
#ifdef ENABLE_IPV6
	struct sockaddr_storage sockname;
#else
	struct sockaddr_in sockname;
#endif
	int l, n; //, i=0;
	char bufr[512];
	//char ver_str[4];

	memset(&sockname, 0, sizeof(sockname));
#ifdef ENABLE_IPV6
	if(ipv6) {
		struct sockaddr_in6 * p = (struct sockaddr_in6 *)&sockname;
		p->sin6_family = AF_INET6;
		p->sin6_port = htons(SSDP_PORT);
		inet_pton(AF_INET6, LL_SSDP_MCAST_ADDR, &(p->sin6_addr));
	} else
#endif
	{
		struct sockaddr_in *p = (struct sockaddr_in *)&sockname;
		p->sin_family = AF_INET;
		p->sin_port = htons(SSDP_PORT);
		p->sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDR);
	}

	do {
		l = snprintf(bufr, sizeof(bufr),
			"NOTIFY * HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"NT: hnap:%s%s\r\n"
			"NTS: ssdp:alive\r\n"
			"Location: %s://%s:%d%s\r\n"
			"USN: uuid:%s\r\n"
			"Cache-Control: max-age=%u\r\n"
			"\r\n",
			ipv6 ? "[" LL_SSDP_MCAST_ADDR "]" : SSDP_MCAST_ADDR,
			SSDP_PORT,
			nt,
			suffix,
			ssl? "https":"http",
			host, port, path,//LOCATION
			uuid,//USN
			lifetime);//NTS

			//printf("bufr:%s\n",bufr);//test
		if(l<0) {
			printf("ERROR %s() snprintf error\n", __FUNCTION__);
			break;
		}
		if((unsigned int)l >= sizeof(bufr)) {
			printf("WARNING (): truncated output\n", __FUNCTION__);
			l = sizeof(bufr);
		}
		n = sendto(s, bufr, l, 0,
			(struct sockaddr *)&sockname,
#ifdef ENABLE_IPV6
			ipv6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)
#else
			sizeof(struct sockaddr_in)
#endif
			);
		if(n < 0) {
			/* XXX handle EINTR, EAGAIN, EWOULDBLOCK */
			printf("ERROR sendto(udp_notify=%d, %s)", s,
			       host ? host : "NULL");
            break;
		}
		retval = 0;
	} while(0);
    return retval;
}


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
SendSSDPNotifies2(int * sockets,
                  const char * nt, const char * suffix, int ssl,
                  unsigned short port, const char * path,
                  const char * uuid,
                  unsigned int lifetime)
{
    int retval = -1;

    retval = SendSSDPNotifies(sockets[0], nt, suffix, ssl,lan_addr->str, port, path,
            uuid, lifetime, 0);
#ifdef ENABLE_IPV6
    retval &&= SendSSDPNotifies(sockets[1], nt, suffix, ssl, ipv6_addr_for_http_with_brackets, port, path,
            uuid, lifetime, 1);
#endif
    if( retval ) {
        retval = -1;
    }
    return retval;
}

/*
 * return 0: succ
        -1: fail
 */
int
ProcessSSDPData(int s, const char *bufr, int n,
                const struct sockaddr * sender, const char * nt, const char * suffix,
                int ssl, unsigned short port, const char * path,
                const char * uuid, unsigned int lifetime ) {
    int retval = -1;
	int i, l;
	const char * st = NULL;
	int st_len = 0;
	int st_ver = 0;
	char sender_str[64];
	//char ver_str[4];
	const char * announced_host = NULL;
#ifdef ENABLE_IPV6
#ifdef UPNP_STRICT
	char announced_host_buf[64];
#endif
#endif
	//printf("[%s]:bufr%s.\n", __FUNCTION__, bufr);//test
	/* get the string representation of the sender address */
	sockaddr_to_string(sender, sender_str, sizeof(sender_str));
	//printf("[%s]:sender_str:%s\n", __FUNCTION__, sender_str);//test
	
	if (memcmp(bufr, "NOTIFY", 6) == 0)
	{
		/* ignore NOTIFY packets. We could log the sender and device type */
		return retval;
	}
	else if(memcmp(bufr, "M-SEARCH", 8) == 0)
	{
		//printf("[%s]: M-SEARCH\n", __FUNCTION__ );//test
		i = 0;
		while(i < n)
		{
			while((i < n - 1) && (bufr[i] != '\r' || bufr[i+1] != '\n'))
				i++;
			i += 2;
			if((i < n - 3) && (strncasecmp(bufr+i, "st:", 3) == 0))
			{
				st = bufr+i+3;
				st_len = 0;
				while((*st == ' ' || *st == '\t') && (st < bufr + n))
					st++;
				while(st[st_len]!='\r' && st[st_len]!='\n'
				     && (st + st_len < bufr + n))
					st_len++;
				l = st_len;
				while(l > 0 && st[l-1] != ':')
					l--;
				st_ver = atoi(st+l);
				//printf("INFO:ST: %.*s (ver=%d)\n", st_len, st, st_ver);
			}
		}

		if(st && (st_len > 0)) {
			//printf("INFO:SSDP M-SEARCH from %s ST: %.*s",sender_str, st_len, st);
			/* find in which sub network the client is */
			if(sender->sa_family == AF_INET) {
				#if 0
				if( (((const struct sockaddr_in *)sender)->sin_addr.s_addr & lan_addr->mask.s_addr)
				   == (lan_addr->addr.s_addr & lan_addr->mask.s_addr))
				{
					//TODO
					//announced_host = lan_addr->str;
				}
				else
					return;
				#endif
			
				announced_host = lan_addr->str;
			}
#ifdef ENABLE_IPV6
			else
			{
				/* IPv6 address with brackets */
#ifdef UPNP_STRICT
				struct in6_addr addr6;
				size_t addr6_len = sizeof(addr6);
				/* retrieve the IPv6 address which
				 * will be used locally to reach sender */
				memset(&addr6, 0, sizeof(addr6));
				if(get_src_for_route_to (sender, &addr6, &addr6_len) < 0) {
					printf("WARNING:get_src_for_route_to() failed, using %s",
                            ipv6_addr_for_http_with_brackets);
					announced_host = ipv6_addr_for_http_with_brackets;
				} else {
					if(inet_ntop(AF_INET6, &addr6,
					             announced_host_buf+1,
					             sizeof(announced_host_buf) - 2)) {
						announced_host_buf[0] = '[';
						i = strlen(announced_host_buf);
						if(i < (int)sizeof(announced_host_buf) - 1) {
							announced_host_buf[i] = ']';
							announced_host_buf[i+1] = '\0';
						} else {
							printf("notice cannot suffix %s with ']'\n",
							       announced_host_buf);
						}
						announced_host = announced_host_buf;
					} else {
						printf("notice inet_ntop() failed\n");
						announced_host = ipv6_addr_for_http_with_brackets;
					}
				}
#else
				announced_host = ipv6_addr_for_http_with_brackets;
#endif
			}
#endif
			/* Responds to request with ST of 3 types:
             *   1 - ssdp:all
             *   2 - hnap:all
             *   3 - hnap:$DeviceType
             */
            char st_str[64] = {0};
            snprintf( st_str, sizeof(st_str), "hnap:%s", nt );
            if( (0 == strncmp(st, st_str, strlen(st_str) ))
                    || ((strlen("ssdp:all")==st_len) && (0== strncmp(st, "ssdp:all", st_len)))
                    || ((strlen("hnap:all")==st_len) && (0== strncmp(st, "hnap:all", st_len)))
              ) {
                printf( "Reponse to MSearch of type: %.*s\n", st_len, st );
                SendSSDPAnnounce2(s, sender,
                        st_str, suffix, ssl,
                        announced_host, port, path,
                        uuid, lifetime);
                retval = 0;
            } else {
                //printf( "Warning: Unknown MSearch type: %s\n", st );
                //printf( "Warning: Unknown MSearch type: %.*s\n", st_len, st );
            }
			/* Responds to request with UUID types:
             */
            // JBB_TODO
		} else {
			printf("LOG_INFO Invalid SSDP M-SEARCH from %s", sender_str);
		}
	} else {
		printf("Info:Unknown udp packet received from %s", sender_str);
	}
    return retval;
}

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
                    const char * uuid, unsigned int lifetime )
{
	int n;
	char bufr[1500];
	socklen_t len_r;
#ifdef ENABLE_IPV6
	struct sockaddr_storage sendername;
	len_r = sizeof(struct sockaddr_storage);
#else
	struct sockaddr_in sendername;
	len_r = sizeof(struct sockaddr_in);
#endif

	n = recvfrom(s, bufr, sizeof(bufr), 0,
	             (struct sockaddr *)&sendername, &len_r);
	if(n < 0)
	{
		/* EAGAIN, EWOULDBLOCK, EINTR : silently ignore (try again next time)
		 * other errors : log to LOG_ERR */
		if(errno != EAGAIN &&
		   errno != EWOULDBLOCK &&
		   errno != EINTR)
		{
			printf("Error:recvfrom(udp)\n");
		}
        /* CID 32246:  Missing return statement (MISSING RETURN) */
		return -1;
	}
	return ProcessSSDPData(s, bufr, n, (struct sockaddr *)&sendername, nt, suffix, ssl, port, path, uuid, lifetime );

}
