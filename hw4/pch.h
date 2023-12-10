// Anthony Noyes, Class of 2024, 7th Semester
#ifndef PCH_H
#define PCH_H


#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <iostream>
#include <Ws2tcpip.h>
#include <iomanip>
#define IP_HDR_SIZE 20 /* RFC 791 */
#define ICMP_HDR_SIZE 8 /* RFC 792 */
/* max payload size of an ICMP message originated in the program */
#define MAX_SIZE 65200
/* max size of an IP datagram */
#define MAX_ICMP_SIZE (MAX_SIZE + ICMP_HDR_SIZE)
/* the returned ICMP message will most likely include only 8 bytes
* of the original message plus the IP header (as per RFC 792); however,
* longer replies (e.g., 68 bytes) are possible */
#define MAX_REPLY_SIZE (IP_HDR_SIZE + ICMP_HDR_SIZE + MAX_ICMP_SIZE)
/* ICMP packet types */
#define ICMP_ECHO_REPLY 0
#define ICMP_DEST_UNREACH 3
#define ICMP_TTL_EXPIRED 11
#define ICMP_ECHO_REQUEST 8
#define MAX_DNS_LEN		512
#define DNS_PTR			12 /* IP -> name */
#define DNS_QUERY		(0 << 15) /* 0 = query; 1 = response */
#define DNS_RESPONSE	(1 << 15)
#define DNS_STDQUERY	(0 << 11) /* opcode - 4 bits */
#define DNS_AA			(1 << 10) /* authoritative answer */
#define DNS_TC			(1 << 9) /* truncated */
#define DNS_RD			(1 << 8) /* recursion desired */
#define DNS_RA			(1 << 7) /* recursion available */
#endif //PCH_H
