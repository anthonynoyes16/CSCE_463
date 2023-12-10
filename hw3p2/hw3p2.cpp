// Anthony Noyes, Class of 2024, 7th Semester

#include "pch.h"
#pragma comment(lib, "ws2_32.lib") 

class Checksum {
public:
    DWORD crc_table[256];
    Checksum()
    {
        // set up a lookup table for later use
        for (DWORD i = 0; i < 256; i++)
        {
            DWORD c = i;
            for (int j = 0; j < 8; j++) {
                c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
            }
            crc_table[i] = c;
        }
    }
    DWORD CRC32(unsigned char* buf, size_t len)
    {
        DWORD c = 0xFFFFFFFF;
        for (size_t i = 0; i < len; i++)
            c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFF;
    }
};

#pragma pack(push,1)
class Flags {
public:
    DWORD reserved : 5; // must be zero
    DWORD SYN : 1;
    DWORD ACK : 1;
    DWORD FIN : 1;
    DWORD magic : 24;
    Flags() { memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL; }
};


class SenderDataHeader {
public:
    Flags flags;
    DWORD seq; // must begin from 0
};

class LinkProperties {
public:
    // transfer parameters
    float RTT; // propagation RTT (in sec)
    float speed; // bottleneck bandwidth (in bits/sec)
    float pLoss[2]; // probability of loss in each direction
    DWORD bufferSize; // buffer size of emulated routers (in packets)
    LinkProperties() { memset(this, 0, sizeof(*this)); }
};

class ReceiverHeader {
public:
    Flags flags;
    DWORD recvWnd; // receiver window for flow control (in pkts)
    DWORD ackSeq; // ack value = next expected sequence
};

class SenderSynHeader {
public:
    SenderDataHeader sdh;
    LinkProperties lp;
};

class dataPacket {
public:
    SenderDataHeader sdh;
    char data[MAX_PKT_SIZE]; // Actual data payload
};

#pragma pack(pop)

class SenderSocket {
public:
    SOCKET sock;
    sockaddr_in serverAddress;
    int senderWindow;
    double rtt;
    double estimatedRTT;
    double devRTT;
    double timeoutInterval;
    double lossProbForward;
    double lossProbReverse;
    int bottleneckSpeed;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> openTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> sendTime;
    double prevRTO;
    struct hostent* host;
    struct sockaddr_in local;
    struct in_addr ipAddr;
    struct sockaddr_in remote;
    bool alreadyOpened;
    bool ackReceived = false;
    int curBytes;
    int seqNum;
    int receiverWindow;
    int timeoutCount;
    int retransmits;
    SenderSocket() {
        startTime = std::chrono::high_resolution_clock::now();
        openTime = std::chrono::high_resolution_clock::now();

        // Create a UDP socket
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET) {
            printf("Socket() generated error %d\n", WSAGetLastError());
        }

        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = INADDR_ANY;
        local.sin_port = htons(0);

        struct hostent* remote;
        DWORD IP = inet_addr("s3.irl.cs.tamu.edu");
        if (IP == INADDR_NONE)
        {
            // if not a valid IP, then do a DNS lookup
            if ((remote = gethostbyname("s3.irl.cs.tamu.edu")) == NULL)
            {
                printf("Invalid string: neither FQDN, nor IP address\n");
                return;
            }
            else {
                // take the IP address and copy into sin_addr
                memcpy((char*)&(serverAddress.sin_addr), remote->h_addr, remote->h_length);
            }
        }
        else
        {
            // if a valid IP, directly drop its binary version into sin_addr
            serverAddress.sin_addr.S_un.S_addr = IP;
        }

        // Set server's address and port
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(MAGIC_PORT);

        // Initialize other parameters
        senderWindow = 0; // Set to a suitable value
        rtt = 0.0; // Set to the RTT value
        lossProbForward = 0.0; // Set to the forward loss probability
        lossProbReverse = 0.0; // Set to the reverse loss probability
        bottleneckSpeed = 0; // Set to the bottleneck speed
        prevRTO = 1.0;
        host = NULL;
        alreadyOpened = false;
        estimatedRTT = 0.1;
        devRTT = 0;
        timeoutInterval = estimatedRTT + 4 * devRTT;
        curBytes = 0;
        seqNum = 0;
        receiverWindow = 0;
        timeoutCount = 0;
        retransmits = 0;
    }

    int Open(const char* targetHost, int port, int senderWindow, LinkProperties* lp) {
        openTime = std::chrono::high_resolution_clock::now();

        if (alreadyOpened) {
            return ALREADY_CONNECTED;
        }

        // Resolve the server's hostname to an IP address
        host = gethostbyname(targetHost);
        if (host == NULL) {
            //std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] --> target " << targetHost << " is invalid" << std::endl;
            closesocket(sock);
            return INVALID_NAME;
        }
        
        // bind to port 0
        if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
            printf("Error binding socket %d\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // set up target machine values
        ipAddr.S_un.S_addr = inet_addr(inet_ntoa(*((struct in_addr*)host->h_addr_list[0])));
        memset(&remote, 0, sizeof(remote));
        remote.sin_family = AF_INET;
        remote.sin_addr = ipAddr;
        remote.sin_port = htons(port);

        // create SenderSynHeader with correct values as specified
        Flags flags;
        flags.reserved = 0;
        flags.ACK = 0;
        flags.FIN = 0;
        flags.SYN = 1;
        SenderDataHeader sdh;
        sdh.flags = flags;
        sdh.seq = seqNum;
        SenderSynHeader ssh;
        ssh.sdh = sdh;
        lp->bufferSize = senderWindow + 3; // Buffer size = W + R
        ssh.lp = *lp;

        // send over UDP connection
        char connectPacket[sizeof(SenderSynHeader)];
        char responseBuffer[sizeof(ReceiverHeader)];
        memcpy(connectPacket, &ssh, sizeof(SenderSynHeader));

        for (int i = 1; i < 4; i++) {
            openTime = std::chrono::high_resolution_clock::now();
            // std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] --> SYN " << seqNum << " (attempt " << i << " of 3, RTO " << prevRTO << ") to " << inet_ntoa(*((struct in_addr*)host->h_addr_list[0])) << std::endl;
            sendTime = std::chrono::high_resolution_clock::now();
            if (sendto(sock, connectPacket, sizeof(SenderSynHeader), 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
                // std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] --> ";
                printf("failed Open sendto with %d\n", WSAGetLastError());
                closesocket(sock);
                WSACleanup();
                return FAILED_SEND;
            }

            struct timeval timeout;
            timeout.tv_sec = prevRTO;
            timeout.tv_usec = 0;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);

            int selectResult = select(0, &fds, nullptr, nullptr, &timeout);

            if (selectResult == SOCKET_ERROR) {
                std::cout << "Error in select. Error code: " << WSAGetLastError() << std::endl;
                closesocket(sock);
                WSACleanup();
                return -1;
            }

            if (selectResult == 0) {
                if (i != 3) {
                    openTime = std::chrono::high_resolution_clock::now();
                }
                else {
                    return TIMEOUT;
                }
                continue;
            }

            if (recvfrom(sock, responseBuffer, sizeof(responseBuffer), 0, NULL, NULL) != SOCKET_ERROR) {
                openTime = std::chrono::high_resolution_clock::now();
                prevRTO = 3 * std::chrono::duration<double>(openTime - sendTime).count();
                estimatedRTT = std::chrono::duration<double>(openTime - sendTime).count();
                //std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] <-- SYN-ACK " << rcv->ackSeq << " window " << rcv->recvWnd << "; setting initial RTO to " << std::fixed << std::setprecision(3) << prevRTO << std::endl;
                break;
            }
            else {
                openTime = std::chrono::high_resolution_clock::now();
                estimatedRTT = std::chrono::duration<double>(openTime - sendTime).count();
                //std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] <-- ";
                printf("failed recvfrom with %d\n", WSAGetLastError());
                return FAILED_RECV;
            }
            openTime = std::chrono::high_resolution_clock::now();
        }
        ReceiverHeader* rec = (ReceiverHeader*)responseBuffer;
        receiverWindow = rec->recvWnd;
        alreadyOpened = true;
        return STATUS_OK;
    }

    int Send(const char* data, int length) {
        if (!alreadyOpened) {
            return NOT_CONNECTED;
        }
        for (int i = 1; i < 101; i++) { // max attempts still 5 for part 2

            SenderDataHeader sdh;
            sdh.flags.ACK = 0;
            sdh.flags.FIN = 0;
            sdh.flags.SYN = 0;
            sdh.seq = seqNum;

            char* dataPacket = new char[sizeof(SenderDataHeader) + length];

            memcpy(dataPacket, &sdh, sizeof(SenderDataHeader));

            memcpy(dataPacket + sizeof(SenderDataHeader), data, length);
            std::chrono::time_point<std::chrono::high_resolution_clock> startTest = std::chrono::high_resolution_clock::now();
            if (sendto(sock, dataPacket, sizeof(SenderDataHeader) + length, 0, (sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
                printf("failed Send sendto with %d\n", WSAGetLastError());
                continue;
            }
            // ----- DATA SENT BY THIS POINT ----

            // ----- NOW RECEIVE ACK ------------
            struct timeval timeout;
            if (prevRTO >= 1.0) {
                timeout.tv_sec = static_cast<time_t>(prevRTO);
                timeout.tv_usec = static_cast<long>((prevRTO - static_cast<double>(timeout.tv_sec)) * 1000000.0);
            }
            else {
                // If prevRTO is less than 1 second, set the timeout to microseconds
                timeout.tv_sec = 0;
                timeout.tv_usec = static_cast<long>(prevRTO * 1000000.0);
            }
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            int selectResult = select(0, &fds, nullptr, nullptr, &timeout);

            if (selectResult == SOCKET_ERROR) {
                std::cout << "Error in select. Error code: " << WSAGetLastError() << std::endl;
                closesocket(sock);
                WSACleanup();
                return -1;
            }
            if (selectResult == 0) {
                timeoutCount += 1;
                if (i == 100) {
                    return TIMEOUT;
                }
                else {
                    continue;
                }
            }
            char buf[sizeof(ReceiverHeader)];
            int responseSize = 0;
            if (recvfrom(sock, buf, sizeof(ReceiverHeader), 0, NULL, NULL) < 0) {
                printf("failed recvfrom with %d\n", WSAGetLastError());
                continue;
            }
            struct ReceiverHeader* receivedPkt = (struct ReceiverHeader*)buf;
            if (receivedPkt->ackSeq == seqNum + 1 && receivedPkt->flags.ACK == 1) {
                seqNum += 1;
                curBytes += length;
            }
            //openTime = std::chrono::high_resolution_clock::now();
            std::chrono::time_point<std::chrono::high_resolution_clock> test = std::chrono::high_resolution_clock::now();
            auto time = test - startTest;
            std::chrono::seconds seconds = std::chrono::duration_cast<std::chrono::seconds>(time);
            time -= seconds;
            double milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
            double timeInSeconds = seconds.count() + (milliseconds / 1000.0);
            
            estimatedRTT = 0.875*estimatedRTT + 0.125*(timeInSeconds); // formula in slides
            devRTT = 0.75*devRTT + 0.25*abs(timeInSeconds - estimatedRTT); // formula in slides
            prevRTO = estimatedRTT + 4.0*max(devRTT, 0.01); // formula in pdf
            delete[] dataPacket;
            break;
        }
        return STATUS_OK;
    }


    int Close(int senderWindow, LinkProperties* lp) {
        openTime = std::chrono::high_resolution_clock::now();
        double timeSinceStart = std::chrono::duration<double>(openTime - startTime).count();
        if (!alreadyOpened) {
            // Handle the second Open call
            return NOT_CONNECTED;
        }

        // create SenderSynHeader with correct values as specified
        Flags flags;
        flags.reserved = 0;
        flags.ACK = 0;
        flags.FIN = 1;
        flags.SYN = 0;
        SenderDataHeader sdh;
        sdh.flags = flags;
        sdh.seq = seqNum;
        SenderSynHeader ssh;
        ssh.sdh = sdh;
        lp->bufferSize = senderWindow + 5; // Buffer size = W + R
        ssh.lp = *lp;
        // send over UDP connection
        char closePacket[sizeof(SenderSynHeader)];
        char responseBuffer[sizeof(ReceiverHeader)];
        memcpy(closePacket, &ssh, sizeof(SenderSynHeader));
        for (int i = 1; i < 6; i++) {
            openTime = std::chrono::high_resolution_clock::now();
            timeSinceStart = std::chrono::duration<double>(openTime - startTime).count();
            //std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] --> FIN " << sequence_number << " (attempt " << i << " of 5, RTO " << prevRTO << ")" << std::endl;
            if (sendto(sock, closePacket, sizeof(SenderSynHeader), 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
                //std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] --> ";
                printf("failed sendto with %d\n", WSAGetLastError());
                closesocket(sock);
                WSACleanup();
                return FAILED_SEND;
            }

            struct timeval timeout;
            if (prevRTO >= 1.0) {
                timeout.tv_sec = static_cast<time_t>(prevRTO);
                timeout.tv_usec = static_cast<long>((prevRTO - static_cast<double>(timeout.tv_sec)) * 1000000.0);
            }
            else {
                // If prevRTO is less than 1 second, set the timeout to microseconds
                timeout.tv_sec = 0;
                timeout.tv_usec = static_cast<long>(prevRTO * 1000000.0);
            }
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);

            int selectResult = select(0, &fds, nullptr, nullptr, &timeout);

            if (selectResult == SOCKET_ERROR) {
                std::cout << "Error in select. Error code: " << WSAGetLastError() << std::endl;
                closesocket(sock);
                WSACleanup();
                return -1;
            }

            if (selectResult == 0) {
                if (i != 5) {
                    openTime = std::chrono::high_resolution_clock::now();
                    timeSinceStart = std::chrono::duration<double>(openTime - startTime).count();
                }
                else {
                    std::cout << "timeout" << std::endl;
                    return TIMEOUT;
                }
                continue;
            }

            if (recvfrom(sock, responseBuffer, sizeof(responseBuffer), 0, NULL, NULL) != SOCKET_ERROR) {
                ReceiverHeader* rcv = (ReceiverHeader*)responseBuffer;
                openTime = std::chrono::high_resolution_clock::now();
                timeSinceStart = std::chrono::duration<double>(openTime - startTime).count();
                //std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] <-- FIN-ACK " << rcv->ackSeq << " window " << rcv->recvWnd << std::endl;
                break;
            }
            else {
                openTime = std::chrono::high_resolution_clock::now();
                timeSinceStart = std::chrono::duration<double>(openTime - startTime).count();
                //std::cout << "[" << std::fixed << std::setprecision(3) << timeSinceStart << "] <-- ";
                printf("failed recvfrom with %d\n", WSAGetLastError());
                return FAILED_RECV;
            }
            openTime = std::chrono::high_resolution_clock::now();
            timeSinceStart = std::chrono::duration<double>(openTime - startTime).count();
        }
        ReceiverHeader* rec = (ReceiverHeader*)responseBuffer;
        alreadyOpened = false;
        std::cout << "[" << std::fixed << std::setprecision(2) << timeSinceStart << "] <-- FIN-ACK " << rec->ackSeq << " window " << std::hex << rec->recvWnd << std::endl;
        return STATUS_OK;

    }

};

class statsThreadValues {
public:
    SenderSocket* threadSenderSocket;
    bool end;
};

UINT StatsRun(LPVOID pParam)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    statsThreadValues* statsThread = (statsThreadValues*)pParam;
    SenderSocket* statsSS = (SenderSocket*)statsThread->threadSenderSocket;

    std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
    int prevBase = 0;
    while (!statsThread->end) { // wait for main to signal to stop printing
        prevBase = statsSS->seqNum;
        Sleep(2000);
        double receiveSpeed = ((statsSS->seqNum - prevBase) * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader))) / 1000000.0;
        printf("[%3.0f] B %5d ( %.1f MB) N %5d T %2d F %d W %d S %0.3f Mbps RTT %.3f\n", std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - startTime).count(), statsSS->seqNum, statsSS->curBytes / 1000000.0, statsSS->seqNum + 1, statsSS->timeoutCount, statsSS->retransmits, min(statsSS->receiverWindow, statsSS->senderWindow), receiveSpeed, statsSS->estimatedRTT);
    }
    return 1;
}
int main(int argc, char** argv) {
    if (argc != 8) {
        std::cout << "Usage: " << argv[0] << " <targetHost> <buffer size> <sender window> <RTT> <loss prob forward> <loss prob reverse> <bottleneck speed>" << std::endl;
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "Failed to initialize Winsock." << std::endl;
        return 1;
    }

    char* targetHost = argv[1];
    int power = atoi(argv[2]);
    int senderWindow = atoi(argv[3]);
    double rtt = atof(argv[4]);
    double lossProbForward = atof(argv[5]);
    double lossProbReverse = atof(argv[6]);
    int bottleneckSpeed = atoi(argv[7]);

    std::cout << "Main:   sender W = " << senderWindow << ", RTT " << std::fixed << std::setprecision(3) << rtt << " sec, loss ";
    printf("%g", lossProbForward);
    std::cout << " / ";
    printf("%g", lossProbReverse);
    std::cout << ", link " << bottleneckSpeed << " Mbps" << std::endl;
    std::cout << "Main:   initializing DWORD array with 2^" << power << " elements... done in ";

    std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::steady_clock::now();
    
    UINT64 numDWORDs = (UINT64)1 << power;
    DWORD* dwordBuf = new DWORD[numDWORDs];
    for (UINT64 i = 0; i < numDWORDs; i++) {
        dwordBuf[i] = i;
    }
    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();
    std::cout << duration * 1000 << " ms" << std::endl;

    LinkProperties lp;
    lp.RTT = rtt;
    lp.speed = 1e6 * bottleneckSpeed; // convert to megabits
    lp.pLoss[FORWARD_PATH] = lossProbForward;
    lp.pLoss[RETURN_PATH] = lossProbReverse;

    SenderSocket ss;
    ss.senderWindow = senderWindow;
    start = std::chrono::steady_clock::now();
    int status = ss.Open(targetHost, MAGIC_PORT, senderWindow, &lp);
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration<double>(end - start).count();
    if (status != STATUS_OK) {
        std::cout << "Main:   connect failed with status " << status << std::endl;
        return 1;
    }
    std::cout << "Main:   connected to " << targetHost << " in " << std::fixed << std::setprecision(3) << duration << " sec, pkt size " << MAX_PKT_SIZE << " bytes" << std::endl;

    char* charBuf = (char*)dwordBuf;
    UINT64 byteBufferSize = numDWORDs << 2;
    UINT64 off = 0;
    struct statsThreadValues statsValues;
    statsValues.threadSenderSocket = &ss;
    statsValues.end = false;
    HANDLE handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)StatsRun, &statsValues, 0, NULL); // start stat thread
    std::chrono::time_point<std::chrono::high_resolution_clock> dataBegin = std::chrono::high_resolution_clock::now();
    while (off < byteBufferSize) {
        int bytes = static_cast<int>(byteBufferSize - off);
        if (bytes > static_cast<int>(MAX_PKT_SIZE - sizeof(SenderDataHeader))) {
            bytes = static_cast<int>(MAX_PKT_SIZE - sizeof(SenderDataHeader));
        }
        status = ss.Send(charBuf + off, bytes);
        if (status == TIMEOUT) { // reattempt transmission if timeout
            std::cout << "Error sending data: " << status << std::endl;
            continue;
        }
        off += bytes;
    }
    end = std::chrono::steady_clock::now();
    double rate = ((ss.curBytes * 8) / 1000.0) / std::chrono::duration<double>(end-dataBegin).count();
    statsValues.end = true;
    WaitForSingleObject(handle, INFINITE); // make sure main doesn't quit before stats thread
    CloseHandle(handle);
    duration = std::chrono::duration<double>(end - dataBegin).count();
    status = ss.Close(senderWindow, &lp);
    if (status != STATUS_OK) {
        std::cout << "Main:   close connection failed with status " << status << std::endl;
        return 1;
    }
    Checksum cs;
    DWORD check = cs.CRC32(reinterpret_cast<unsigned char*>(charBuf), byteBufferSize);
    std::cout << "Main:   transfer finished in " << std::fixed << std::setprecision(3) << duration << " sec, " << std::fixed << std::setprecision(2) << rate << " Kbps, checksum " << std::hex << check << std::endl;
    std::cout << "Main:   estRTT " << std::fixed << std::setprecision(3) << ss.estimatedRTT << ", ideal rate " << std::fixed << std::setprecision(2) << (senderWindow * 8 * MAX_PKT_SIZE / 1000.0) / ss.estimatedRTT << " Kbps" << std::endl;
    return 0;
}