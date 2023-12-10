// Anthony Noyes, Class of 2024, 7th Semester
//

#include "pch.h"
#pragma comment(lib, "ws2_32.lib") 

#pragma pack (push)
#pragma pack (1)
/* define the IP header (20 bytes) */
class IPHeader {
public:
	u_char h_len : 4; /* lower 4 bits: length of the header in dwords */
	u_char version : 4; /* upper 4 bits: version of IP, i.e., 4 */
	u_char tos; /* type of service (TOS), ignore */
	u_short len; /* length of packet */
	u_short ident; /* unique identifier */
	u_short flags; /* flags together with fragment offset - 16 bits */
	u_char ttl; /* time to live */
	u_char proto; /* protocol number (6=TCP, 17=UDP, etc.) */
	u_short checksum; /* IP header checksum */
	u_long source_ip;
	u_long dest_ip;
};
/* define the ICMP header (8 bytes) */
class ICMPHeader {
public:
	u_char type; /* ICMP packet type */
	u_char code; /* type subcode */
	u_short checksum; /* checksum of the ICMP */
	u_short id; /* application-specific ID */
	u_short seq; /* application-specific sequence */
};

struct DNSHeader {
	uint16_t ID;
	uint16_t flags;
	uint16_t nQuestions;
	uint16_t nAnswers;
	uint16_t nAuthority;
	uint16_t nAdditional;
};

struct QueryHeader {
	u_short qType;
	u_short qClass;
};

struct DNSanswerHdr {
	u_short type;
	u_short dClass;
	u_int ttl;
	u_short len;
};

#pragma pack(pop)

struct packetValues {							
	int ttl = 0;
	int sendCount = 0;
	double RTT = 0.0;
	bool echo = false;
	bool received = false;
	char routerName[512];
	u_long IP;
	LARGE_INTEGER send;
};

char* ReverseIP(char* host) {
	char* reversedAddr = new char[256];
	char* token = strtok(host, ".");
	char* reversedSections[4];
	int i = 0;

	while (token != nullptr && i < 4) {
		reversedSections[i] = token;
		token = strtok(nullptr, ".");
		i++;
	}

	reversedAddr[0] = '\0';
	for (int i = 3; i >= 0; i--) {
		strcat(reversedAddr, reversedSections[i]);
		if (i > 0) {
			strcat(reversedAddr, ".");
		}
	}
	strcat(reversedAddr, ".in-addr.arpa");
	return(reversedAddr);

}

int ParseJumpName(char* buf, int curPos, char* name, int responseSize) {
	// begin from buf[curPos], construct the string inside name[], and return the new curPos
	int jumpCount = 0;
	int originalPos = -1;
	int namePos = 0;

	while (buf[curPos] != 0x0) {
		if ((u_char)buf[curPos] >= 0xC0) {
			if (jumpCount == 0) {
				originalPos = curPos;
			}
			if (jumpCount > MAX_DNS_LEN) { // random6.irl
				std::cout << "  ++ invalid record: jump loop";
				return -1;
			}
			int offset = (u_char)(((buf[curPos] & 0x3F) << 8) | buf[curPos + 1]);
			if (curPos == responseSize - 1) { // random7.irl
				std::cout << "  ++ invalid record: truncated jump offset (e.g., 0xC0 and the packet ends)";
				return -1;
			}
			if (offset < sizeof(DNSHeader)) { // random0.irl
				std::cout << "  ++ invalid record: jump into fixed DNS header";
				return -1;
			}
			if (offset > responseSize) { // random5.irl
				std::cout << "  ++ invalid reply: jump beyond packet boundary";
				return -1;
			}
			curPos = offset;
			if ((u_char)buf[curPos] >= 0xC0) {
				jumpCount += 1;
				continue;
			}
			int extractedInt = buf[offset];
			for (int i = 0; i < extractedInt; i++) {
				if (buf[curPos + 1] == 0x0) { // random4.irl
					std::cout << "  ++ invalid record: truncated name (e.g., '6 goog' and the packet ends)";
					return -1;
				}
				name[namePos] = buf[curPos + 1];
				namePos += 1;
				curPos += 1;
			}
			curPos += 1;
			name[namePos] = '.';
			namePos += 1;
			jumpCount += 1;
		}
		else {
			int extractedInt = buf[curPos];
			for (int i = 0; i < extractedInt; i++) {
				name[namePos] = buf[curPos + 1];
				namePos += 1;
				curPos += 1;
			}
			curPos += 1;
			name[namePos] = '.';
			namePos += 1;
		}
	}
	name[namePos - 1] = '\0';
	if (jumpCount == 0) {
		return curPos + 1;
	}
	else {
		return originalPos + 2;
	}
}

void makeDNSquestion(char* buf, char* host) {
	int i = 0;
	while (strchr(host, '.') != nullptr) {
		char* location = strchr(host, '.');
		*location = '\0';
		int sectionSize = static_cast<int>(strlen(host));
		buf[i++] = static_cast<char>(sectionSize);
		memcpy(buf + i, host, sectionSize);
		i += sectionSize;
		host = location + 1;
	}
	int sectionSize = static_cast<int>(strlen(host));
	buf[i++] = static_cast<char>(sectionSize);
	memcpy(buf + i, host, sectionSize);
	i += sectionSize;
	buf[i] = '\0';
}

int lookup(packetValues* vals){
	packetValues* traceStats = vals;
	in_addr addr;
	addr.S_un.S_addr = traceStats->IP;
	char* host = inet_ntoa(addr);
	char packet[MAX_DNS_LEN];
	DNSHeader* dh = (DNSHeader*)packet;
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
	{
		printf("Socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}
	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(0);
	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		printf("Error binding socket %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	int pkt_size = 0;
	QueryHeader* qh;
	struct in_addr ipAddr;
	host = ReverseIP(host);
	pkt_size = strlen(host) + 2 + sizeof(DNSHeader) + sizeof(QueryHeader);
	qh = (QueryHeader*)(packet + pkt_size - sizeof(QueryHeader));
	qh->qType = htons(DNS_PTR);
	const char* IP = "8.8.8.8";
	char* dnsServerIP = (char*)IP;
	ipAddr.S_un.S_addr = inet_addr(dnsServerIP);
	qh->qClass = htons(0x01);
	dh->ID = htons(12345);
	dh->flags = htons(DNS_QUERY | DNS_RD | DNS_STDQUERY);
	dh->nQuestions = htons(1);
	dh->nAnswers = htons(0);
	dh->nAuthority = htons(0);
	dh->nAdditional = htons(0);

	struct sockaddr_in remote;
	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;
	remote.sin_addr = ipAddr;
	remote.sin_port = htons(53);

	makeDNSquestion((char*)(dh + 1), host);
	int sendAttempt = 0;
	char responseBuffer[MAX_DNS_LEN];
	int curPos = pkt_size;
	int responseSize = 0;
	while (sendAttempt < 3) {

		if (sendto(sock, packet, pkt_size, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
			printf("Error sending packet %d\n", WSAGetLastError());
			closesocket(sock);
			WSACleanup();
			return -1;
		}

		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		int selectResult = select(0, &fds, nullptr, nullptr, &timeout);

		if (selectResult == SOCKET_ERROR) {
			std::cerr << "Error in select. Error code: " << WSAGetLastError() << std::endl;
			closesocket(sock);
			WSACleanup();
			return -1;
		}

		if (selectResult == 0) {
			sendAttempt++;
			if (sendAttempt == 3) {
				closesocket(sock);
				WSACleanup();
				return -1;
			}
			continue;
		}
		struct sockaddr_in response;
		socklen_t len = sizeof(struct sockaddr_storage);
		responseSize = recvfrom(sock, responseBuffer, sizeof(responseBuffer), 0, (struct sockaddr*)&response, &len);
		if (responseSize == SOCKET_ERROR) {
			closesocket(sock);
			WSACleanup();
			return -1;
		}
		else {
			if (response.sin_addr.s_addr != remote.sin_addr.s_addr || response.sin_port != remote.sin_port) {
				closesocket(sock);
				WSACleanup();
				return -1;
			}

			if (responseSize < sizeof(DNSHeader)) { // random3.irl
				closesocket(sock);
				WSACleanup();
				return -1;
			}
			break;
		}
	}

	DNSHeader* ans = (DNSHeader*)responseBuffer;
	curPos = sizeof(DNSHeader);
	for (int i = 0; i < htons(ans->nQuestions); i++) {
		char name[MAX_DNS_LEN];
		curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
		if (curPos == -1) {
			closesocket(sock);
			WSACleanup();
			return -1;
		}
		QueryHeader* hdr = (QueryHeader*)(responseBuffer + curPos);
		curPos += sizeof(QueryHeader);
	}
	if (htons(ans->nAnswers) == 0x0) {
		memcpy(traceStats->routerName, "<no DNS entry>", 15);
	}

	for (int i = 0; i < 1; i++) {
		char name[MAX_DNS_LEN];
		curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
		if (curPos == -1) {
			closesocket(sock);
			WSACleanup();
			return -1;
		}
		DNSanswerHdr* hdr = (DNSanswerHdr*)(responseBuffer + curPos);
		curPos += sizeof(DNSanswerHdr);
		if (htons(hdr->type) == 0x1) { // A
			// type A answers have the 4 byte IP then there could be more answers
			uint8_t byte1 = (uint8_t)responseBuffer[curPos];
			uint8_t byte2 = (uint8_t)responseBuffer[curPos + 1];
			uint8_t byte3 = (uint8_t)responseBuffer[curPos + 2];
			uint8_t byte4 = (uint8_t)responseBuffer[curPos + 3];
			sprintf(name, "%u.%u.%u.%u", byte1, byte2, byte3, byte4);
			memcpy(traceStats->routerName, name, 512);
		}
		else if (htons(hdr->type) == 0x2) { // NS
			// type NS is just string format as before (could be compressed)
			curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
			if (curPos == -1) {
				closesocket(sock);
				WSACleanup();
				return -1;
			}
			memcpy(traceStats->routerName, name, 512);
		}
		else if (htons(hdr->type) == 0x5) { // CNAME
			// type CNAME is just string format as before (could be compressed)
			curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
			if (curPos == -1) {
				closesocket(sock);
				WSACleanup();
				return -1;
			}
			memcpy(traceStats->routerName, name, 512);
		}
		else if (htons(hdr->type) == 0xC) { // PTR
			// type PTR is just string format as before (could be compressed)
			curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
			if (curPos == -1) {
				closesocket(sock);
				WSACleanup();
				return -1;
			}
			memcpy(traceStats->routerName, name, 512);
		}
		
	}
	return 0;
}

class runner {
public:
	SOCKET sock;
	int ttl;
	char* targetHost;
	packetValues packets[30];
	int curHops;

	runner() {
		sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		ttl = -1;
		curHops = 30;
	}

	u_short ip_checksum(u_short* buffer, int size)
	{
		u_long cksum = 0;

		/* sum all the words together, adding the final byte if size is odd */
		while (size > 1)
		{
			cksum += *buffer++;
			size -= sizeof(u_short);
		}
		if (size) 
			cksum += *(u_char*)buffer;

		/* add carry bits to lower u_short word */
		cksum = (cksum >> 16) + (cksum & 0xffff);

		/* return a bitwise complement of the resulting mishmash */
		return (u_short)(~cksum);
	}

	int send() {
		struct sockaddr_in remote;
		memset(&remote, 0, sizeof(remote));
		remote.sin_family = AF_INET;
		DWORD IP = inet_addr(targetHost);
		if (IP == INADDR_NONE)
		{
			struct hostent* server;
			// if not a valid IP, then do a DNS lookup
			if ((server = gethostbyname(targetHost)) == NULL)
			{
				printf("Invalid string: neither FQDN, nor IP address\n");
				return -1;
			}
			else {
				// take the IP address and copy into sin_addr
				memcpy((char*)&(remote.sin_addr), server->h_addr, server->h_length);
			}
		}
		else
		{
			// if a valid IP, directly drop its binary version into sin_addr
			remote.sin_addr.S_un.S_addr = IP;
		}
		
		// buffer for the ICMP header
		u_char send_buf[MAX_ICMP_SIZE]; /* IP header is not present here */
		ICMPHeader* icmp = (ICMPHeader*)send_buf;
		// set up the echo request
		// no need to flip the byte order since fields are 1 byte each
		icmp->type = ICMP_ECHO_REQUEST;
		icmp->code = 0;
		// set up ID/SEQ fields as needed
		icmp->id = (u_short)GetCurrentProcessId();
		icmp->seq = ttl;
		// initialize checksum to zero
		icmp->checksum = 0;
		/* calculate the checksum */
		int packet_size = sizeof(ICMPHeader); // 8 bytes
		icmp->checksum = ip_checksum((u_short*)send_buf, packet_size);
		// need Ws2tcpip.h for IP_TTL, which is equal to 4; there is another constant with the same
		// name in multicast headers – do not use it!
		if (setsockopt(sock, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl)) == SOCKET_ERROR) {
			printf("setsockopt failedwith% d\n", WSAGetLastError());
			closesocket(sock);
			// some cleanup
			return -1;
		}
		// use regular sendto on the above socket
		if (sendto(sock, (char*)icmp, packet_size, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
			printf("sendto failed with %d\n", WSAGetLastError());
			return -1;
		}

		QueryPerformanceCounter(&packets[ttl].send);
		packets[ttl].ttl = ttl;
		packets[ttl].sendCount += 1;
	}

	int receive() {
		u_char rec_buf[MAX_REPLY_SIZE]; /* this buffer starts with an IP header */
		IPHeader* router_ip_hdr = (IPHeader*)rec_buf;
		ICMPHeader* router_icmp_hdr = (ICMPHeader*)(router_ip_hdr + 1);
		IPHeader* orig_ip_hdr = (IPHeader*)(router_icmp_hdr + 1);
		ICMPHeader* orig_icmp_hdr = (ICMPHeader*)(orig_ip_hdr + 1);

		LARGE_INTEGER start, end, total, freq;
		DWORD maxRTO = 0;
		bool done = false;
		// Get the frequency of the performance counter
		QueryPerformanceFrequency(&freq);

		struct timeval timeout;

		// Record the starting time
		QueryPerformanceCounter(&start);
		QueryPerformanceCounter(&end); // to initialize it
		bool first = true;
		int counter = 0;
		while (done == false) {
			if (counter == 5) {
				break;
			}
			counter++;
			double elapsedMicroseconds = static_cast<double>(end.QuadPart - start.QuadPart) * 100000 / freq.QuadPart + maxRTO / 1000;
			if (first) {
				timeout.tv_sec = 0;
				timeout.tv_usec = 500000; // start timeout at 500 ms
				first = false;
			}
			// Calculate the elapsed time in microseconds
			else if (elapsedMicroseconds >= 1000000) {
				timeout.tv_sec = static_cast<time_t>(elapsedMicroseconds / 1000000);
				timeout.tv_usec = static_cast<long>((elapsedMicroseconds - (static_cast<double>(timeout.tv_sec) * 1000000.0)));
			}
			else {
				// If prevRTO is less than 1 second, set the timeout to microseconds
				timeout.tv_sec = 0;
				timeout.tv_usec = static_cast<long>(elapsedMicroseconds);
			}
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(sock, &fds);
			int selectResult = select(sock + 1, &fds, nullptr, nullptr, &timeout);

			if (selectResult == -1) {
				std::cout << "Error in select. Error code: " << WSAGetLastError() << std::endl;
				closesocket(sock);
				WSACleanup();
				return -1;
			}

			if (selectResult == 0) {
				// socket timeout, no response
				bool retransmit = false;
				for (int i = 1; i < curHops; i++) {
					DWORD RTO = 500; // start at 500 ms
					if (!packets[i].received && packets[i].sendCount < 3) {
						double prev_rtt = 0.0;
						double next_rtt = 0.0;
						if (i != 1 && packets[i-1].received == true) {
							prev_rtt = packets[i - 1].RTT;
						}
						if (i != 30 && packets[i + 1].received == true) {
							next_rtt = packets[i + 1].RTT;
						}
						
						double average_rtt = (prev_rtt + next_rtt) / 2;
						double adjusted_rto = 2 * average_rtt;
						packets[i].RTT = adjusted_rto;
						RTO = (DWORD)packets[i].RTT;
						maxRTO = max(maxRTO, RTO);
						ttl = i;
						send();
						retransmit = true;
					}
				}
				if (retransmit == false) {
					done = true;
				}
				QueryPerformanceCounter(&end);
			}

			if (recvfrom(sock, (char*)&rec_buf, MAX_REPLY_SIZE, 0, NULL, NULL) != -1) {
				router_ip_hdr = (IPHeader*)rec_buf;
				router_icmp_hdr = (ICMPHeader*)(router_ip_hdr + 1);
				orig_ip_hdr = (IPHeader*)(router_icmp_hdr + 1);
				orig_icmp_hdr = (ICMPHeader*)(orig_ip_hdr + 1);
				if (router_icmp_hdr->type == ICMP_TTL_EXPIRED && router_icmp_hdr->code == 0) { // check if received packet is ours + contains correct info
					if (orig_ip_hdr->proto == IPPROTO_ICMP) {
						if (orig_icmp_hdr->id == (u_short)GetCurrentProcessId()) { // if id matches
							int ttl = orig_icmp_hdr->seq; // ttl of the packet we received
							QueryPerformanceCounter(&end);
							total.QuadPart = ((end.QuadPart - packets[ttl].send.QuadPart) * 100000) / freq.QuadPart;
							packets[ttl].RTT = total.QuadPart / 1000;
							packets[ttl].IP = (router_ip_hdr->source_ip);
							lookup(&packets[ttl]);
							packets[ttl].received = true;
						}
					}
				}
				if (router_icmp_hdr->type == ICMP_ECHO_REPLY && router_icmp_hdr->code == 0) { // check if received packet is ours + contains correct info
					if (router_ip_hdr->proto == IPPROTO_ICMP) {
						if (router_icmp_hdr->id == (u_short)GetCurrentProcessId()) { // if id matches
							int ttl = router_icmp_hdr->seq; // ttl of the packet we received
							QueryPerformanceCounter(&end);
							total.QuadPart = ((end.QuadPart - packets[ttl].send.QuadPart) * 100000) / freq.QuadPart;
							packets[ttl].RTT = total.QuadPart / 1000;
							packets[ttl].IP = (router_ip_hdr->source_ip);
							lookup(&packets[ttl]);
							packets[ttl].received = true;
							packets[ttl].echo = true;
							curHops = ttl;
						}
					}
				}
			}
		}
	}
};

int main(int argc, char** argv) {
	if (argc != 2)
	{
		std::cout << "Usage: " << argv[0] << " <targetHost>" << std::endl;
		return 1;
	}

	char* targetHost = argv[1];
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cout << "Failed to initialize Winsock." << std::endl;
		WSACleanup();
		return 1;
	}

	runner run;
	run.targetHost = targetHost;
	std::cout << "Tracerouting to " << targetHost << "..." << std::endl;
	if (run.sock == INVALID_SOCKET)
	{
		printf("Unable to create a raw socket: error %d\n", WSAGetLastError());
		return -1;
	}
	for (int i = 1; i < 31; i++)
	{
		run.ttl = i;
		if (run.send() < 0)
		{
			closesocket(run.sock);
			return -1;
		}
	}

	int ret = run.receive();
	if (ret < 0)
	{
		closesocket(run.sock);
		return -1;
	}

	for (int i = 1; i <= 30; i++) {
		std::cout << run.packets[i].ttl << " ";
		if (run.packets[i].sendCount == 3) { // no response received
			std::cout << "*" << std::endl;
			continue;
		}
		std::cout << run.packets[i].routerName << " ";

		in_addr addr;
		addr.S_un.S_addr = run.packets[i].IP;
		std::cout << "(" << inet_ntoa(addr) << ") ";
		std::cout << std::fixed << std::setprecision(3) << run.packets[i].RTT << " ms (" << run.packets[i].sendCount << ") " << std::endl;
		if (run.packets[i].echo) { // last packet received in trace
			break;
		}
	}
	return 0;
}