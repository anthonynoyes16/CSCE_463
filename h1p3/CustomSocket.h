// Anthony Noyes, Class of 2024, 7th Semester
#pragma once

#ifndef CUSTOMSOCKET_H
#define CUSTOMSOCKET_H

class CustomSocket {
public:
    CustomSocket() :sock(INVALID_SOCKET), buf(nullptr), allocatedSize(INITIAL_BUF_SIZE), curPos(0) {
        buf = static_cast<char*>(malloc(allocatedSize));
    }

    ~CustomSocket() {
        if (buf != nullptr) {
            free(buf);
        }
    }

    bool Read(int max_download_size) {
        int ret = 0;
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);

        // Set up the timeout
        struct timeval timeout;
        timeout.tv_sec = 10;  // seconds
        timeout.tv_usec = 0; // microseconds

        while (true) {
            ret = select(0, &readSet, nullptr, nullptr, &timeout);
            if (ret > 0) {
                int bytesRead = recv(sock, buf + curPos, allocatedSize - curPos, 0);
                if (bytesRead == SOCKET_ERROR) {

                    // std::cout << "Error while receiving bytes: " << WSAGetLastError() << std::endl;
                    return false;
                }

                if (bytesRead == 0) {
                    // Connection closed gracefully
                    buf[curPos] = '\0';  // NULL-terminate buffer
                    return true; // normal completion
                }


                if (curPos + bytesRead > max_download_size) {
                    buf[curPos] = '\0';
                    return false;
                }

                curPos += bytesRead;
                if (allocatedSize - curPos < THRESHOLD) {
                    allocatedSize *= 2;
                    char* newBuffer = static_cast<char*>(realloc(buf, allocatedSize));
                    if (newBuffer == nullptr) {
                        //std::cerr << "Memory reallocation failed." << std::endl;
                        free(buf);
                        break;
                    }
                    buf = newBuffer;
                }
            }
            else if (ret == 0) {
                // std::cout << "Timeout occurred." << std::endl;
                return false;
            }
            else {
                //std::cout << "Generic error while receiving bytes: " << WSAGetLastError() << std::endl;
                return false;
            }
        }

        return false; // Returning false as a default case
    }

    SOCKET sock;          // socket handle
    char* buf;            // current buffer
    int allocatedSize;    // bytes allocated for buf
    int curPos;           // current position in buffer
};

#endif // CUSTOMSOCKET_H
