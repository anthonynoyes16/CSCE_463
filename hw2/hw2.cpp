// Anthony Noyes, Class of 2024, 7th Semester

#include "pch.h"

#pragma comment(lib, "ws2_32.lib") 

#pragma pack(push,1)

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

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <lookup_string> <DNS_server_IP>" << std::endl;
        WSACleanup();
        return 1;
    }

    char* host = argv[1];
    char* dnsServerIP = argv[2];

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
    if (inet_pton(AF_INET, host, &ipAddr) == 1) {
        std::cout << "Lookup    : " << host << std::endl;
        host = ReverseIP(host);
        pkt_size = strlen(host) + 2 + sizeof(DNSHeader) + sizeof(QueryHeader);
        qh = (QueryHeader*)(packet + pkt_size - sizeof(QueryHeader));
        qh->qType = htons(DNS_PTR);
    }

    else {
        std::cout << "Lookup    : " << host << std::endl;
        pkt_size = strlen(host) + 2 + sizeof(DNSHeader) + sizeof(QueryHeader);
        qh = (QueryHeader*)(packet + pkt_size - sizeof(QueryHeader));
        qh->qType = htons(DNS_A);
    }
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
    std::cout << "Query     : " << host << ", ";

    makeDNSquestion((char*)(dh + 1), host);
    printf("type %d, TXID 0x%.4x\n", htons(qh->qType), htons(dh->ID));
    printf("Server    : %s\n", inet_ntoa(ipAddr));
    std::cout << "********************************" << std::endl;
    int sendAttempt = 0;
    char responseBuffer[MAX_DNS_LEN];
    int curPos = pkt_size;
    int responseSize = 0;
    while (sendAttempt < 3) {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

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
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "Attempt " << sendAttempt << " with " << pkt_size << " bytes... timeout in " << duration.count() << " ms" << std::endl;
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
            std::cout << "Attempt " << sendAttempt << " with " << pkt_size << " bytes... socket error " << WSAGetLastError() << std::endl;
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        else {
            if (response.sin_addr.s_addr != remote.sin_addr.s_addr || response.sin_port != remote.sin_port) {
                std::cout << "Sent and received IP and port do not match" << std::endl;
                closesocket(sock);
                WSACleanup();
                return -1;
            }

            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            std::cout << "Attempt " << sendAttempt << " with " << pkt_size << " bytes... response in " << duration.count() << " ms with " << responseSize << " bytes." << std::endl;
            if (responseSize < sizeof(DNSHeader)) { // random3.irl
                closesocket(sock);
                WSACleanup();
                std::cout << "  ++ invalid reply: packet smaller than fixed DNS header";
                return -1;
            }
            break;
        }
    }

    DNSHeader* ans = (DNSHeader*)responseBuffer;
    printf("  TXID 0x%.4x, flags 0x%.4x, questions %d, answers %d, authority %d, additional %d\n", htons(ans->ID), htons(ans->flags), htons(ans->nQuestions), htons(ans->nAnswers), htons(ans->nAuthority), htons(ans->nAdditional));
    if (htons(ans->ID) != htons(dh->ID)) { // random9.irl
        printf("  ++ invalid reply: TXID mismatch, sent 0x%.4x, received 0x%.4x", htons(dh->ID), htons(ans->ID));
        return -1;
    }
    uint16_t rcode = htons(ans->flags) & 0x000F;
    int rcodeInt = static_cast<int>(rcode);
    if (rcodeInt != 0) {
        printf("  failed with Rcode = %d\n", rcodeInt);
        closesocket(sock);
        WSACleanup();
        return -1;
    }
    else {
        printf("  succeeded with Rcode = %d\n", rcodeInt);
    }

    std::cout << "  ------------ [questions] ----------" << std::endl;
    curPos = sizeof(DNSHeader);
    for (int i = 0; i < htons(ans->nQuestions); i++) {
        char name[MAX_DNS_LEN];
        curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
        if (curPos == -1) {
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        if (htons(qh->qType) == 0xC) {
            std::cout << "      ";
            ReverseIP(name);
            std::cout << name;
        }
        else {
            std::cout << "      " << name;
        }
        QueryHeader* hdr = (QueryHeader*)(responseBuffer + curPos);
        std::cout << " type " << htons(hdr->qType) << " class " << htons(hdr->qClass) << std::endl;
        curPos += sizeof(QueryHeader);
    }

    if (htons(ans->nAnswers) != 0x0) {
        if (htons(ans->nAnswers) > MAX_DNS_LEN) { // random1.irl
            std::cout << "  ++ invalid record: RR value length stretches the answer beyond packet";
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        std::cout << "  ------------ [answers] ------------ " << std::endl;
    }
    for (int i = 0; i < htons(ans->nAnswers); i++) {
        char name[MAX_DNS_LEN];
        if (curPos == responseSize) {
            std::cout << "  ++ invalid section: not enough records (e.g., declared 5 answers but only 3 found)";
            return -1;
        }
        if (curPos + sizeof(DNSanswerHdr) > responseSize) { // random4.irl
            std::cout << "  ++ invalid record: truncated RR answer header (i.e., don't have the full 10 bytes)";
            return -1;
        }
        curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
        if (curPos == -1) {
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        std::cout << "      " << name;
        DNSanswerHdr* hdr = (DNSanswerHdr*)(responseBuffer + curPos);
        curPos += sizeof(DNSanswerHdr);
        if (htons(hdr->type) == 0x1) { // A
            // type A answers have the 4 byte IP then there could be more answers
            std::cout << " A ";
            uint8_t byte1 = (uint8_t)responseBuffer[curPos];
            uint8_t byte2 = (uint8_t)responseBuffer[curPos + 1];
            uint8_t byte3 = (uint8_t)responseBuffer[curPos + 2];
            uint8_t byte4 = (uint8_t)responseBuffer[curPos + 3];
            printf("%d.%d.%d.%d", byte1, byte2, byte3, byte4);
            std::cout << " TTL = " << htonl(hdr->ttl) << std::endl;
            curPos += htons(hdr->len);
        }
        else if (htons(hdr->type) == 0x2) { // NS
            // type NS is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " NS " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
        else if (htons(hdr->type) == 0x5) { // CNAME
            // type CNAME is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " CNAME " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
        else if (htons(hdr->type) == 0xC) { // PTR
            // type PTR is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " PTR " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
    }

    if (htons(ans->nAuthority) != 0x0) {
        if (htons(ans->nAuthority) > MAX_DNS_LEN) { // random1.irl
            std::cout << "  ++ invalid record: RR value length stretches the answer beyond packet";
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        std::cout << "  ------------ [authority] ----------  " << std::endl;
    }
    for (int i = 0; i < htons(ans->nAuthority); i++) {
        char name[MAX_DNS_LEN];
        if (curPos == responseSize) {
            std::cout << "  ++ invalid section: not enough records (e.g., declared 5 answers but only 3 found)";
            return -1;
        }
        if (curPos + sizeof(DNSanswerHdr) > responseSize) { // random4.irl
            std::cout << "  ++ invalid record: truncated RR answer header (i.e., don't have the full 10 bytes)";
            return -1;
        }
        curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
        if (curPos == -1) {
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        std::cout << "      " << name;
        DNSanswerHdr* hdr = (DNSanswerHdr*)(responseBuffer + curPos);
        curPos += sizeof(DNSanswerHdr);
        if (htons(hdr->type) == 0x1) { // A
            // type A answers have the 4 byte IP then there could be more answers
            std::cout << " A ";
            uint8_t byte1 = (uint8_t)responseBuffer[curPos];
            uint8_t byte2 = (uint8_t)responseBuffer[curPos + 1];
            uint8_t byte3 = (uint8_t)responseBuffer[curPos + 2];
            uint8_t byte4 = (uint8_t)responseBuffer[curPos + 3];
            printf("%d.%d.%d.%d", byte1, byte2, byte3, byte4);
            std::cout << " TTL = " << htonl(hdr->ttl) << std::endl;
            curPos += htons(hdr->len);
        }
        else if (htons(hdr->type) == 0x2) { // NS
            // type NS is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " NS " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
        else if (htons(hdr->type) == 0x5) { // CNAME
            // type CNAME is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " CNAME " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
        else if (htons(hdr->type) == 0xC) { // PTR
            // type PTR is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " PTR " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
    }

    if (htons(ans->nAdditional) != 0x0) {
        if (htons(ans->nAdditional) > MAX_DNS_LEN) { // random1.irl
            std::cout << "  ++ invalid record: RR value length stretches the answer beyond packet";
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        std::cout << "  ------------ [additional] ---------  " << std::endl;
    }
    for (int i = 0; i < htons(ans->nAdditional); i++) {
        char name[MAX_DNS_LEN];
        if (curPos == responseSize) {
            std::cout << "  ++ invalid section: not enough records (e.g., declared 5 answers but only 3 found)";
            return -1;
        }
        if (curPos + sizeof(DNSanswerHdr) > responseSize) { // random4.irl
            std::cout << "  ++ invalid record: truncated RR answer header (i.e., don't have the full 10 bytes)";
            return -1;
        }
        curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
        if (curPos == -1) {
            closesocket(sock);
            WSACleanup();
            return -1;
        }
        std::cout << "      " << name;
        DNSanswerHdr* hdr = (DNSanswerHdr*)(responseBuffer + curPos);
        curPos += sizeof(DNSanswerHdr);
        if (htons(hdr->type) == 0x1) { // A
            // type A answers have the 4 byte IP then there could be more answers
            std::cout << " A ";
            uint8_t byte1 = (uint8_t)responseBuffer[curPos];
            uint8_t byte2 = (uint8_t)responseBuffer[curPos + 1];
            uint8_t byte3 = (uint8_t)responseBuffer[curPos + 2];
            uint8_t byte4 = (uint8_t)responseBuffer[curPos + 3];
            printf("%d.%d.%d.%d", byte1, byte2, byte3, byte4);
            std::cout << " TTL = " << htonl(hdr->ttl) << std::endl;
            curPos += htons(hdr->len);
        }
        else if (htons(hdr->type) == 0x2) { // NS
            // type NS is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " NS " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
        else if (htons(hdr->type) == 0x5) { // CNAME
            // type CNAME is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " CNAME " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
        else if (htons(hdr->type) == 0xC) { // PTR
            // type PTR is just string format as before (could be compressed)
            curPos = ParseJumpName(responseBuffer, curPos, name, responseSize);
            if (curPos == -1) {
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            std::cout << " PTR " << name << " TTL = " << htonl(hdr->ttl) << std::endl;
        }
    }

    WSACleanup();
    return 0;
}