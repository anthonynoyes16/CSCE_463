// Anthony Noyes, Class of 2024, 7th Semester
#pragma comment(lib, "ws2_32.lib")
#include "pch.h"


bool isValidURL(const std::string& url) {
	// Check if the URL starts with a valid scheme
	std::string validSchemes[] = {"http://"};
	bool validScheme = false;

	for (const std::string& scheme : validSchemes) {
		if (url.find(scheme) == 0) {
			validScheme = true;
			break;
		}
	}

	if (!validScheme) {
		std::cout << "Invalid URL scheme" << std::endl;
		return false;
	}
	return true;
}

void print_pointer(char* pointer) {
	// function to print out whatever data is at the location pointer
	if (pointer != nullptr) {
		char temp[MAX_URL_LEN];
		sprintf_s(temp, "%s", pointer);
		puts(temp);
	}
	else {
		return;
	}
}


void web_crawl(char* original_url, char* str, char* path, char* host, char* port) {
	// main function for web crawling
	// str and host should be the same value, whatever the base url is i.e http://tamu.edu
	WSADATA wsaData;

	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}	

	// values used for timing of certain functions
	LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);

	// open a TCP socket
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		printf("Socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}

	// structure used in DNS lookups
	struct hostent* remote;

	// structure for connecting to server
	struct sockaddr_in server;

	// first assume that the string is an IP address
	DWORD IP = inet_addr(str);
	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(str)) == NULL)
		{
			printf("Invalid string: neither FQDN, nor IP address\n");
			return;
		}
		else // take the first IP address and copy into sin_addr
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
	}
	else
	{
		// if a valid IP, directly drop its binary version into sin_addr
		server.sin_addr.S_un.S_addr = IP;
	}

	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	printf("done in %i ms", ElapsedMicroseconds);
	printf(", found %s\n", inet_ntoa(server.sin_addr));

	// setup the port # and protocol type
	server.sin_family = AF_INET;
	int port_val = std::atoi(port);
	unsigned int port_num = static_cast<unsigned int>(port_val);
	server.sin_port = htons(port_num);		// host-to-network flips the byte order
	printf("    * Connecting on page... ");

	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);
	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		printf("Connection error: %d\n", WSAGetLastError());
		return;
	}
	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	printf("done in %i ms\n", ElapsedMicroseconds);
	

	// constructing GET request and inserting into a char* so it can be send by send()
	std::string request = "GET " + std::string(path) + " HTTP/1.0\r\n"
		"Host: " + std::string(host) + "\r\n"
		"Connection: close\r\n"
		"User-Agent: AnthonyCrawler/1.0\r\n"
		"\r\n";
	char* sendBuf = new char[request.size() + 1]; // +1 for NULL
	sprintf_s(sendBuf, request.size() + 1, "%s", request.c_str());
	if (send(sock, sendBuf, request.size() + 1, 0) == SOCKET_ERROR) {
		printf("Sending error: %d\n", WSAGetLastError());
		return;
	}

	printf("      Loading... ");
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);
	char* buffer;
	buffer = (char*)malloc(INITIAL_BUF_SIZE);
	int allocated_size = INITIAL_BUF_SIZE;
	int curPos = 0;
	int ret = 0;

	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(sock, &readSet);

	// Set up the timeout
	struct timeval timeout;
	timeout.tv_sec = 5;  // seconds
	timeout.tv_usec = 0; // microseconds

	while (true) {
		ret = select(0, &readSet, NULL, NULL, &timeout);
		if (ret > 0) {
			int bytesRead = recv(sock, buffer + curPos, allocated_size - curPos, 0);
			if (bytesRead == SOCKET_ERROR) {
				std::cout << "Error while receiving bytes: " <<  WSAGetLastError() << std::endl;
				break;
			}

			if (bytesRead == 0) {
				// Connection closed gracefully
				buffer[curPos] = '\0';  // NULL-terminate buffer
				break; // normal completion
			}

			curPos += bytesRead;
			if (allocated_size - curPos < THRESHOLD) {
				allocated_size *= 2;
				char* newBuffer = (char*)realloc(buffer, allocated_size);
				if (newBuffer == nullptr) {
					std::cerr << "Memory reallocation failed." << std::endl;
					free(buffer);
					break;
				}
				buffer = newBuffer;
			}
		}
		else if (ret == 0) {
			std::cout << "Timeout occurred." << std::endl;
		}
		else {
			std::cout << "Generic error while receiving bytes: " << WSAGetLastError() << std::endl;
		}
		
	}
	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	printf("done in %i ms with %d bytes\n", ElapsedMicroseconds, curPos);

	// get the status code as an integer
	int digit1 = buffer[9] - '0';
	int digit2 = buffer[10] - '0';
	int digit3 = buffer[11] - '0';
	int statusCode = digit1 * 100 + digit2 * 10 + digit3;
	printf("      Verifying header... status code %i\n", statusCode);

	char* html_begin = strstr(buffer, "\r\n\r\n") + 4;
	// make sure status code is good before parsing
	if (digit1 == 2) {
		printf("    + Parsing page... ");
		QueryPerformanceFrequency(&Frequency);
		QueryPerformanceCounter(&StartingTime);
		HTMLParserBase* parser = new HTMLParserBase;
		int nLinks;
		char* linkBuffer = parser->Parse(html_begin, curPos, original_url, (int)strlen(original_url), &nLinks);
		QueryPerformanceCounter(&EndingTime);
		ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
		ElapsedMicroseconds.QuadPart *= 1000;
		ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
		printf("done in %i ms with ", ElapsedMicroseconds);
		// check for errors indicated by negative values
		if (nLinks < 0) {
			nLinks = 0;
		}

		printf("%d links\n", nLinks);
	}
	
	printf("----------------------------------------\n");
	// print out all of the header
	int offset = html_begin - buffer;
	for (int i = 0; i < offset; i++) {
		std::cout << buffer[i];
	}
	// close the socket to this server; open again for the next one
	if (closesocket(sock) != 0) {
		std::cout << "Error closing socket: " << WSAGetLastError() << std::endl;
	}

	// call cleanup when done with everything and ready to exit program
	WSACleanup();
}



char* find_occurance(char* text, char character) {
	// function to find a character within a given char* and return a pointer to that location, or that location + 1 in some cases
    char* location = strchr(text, character);
    if (location != nullptr) {
        char* ret = new char[strlen(location) + 1];
        if (character == '#' or character == ':' or character == '?') {
            strcpy_s(ret, strlen(location), location + 1);
        }
        else {
            strcpy_s(ret, strlen(location) + 1, location);
        }
        *location = '\0';
        return ret;
    }
    return location;
}

int main(int argc, char* argv[]) 
{   
	// check for invalid use: either no url supplied, too many arguments supplied, or url too long
    if (argc != 2 or strlen(argv[1]) > MAX_URL_LEN) {
        std::cout << "Usage: h1p1.cpp 'your-url-here'" << std::endl;
        std::cout << "Ensure that the URL length is less than " << MAX_URL_LEN << std::endl;
        return 1;
    }

	// put the original supplied url into new char* for later use
    char* original_url = new char[strlen(argv[1]) + 1];
    strcpy_s(original_url, strlen(argv[1]) + 1, argv[1]);
	if (!isValidURL) {
		return 1;
	}

    char* fragment = find_occurance(argv[1], '#');
    char* query = find_occurance(argv[1], '?');
    char* path = find_occurance(argv[1] + 7, '/');
    char* port = find_occurance(argv[1] + 5, ':');
    char* temp_url = find_occurance(argv[1], '/');
    char* url = new char[strlen(temp_url) - 2];
    strcpy_s(url, strlen(temp_url) - 1, temp_url + 2);

    if (port == nullptr or strlen(port) < 2) { // no port specified or bad port, use default 80
        port = new char[3];
        port[0] = '8';
        port[1] = '0';
        port[2] = '\0';
    }
    if (path == nullptr) { // no path specified, use default /
        path = new char[2];
        path[0] = '/';
        path[1] = '\0';
    }
	/*std::cout << "Original URL: ";
	print_pointer(original_url);
	std::cout << "Fragment: ";
	print_pointer(fragment);
	std::cout << "Query: ";
	print_pointer(query);
	std::cout << "Path: ";
	print_pointer(path);
	std::cout << "Port: ";
	print_pointer(port);
	std::cout << "temp_url: ";
	print_pointer(temp_url);
	std::cout << "URL: ";
	print_pointer(url);*/

    std::cout << "URL: ";
    print_pointer(original_url);
	std::cout << "      Parsing URL..." << " host " << url << ", port " << port << ", request " << path;
	if (query != nullptr) {
		std::cout << '?' << query;
	}
	if (fragment != nullptr) {
		std::cout << '#' << fragment;
	}
	std::cout << std::endl;
	std::cout << "      Doing DNS... ";
	web_crawl(original_url, url, path, url, port);

    return 0;
}
