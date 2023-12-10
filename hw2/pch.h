// Anthony Noyes, Class of 2024, 7th Semester

#ifndef PCH_H
#define PCH_H

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <winsock2.h>
#include <ws2tcpip.h> 
#include <chrono>

#define MAX_DNS_LEN		512
/* DNS query types */
#define DNS_A			1 /* name -> IP */
#define DNS_NS			2 /* name server */
#define DNS_CNAME		5 /* canonical name */
#define DNS_PTR			12 /* IP -> name */
#define DNS_HINFO		13 /* host info/SOA */
#define DNS_MX			15 /* mail exchange */
#define DNS_AXFR		252 /* request for zone transfer */
#define DNS_ANY			255 /* all records */ 

#define DNS_OK			0 /* success */
#define DNS_FORMAT		1 /* format error (unable to interpret) */
#define DNS_SERVERFAIL	2 /* can’t find authority nameserver */
#define DNS_ERROR		3 /* no DNS entry */
#define DNS_NOTIMPL		4 /* not implemented */
#define DNS_REFUSED		5 /* server refused the query */

#define DNS_QUERY		(0 << 15) /* 0 = query; 1 = response */
#define DNS_RESPONSE	(1 << 15)
#define DNS_STDQUERY	(0 << 11) /* opcode - 4 bits */
#define DNS_AA			(1 << 10) /* authoritative answer */
#define DNS_TC			(1 << 9) /* truncated */
#define DNS_RD			(1 << 8) /* recursion desired */
#define DNS_RA			(1 << 7) /* recursion available */


#endif //PCH_H
