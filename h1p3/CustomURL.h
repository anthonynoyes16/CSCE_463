// Anthony Noyes, Class of 2024, 7th Semester
#ifndef CUSTOMURL_H
#define CUSTOMURL_H

class CustomURL {
public:
	CustomURL() {	
		originalURL = static_cast<char*>(malloc(MAX_URL_LEN));
		//port = static_cast<char*>(malloc(3));
		//path = static_cast<char*>(malloc(2));
	}

	~CustomURL() {
		if (originalURL != nullptr) {
			free(originalURL);
		}
		if (port != nullptr) {
			free(port);
		}
		if (path != nullptr) {
			free(path);
		}
	}

	char* originalURL;
	char* host;
	char* fragment;
	char* query;
	char* path;
	char* port;
};

#endif // CUSTOMURL_H