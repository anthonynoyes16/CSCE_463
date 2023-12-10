// Anthony Noyes, Class of 2024, 7th Semester
#include "pch.h"	
#pragma comment(lib, "ws2_32.lib")

bool isValidURL(const std::string& url) {
	// Check if the URL starts with a valid scheme
	std::string validSchemes[] = { "http://"};
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

	CustomSocket customSocket;

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
	customSocket.sock = sock;

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
		else {
			// take the IP address and copy into sin_addr
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
		}
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
	if (connect(customSocket.sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
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
	if (send(customSocket.sock, sendBuf, request.size() + 1, 0) == SOCKET_ERROR) {
		printf("Sending error: %d\n", WSAGetLastError());
		return;
	}

	printf("      Loading... ");
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);
	if (!customSocket.Read(2000000)) {
		std::cout << "failed with exceeding max" << std::endl;
		return;
	}
	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	printf("done in %i ms with %d bytes\n", ElapsedMicroseconds, customSocket.curPos);

	// get the status code as an integer
	int digit1 = customSocket.buf[9] - '0';
	int digit2 = customSocket.buf[10] - '0';
	int digit3 = customSocket.buf[11] - '0';
	int statusCode = digit1 * 100 + digit2 * 10 + digit3;
	printf("      Verifying header... status code %i\n", statusCode);

	char* html_begin = strstr(customSocket.buf, "\r\n\r\n");
	if (html_begin == nullptr) {
		delete[] sendBuf;
		return;
	}
	html_begin += 4;
	// make sure status code is good before parsing
	if (digit1 == 2) {
		printf("    + Parsing page... ");
		QueryPerformanceFrequency(&Frequency);
		QueryPerformanceCounter(&StartingTime);
		HTMLParserBase* parser = new HTMLParserBase;
		int nLinks;
		char* linkBuffer = parser->Parse(html_begin, customSocket.curPos, original_url, (int)strlen(original_url), &nLinks);
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
		printf("----------------------------------------\n");
		// print out all of the header
		int offset = html_begin - customSocket.buf;
		for (int i = 0; i < offset; i++) {
			std::cout << customSocket.buf[i];
		}
	}

	// close the socket to this server; open again for the next one
	if (closesocket(sock) != 0) {
		std::cout << "Error closing socket: " << WSAGetLastError() << std::endl;
	}

	// call cleanup when done with everything and ready to exit program
	WSACleanup();
}




void web_crawl_pt2(std::set<DWORD>* uniqueIPs, char* original_url, char* str, char* path, char* host, char* port) {
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

	CustomSocket customSocket;

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
	customSocket.sock = sock;

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
		else {
			// take the IP address and copy into sin_addr
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
		}
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
	
	std::cout << "      Checking IP uniqueness... ";
	auto result = uniqueIPs->insert(inet_addr(inet_ntoa(server.sin_addr)));
	if (result.second == true) {
		// unique
		std::cout << "passed" << std::endl;
	}
	else {
		// duplicate
		std::cout << "failed" << std::endl;
		return;
	}

	std::cout << "      Connecting on robots... ";
	server.sin_family = AF_INET;
	int port_val = std::atoi(port);
	unsigned int port_num = static_cast<unsigned int>(port_val);
	server.sin_port = htons(port_num);		// host-to-network flips the byte order
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);
	if (connect(customSocket.sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		printf("Connection error: %d\n", WSAGetLastError());
		return;
	}

	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	printf("done in %i ms\n", ElapsedMicroseconds);

	std::string request = "HEAD /robots.txt HTTP/1.0\r\n"
		"Host: " + std::string(host) + "\r\n"
		"Connection: close\r\n"
		"User-Agent: AnthonyCrawler/1.0\r\n"
		"\r\n";
	char* sendBuf = new char[request.size() + 1]; // +1 for NULL
	sprintf_s(sendBuf, request.size() + 1, "%s", request.c_str());
	if (send(customSocket.sock, sendBuf, request.size() + 1, 0) == SOCKET_ERROR) {
		printf("Sending error: %d\n", WSAGetLastError());
		return;
	}

	printf("      Loading... ");
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);
	if (!customSocket.Read(16000)) {
		std::cout << "failed with exceeding max" << std::endl;
		return;
	}
	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	printf("done in %i ms with %d bytes\n", ElapsedMicroseconds, customSocket.curPos);

	int digit1 = customSocket.buf[9] - '0';
	int digit2 = customSocket.buf[10] - '0';
	int digit3 = customSocket.buf[11] - '0';
	int statusCode = digit1 * 100 + digit2 * 10 + digit3;
	printf("      Verifying header... status code %i\n", statusCode);
	if (digit1 != 4) {
		if (closesocket(sock) != 0) {
			std::cout << "Error closing socket: " << WSAGetLastError() << std::endl;
		}

		// call cleanup when done with everything and ready to exit program
		WSACleanup();
		return;
	}
	if (closesocket(sock) != 0) {
		std::cout << "Error closing socket: " << WSAGetLastError() << std::endl;
	}

	CustomSocket customSocketHTML;
	// open a TCP socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		printf("Socket() generated error %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}
	customSocketHTML.sock = sock;

	server.sin_family = AF_INET;
	port_val = std::atoi(port);
	port_num = static_cast<unsigned int>(port_val);
	server.sin_port = htons(port_num);		// host-to-network flips the byte order
	printf("    * Connecting on page... ");

	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);
	if (connect(customSocketHTML.sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
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
	request = "GET " + std::string(path) + " HTTP/1.0\r\n"
		"Host: " + std::string(host) + "\r\n"
		"Connection: close\r\n"
		"User-Agent: AnthonyCrawler/1.0\r\n"
		"\r\n";
	sendBuf = new char[request.size() + 1]; // +1 for NULL
	sprintf_s(sendBuf, request.size() + 1, "%s", request.c_str());
	if (send(customSocketHTML.sock, sendBuf, request.size() + 1, 0) == SOCKET_ERROR) {
		printf("Sending error: %d\n", WSAGetLastError());
		return;
	}

	printf("      Loading... ");
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);
	if (!customSocketHTML.Read(2000000)) {
		std::cout << "failed with exceeding max" << std::endl;
		return;
	}
	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	printf("done in %i ms with %d bytes\n", ElapsedMicroseconds, customSocketHTML.curPos);

	// get the status code as an integer
	digit1 = customSocketHTML.buf[9] - '0';
	digit2 = customSocketHTML.buf[10] - '0';
	digit3 = customSocketHTML.buf[11] - '0';
	statusCode = digit1 * 100 + digit2 * 10 + digit3;
	printf("      Verifying header... status code %i\n", statusCode);

	char* html_begin = strstr(customSocketHTML.buf, "\r\n\r\n") + 4;
	// make sure status code is good before parsing
	if (digit1 == 2) {
		printf("    + Parsing page... ");
		QueryPerformanceFrequency(&Frequency);
		QueryPerformanceCounter(&StartingTime);
		HTMLParserBase* parser = new HTMLParserBase;
		int nLinks;
		char* linkBuffer = parser->Parse(html_begin, customSocketHTML.curPos, original_url, (int)strlen(original_url), &nLinks);
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
		*location = '\0';
		return location + 1;
	}
	return nullptr;
}

int handle_url(CustomURL* url, char* urlToParse) {
	if (strlen(urlToParse) > MAX_URL_LEN) {
		std::cout << "Ensure that the URL length is less than " << MAX_URL_LEN << std::endl;
		return 1;
	}

	// put the original supplied url into new char* for later use
	strcpy_s(url->originalURL, strlen(urlToParse) + 1, urlToParse);
	std::cout << "URL: ";
	print_pointer(url->originalURL);

	std::cout << "      Parsing URL... ";
	if (!isValidURL(url->originalURL)) {
		return 1;
	}

	url->host = url->originalURL + 7;
	url->fragment = find_occurance(url->host, '#');
	url->query = find_occurance(url->host, '?');
	url->path = find_occurance(url->host, '/');
	url->port = find_occurance(url->host, ':');
	url->host = url->originalURL + 7;

	if (url->port != nullptr and strlen(url->port) < 2) {
		std::cout << "failed with invalid port" << std::endl;
		return 1; 
	}

	if (url->port == nullptr) { // no port specified
		url->port = static_cast<char*>(malloc(3));
		url->port[0] = '8';
		url->port[1] = '0';
		url->port[2] = '\0';
	}
	if (url->path == nullptr) { // no path specified, use default /
		url->path = static_cast<char*>(malloc(2));
		url->path[0] = '/';
		url->path[1] = '\0';
	}
	if (url->path[0] != '/') {
		std::string str_path(url->path);
		str_path = "/" + str_path;

		// Allocate new memory for the modified path
		char* new_path = new char[str_path.length() + 1];

		// Copy the modified path using strcpy_s
		if (strcpy_s(new_path, str_path.length() + 1, str_path.c_str()) == 0) {
			// Update the path pointer to the new memory
			url->path = new_path;
		}
		else {
			std::cout << "Error adding '/' to beginning of path" << std::endl;
		}
	}

	std::cout << " host " << url->host << ", port " << url->port << ", request " << url->path;
	if (url->query != nullptr) {
		std::cout << '?' << url->query;
	}
	std::cout << std::endl;
	return 0;
}

int main(int argc, char* argv[])
{
	// check for invalid use: incorrect number of arguments
	if (argc != 2 and argc != 3) {
		std::cout << "Usage: h1p2.cpp 'your-url-here' OR h1p2.cpp 'number of threads' 'your-text-file'" << std::endl;
		return 1;
	}

	if (argc == 2) {
		CustomURL* url = new CustomURL;
		if (handle_url(url, argv[1])) {
			return 1;
		}
		std::cout << "      Doing DNS... ";
		web_crawl(url->originalURL, url->host, url->path, url->host, url->port);
		return 0;
	}
	
	else if (argc == 3) {
		int num_threads = std::stoi(argv[1]);
		if (num_threads != 1) {
			std::cout << "Usage: h1p2.cpp 'number of threads' 'your-text-file-path'" << std::endl;
			std::cout << "Ensure that the number of threads is equal to 1" << std::endl;
			return 1;
		}

		std::string filename = argv[2];
		std::ifstream file(filename);
		// Check if the file is open and readable
		if (file.is_open() && file.good()) {
			// Use seekg to move to the end of the file to get its size
			file.seekg(0, std::ios::end);
			std::streampos fileSize = file.tellg();
			std::cout << "Opened " << filename << " with size " << fileSize << std::endl;
			file.seekg(0, std::ios::beg);
			std::vector<CustomURL*> urls; // Vector to store urls to parse

			std::string url;
			std::set<std::string> uniqueHosts;
			std::set<DWORD> uniqueIPs;
			while (std::getline(file, url)) {
				CustomURL* urlToParse = new CustomURL;
				if (handle_url(urlToParse, const_cast<char*>(url.c_str()))) {
					continue;
				}
				urls.push_back(urlToParse);
				std::cout << "      Checking host uniqueness... ";
				std::string host = urlToParse->host;
				auto result = uniqueHosts.insert(host);
				if (result.second == true) {
					std::cout << "passed" << std::endl;
					// Perform DNS lookup here
					std::cout << "      Doing DNS... ";
					web_crawl_pt2(&uniqueIPs, urlToParse->originalURL, urlToParse->host, urlToParse->path, urlToParse->host, urlToParse->port);
				}
				else {
					std::cout << "failed" << std::endl;
				}
			}
		}
		else {
			std::cout << "Usage: h1p2.cpp 'number of threads' 'your-text-file-path'" << std::endl;
			std::cout << "Ensure that the file is in the correct location and readable" << std::endl;
			return 1;
		}
	}
	

	return 0;
}
