// Anthony Noyes, Class of 2024, 7th Semester
// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here

#define MAX_HOST_LEN		256
#define MAX_URL_LEN			2048
#define MAX_REQUEST_LEN		2048
#define INITIAL_BUF_SIZE	1024
#define THRESHOLD			1024

#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <time.h>
#include <string>
#include <fstream>
#include <vector>
#include <winsock.h>
#include <unordered_set>
#include <set>
#include "CustomSocket.h"
#include "CustomURL.h"
#include "HTMLParserBase.h"

#endif //PCH_H