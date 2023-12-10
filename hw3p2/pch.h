// Anthony Noyes, Class of 2024, 7th Semester

#ifndef PCH_H
#define PCH_H

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <winsock2.h>
#include <iomanip>
#include <chrono>
#include <ws2tcpip.h> 

#define FORWARD_PATH 0
#define RETURN_PATH 1

// possible status codes from ss.Open, ss.Send, ss.Close
#define STATUS_OK 0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED 2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME 3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND 4 // sendto() failed in kernel
#define TIMEOUT 5 // timeout after all retx attempts are exhausted
#define FAILED_RECV 6 // recvfrom() failed in kernel
// Define more error codes as needed

#define MAGIC_PORT						22345 // receiver listens on this port
#define MAX_PKT_SIZE					(1500 - 28) // maximum UDP packet size accepted by receiver
#define MAGIC_PROTOCOL					0x8311AA

#endif //PCH_H
