// Anthony Noyes, Class of 2024, 7th Semester
#include "pch.h"	
#pragma comment(lib, "ws2_32.lib")

bool endsWith(const char* buffer, const char* suffix) {
	size_t bufferLen = strlen(buffer);
	size_t suffixLen = strlen(suffix);

	if (bufferLen < suffixLen) {
		// The buffer is shorter than the suffix, so it can't end with the suffix.
		return false;
	}

	// Compare the last 'suffixLen' characters of the buffer with the suffix.
	return (strcmp(buffer + bufferLen - suffixLen, suffix) == 0);
}

bool isValidURL(const std::string& url) {
	// Check if the URL starts with a valid scheme
	std::string validSchemes[] = { "http://" };
	bool validScheme = false;

	for (const std::string& scheme : validSchemes) {
		if (url.find(scheme) == 0) {
			validScheme = true;
			break;
		}
	}

	if (!validScheme) {
		return false;
	}
	return true;
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

	return 0;
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

	char* html_begin = strstr(customSocket.buf, "\r\n\r\n") + 4;
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

void web_crawl_pt2(int* http2xx, int* http3xx, int* http4xx, int* http5xx, int* httpOther, HANDLE mutex, HTMLParserBase* parser, int* currentBytesRead, int* E, int* H, int* D, int* I, int* R, int* C, int* L, std::set<std::string>* uniqueHosts, std::set<DWORD>* uniqueIPs, char* original_url, char* str, char* path, char* host, char* port) {
	InterlockedIncrement(reinterpret_cast<volatile long*>(E));
	WaitForSingleObject(mutex, INFINITE); // lock mutex
	auto result = uniqueHosts->insert(host);
	ReleaseMutex(mutex); // unlock mutex
	if (result.second != true) {
		return;
	}

	InterlockedIncrement(reinterpret_cast<volatile long*>(H));

	// main function for web crawling
	WSADATA wsaData;

	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		//printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}

	CustomSocket customSocket;

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
			//printf("Invalid string: neither FQDN, nor IP address\n");
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
	InterlockedIncrement(reinterpret_cast<volatile long*>(D));
	WaitForSingleObject(mutex, INFINITE); // lock mutex
	auto IPresult = uniqueIPs->insert(inet_addr(inet_ntoa(server.sin_addr)));
	ReleaseMutex(mutex); // unlock mutex
	if (IPresult.second != true) {
		return;
	}
	InterlockedIncrement(reinterpret_cast<volatile long*>(I));

	server.sin_family = AF_INET;
	int port_val = std::atoi(port);
	unsigned int port_num = static_cast<unsigned int>(port_val);
	server.sin_port = htons(port_num);		// host-to-network flips the byte order
	
	if (connect(customSocket.sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		//printf("Connection error: %d\n", WSAGetLastError());
		return;
	}

	std::string request = "HEAD /robots.txt HTTP/1.0\r\n"
		"Host: " + std::string(host) + "\r\n"
		"Connection: close\r\n"
		"User-Agent: AnthonyCrawler/1.0\r\n"
		"\r\n";
	char* sendBuf = new char[request.size() + 1]; // +1 for NULL
	sprintf_s(sendBuf, request.size() + 1, "%s", request.c_str());
	if (send(customSocket.sock, sendBuf, request.size() + 1, 0) == SOCKET_ERROR) {
		//printf("Sending error: %d\n", WSAGetLastError());
		delete[] sendBuf;
		return;
	}

	if (!customSocket.Read(16000)) {
		//std::cout << "failed with exceeding max" << std::endl;
		delete[] sendBuf;
		return;
	}
	InterlockedAdd(reinterpret_cast<volatile long*>(currentBytesRead), customSocket.curPos);
	int digit1 = customSocket.buf[9] - '0';
	int digit2 = customSocket.buf[10] - '0';
	int digit3 = customSocket.buf[11] - '0';
	int statusCode = digit1 * 100 + digit2 * 10 + digit3;
	if (digit1 != 4) {
		if (closesocket(sock) != 0) {
			std::cout << "Error closing socket: " << WSAGetLastError() << std::endl;
		}
		delete[] sendBuf;
		// call cleanup when done with everything and ready to exit program
		WSACleanup();
		return;
	}
	if (closesocket(sock) != 0) {
		std::cout << "Error closing socket: " << WSAGetLastError() << std::endl;
		delete[] sendBuf;
	}
	InterlockedIncrement(reinterpret_cast<volatile long*>(R));

	CustomSocket customSocketHTML;
	// open a TCP socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		printf("Socket() generated error %d\n", WSAGetLastError());
		delete[] sendBuf;
		WSACleanup();
		return;
	}
	customSocketHTML.sock = sock;

	server.sin_family = AF_INET;
	port_val = std::atoi(port);
	port_num = static_cast<unsigned int>(port_val);
	server.sin_port = htons(port_num);		// host-to-network flips the byte order

	if (connect(customSocketHTML.sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		//printf("Connection error: %d\n", WSAGetLastError());
		delete[] sendBuf;
		return;
	}

	// constructing GET request and inserting into a char* so it can be send by send()
	request = "GET " + std::string(path) + " HTTP/1.0\r\n"
		"Host: " + std::string(host) + "\r\n"
		"Connection: close\r\n"
		"User-Agent: AnthonyCrawler/1.0\r\n"
		"\r\n";
	sendBuf = new char[request.size() + 1]; // +1 for NULL
	sprintf_s(sendBuf, request.size() + 1, "%s", request.c_str());
	if (send(customSocketHTML.sock, sendBuf, request.size() + 1, 0) == SOCKET_ERROR) {
		//printf("Sending error: %d\n", WSAGetLastError());
		delete[] sendBuf;
		return;
	}

	//printf("      Loading... ");
	if (!customSocketHTML.Read(2000000)) {
		//std::cout << "failed with exceeding max" << std::endl;
		delete[] sendBuf;
		return;
	}
	InterlockedAdd(reinterpret_cast<volatile long*>(currentBytesRead), customSocketHTML.curPos);

	// get the status code as an integer
	digit1 = customSocketHTML.buf[9] - '0';
	digit2 = customSocketHTML.buf[10] - '0';
	digit3 = customSocketHTML.buf[11] - '0';
	statusCode = digit1 * 100 + digit2 * 10 + digit3;

	InterlockedIncrement(reinterpret_cast<volatile long*>(C));
	if (digit1 == 2) {
		InterlockedIncrement(reinterpret_cast<volatile long*>(http2xx));
	}
	else if (digit1 == 3) {
		InterlockedIncrement(reinterpret_cast<volatile long*>(http3xx));
	}
	else if (digit1 == 4) {
		InterlockedIncrement(reinterpret_cast<volatile long*>(http4xx));
	}
	else if (digit1 == 5) {
		InterlockedIncrement(reinterpret_cast<volatile long*>(http5xx));
	}
	else {
		InterlockedIncrement(reinterpret_cast<volatile long*>(httpOther));
	}

	char* html_begin = strstr(customSocketHTML.buf, "\r\n\r\n");
	if (html_begin == nullptr) {
		delete[] sendBuf;
		return;
	}
	html_begin += 4;

	// make sure status code is good before parsing
	if (digit1 == 2) {

		int nLinks = 0;
		char* linkBuffer;
		linkBuffer = parser->Parse(html_begin, customSocketHTML.curPos, original_url, (int)strlen(original_url), &nLinks);

		//parser->Parse(html_begin, customSocketHTML.curPos, original_url, (int)strlen(original_url), &nLinks);
		// check for errors indicated by negative values
		if (nLinks < 0) {
			nLinks = 0;
		}
		InterlockedAdd(reinterpret_cast<volatile long*>(L), nLinks);
	}

	// close the socket to this server; open again for the next one
	if (closesocket(sock) != 0) {
		std::cout << "Error closing socket: " << WSAGetLastError() << std::endl;
	}
	delete[] sendBuf;
	// call cleanup when done with everything and ready to exit program
	WSACleanup();
}

class Crawler {
public:
	int numThreads;
	int E;
	int H;
	int D;
	int I;
	int R;
	int C;
	int L;
	HANDLE mutex;
	HANDLE finished;
	HANDLE eventQuit;
	std::set<DWORD>* uniqueIPs;
	std::set<std::string>* uniqueHosts;
	std::queue<CustomURL*> Queue;
	int previousRunSecond;
	int currentRunSecond;
	int currentBytesRead;
	int previousBytesRead;
	int previousPagesRead;
	int http2xx;
	int http3xx;
	int http4xx;
	int http5xx;
	int httpOther;

	Crawler() {
		numThreads = 0;
		E = 0;
		H = 0;
		D = 0;
		I = 0;
		R = 0;
		C = 0;
		L = 0;
		// create a mutex for accessing critical sections (including printf); initial state = not locked
		mutex = CreateMutex(NULL, 0, NULL);
		// create a semaphore that counts the number of active threads; initial value = 0, max = 1
		finished = CreateSemaphore(NULL, 0, 1, NULL);
		// create a quit event; manual reset, initial state = not signaled
		eventQuit = CreateEvent(NULL, true, false, NULL);
		uniqueIPs = new std::set<DWORD>();
		uniqueHosts = new std::set<std::string>();
		previousRunSecond = 0;
		currentRunSecond = 0;
		currentBytesRead = 0;
		previousBytesRead = 0;
		previousPagesRead = 0;
		http2xx = 0;
		http3xx = 0;
		http4xx = 0;
		http5xx = 0;
		httpOther = 0;
	}

	~Crawler() {
		// Release resources associated with the mutex, semaphore, and event
		CloseHandle(mutex);
		CloseHandle(finished);
		CloseHandle(eventQuit);

		// Release dynamically allocated memory
		delete uniqueIPs;
		delete uniqueHosts;
	}

	void StatsRun();
	void Run();
};

void Crawler::StatsRun() {
	while (WaitForSingleObject(eventQuit, 2000) == WAIT_TIMEOUT)
	{
		WaitForSingleObject(mutex, INFINITE); // lock mutex
		previousRunSecond = currentRunSecond;
		currentRunSecond += 2;

		printf("[%3d] %4d Q %7d E %7d H %6d D %6d I %5d R %5d C %5d L %4dK\n", currentRunSecond, numThreads, Queue.size(), E, H, D, I, R, C, L/1000);
		double crawlRate = static_cast<double>(L - previousPagesRead) / (currentRunSecond - previousRunSecond);
		double dataRate = (static_cast<double>(currentBytesRead - previousBytesRead) / 125000) / (currentRunSecond - previousRunSecond);
		printf("      *** crawling %.1f pps @ %.1f Mbps\n", crawlRate, dataRate);
		if (numThreads == 0) {
			std::cout << std::endl;
			printf("Extracted %d URLs @ %d/s\n", E, E / currentRunSecond);
			printf("Looked up %d DNS names @ %d/s\n", H, H / currentRunSecond);
			printf("Attempted %d site robots @ %d/s\n", I, I / currentRunSecond);
			printf("Crawled %d pages @ %d/s (%6.2f MB)\n", C, C / currentRunSecond, (currentBytesRead / 1000000.0));
			printf("Parsed %d links @ %d/s\n", L, L / currentRunSecond);
			printf("HTTP codes: 2xx = %d, 3xx = %d, 4xx = %d, 5xx = %d, other = %d\n", http2xx, http3xx, http4xx, http5xx, httpOther);
			ReleaseMutex(mutex);
			break;
		}
		previousBytesRead = currentBytesRead;
		previousPagesRead = L;
		ReleaseMutex(mutex); // unlock mutex
	}
}

void Crawler::Run() {
	HTMLParserBase* parser = new HTMLParserBase;
	while (true)
	{
		WaitForSingleObject(mutex, INFINITE); // lock mutex
		if (Queue.size() == 0)
		{
			ReleaseMutex(mutex);
			break;
		}
		CustomURL* urlToParse = Queue.front();
		Queue.pop();
		ReleaseMutex(mutex); // unlock mutex
		web_crawl_pt2(&http2xx, &http3xx, &http4xx, &http5xx, &httpOther, mutex, parser, &currentBytesRead, &E, &H, &D, &I, &R, &C, &L, uniqueHosts, uniqueIPs, urlToParse->originalURL, urlToParse->host, urlToParse->path, urlToParse->host, urlToParse->port);
	}
	numThreads -= 1;
	delete(parser);
}

UINT StatsRun(LPVOID pParam)
{
	Crawler* p = ((Crawler*)pParam);
	p->StatsRun();
	return 0;
}

UINT Run(LPVOID pParam)
{
	Crawler* p = ((Crawler*)pParam);
	p->Run();
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

		std::string filename = argv[2];
		std::ifstream file(filename);
		// Check if the file is open and readable
		if (file.is_open() && file.good()) {
			// Use seekg to move to the end of the file to get its size
			file.seekg(0, std::ios::end);
			std::streampos fileSize = file.tellg();
			std::cout << "Opened " << filename << " with size " << fileSize << std::endl;
			file.seekg(0, std::ios::beg);

			std::string url;
			Crawler crawler;
			while (std::getline(file, url)) {
				CustomURL* urlToParse = new CustomURL;
				if (handle_url(urlToParse, const_cast<char*>(url.c_str()))) {
					continue;
				}
				crawler.Queue.push(urlToParse);
			}
			file.close();
			HANDLE* handles = new HANDLE[num_threads + 1];
	 		handles[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)StatsRun, &crawler, 0, NULL); // start stat thread
			for (int i = 0; i < num_threads; i++) {
				crawler.numThreads += 1;
				handles[i+1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Run, &crawler, 0, NULL); // start run thread
			}
			for (int i = 0; i < num_threads; i++)
			{
				WaitForSingleObject(handles[i+1], INFINITE);
				CloseHandle(handles[i+1]);
			}
			WaitForSingleObject(handles[0], INFINITE);
			CloseHandle(handles[0]);

		}
		else {
			std::cout << "Usage: h1p3.cpp 'number of threads' 'your-text-file-path'" << std::endl;
			std::cout << "Ensure that the file is in the correct location and readable" << std::endl;
			return 1;
		}
	}

	return 0;
}
