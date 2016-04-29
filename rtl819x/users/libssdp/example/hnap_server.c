#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "hnap_ssdp.h"

int
main(int argc, char * * argv)
{
	int ret = -1;
	int sudp = -1;		/* IP v4 socket for receiving SSDP */
#ifdef ENABLE_IPV6
	int sudpv6 = -1;	/* IP v6 socket for receiving SSDP */
#endif

	int * snotify = NULL;

	fd_set readset;	/* for select() */
	fd_set writeset;
	struct timeval timeout = {0,0};
	int max_fd = -1;

	struct lan_addr_s br0;
	lan_addr = &br0;

	strcpy(lan_addr->ifname, "br0");
	strcpy(lan_addr->str,"192.168.59.162");
	lan_addr->addr.s_addr = inet_addr(lan_addr->str);
	lan_addr->mask.s_addr = inet_addr("255.255.255.0");
	
#ifndef ENABLE_IPV6
		snotify = calloc(1, sizeof(int));
#else
		/* one for IPv4, one for IPv6 */
		snotify = calloc(2, sizeof(int));
#endif

		/* open socket for SSDP connections */
		sudp = OpenAndConfSSDPReceiveSocket(0);
		if(sudp < 0)
		{
			printf("Error:Failed to open socket for receiving SSDP.\n");
			return -1;
		}
		
#ifdef ENABLE_IPV6
		sudpv6 = OpenAndConfSSDPReceiveSocket(1);
		if(sudpv6 < 0)
		{
			printf("Warning:Failed to open socket for receiving SSDP (IP v6).");
		}
#endif

		/* open socket for sending notifications */
		if(OpenAndConfSSDPNotifySockets(snotify) < 0)
		{
			printf("Error:Failed to open sockets for sending SSDP notify "
		                "messages. EXITING");
			goto shutdown;
			return -1;
		}

		//while (1)
		//{
		//	printf("test\n");
		SendSSDPNotifies2(snotify,80,1800);
		//	sleep(1);
		//}

	
	/* main loop */
	while(1)
	{
		/* select open sockets (SSDP, HTTP listen, and all HTTP soap sockets) */
		FD_ZERO(&readset);
		FD_ZERO(&writeset);

		if (sudp >= 0)
		{
			FD_SET(sudp, &readset);
			max_fd = MAX( max_fd, sudp);
		}
		
#ifdef ENABLE_IPV6
		if (sudpv6 >= 0)
		{
			FD_SET(sudpv6, &readset);
			max_fd = MAX( max_fd, sudpv6);
		}
#endif
		ret = select(max_fd+1, &readset, &writeset, 0, &timeout);
		if (ret< 0)
		{
			printf("select error\n");
			goto shutdown;
		}
		else if (ret == 0)
			continue;

		/* process SSDP packets */
		if(sudp >= 0 && FD_ISSET(sudp, &readset))
		{
			ProcessSSDPRequest(sudp, 80);
		}
#ifdef ENABLE_IPV6
		if(sudpv6 >= 0 && FD_ISSET(sudpv6, &readset))
		{
			printf("Received UDP Packet (IPv6)");
			ProcessSSDPRequest(sudpv6, 80);
		}
#endif
	}

shutdown:
	
/* close out open sockets */
	if (sudp >= 0) close(sudp);
#ifdef ENABLE_IPV6
	if (sudpv6 >= 0) close(sudpv6);
#endif

	free(snotify);
	return 0;
}