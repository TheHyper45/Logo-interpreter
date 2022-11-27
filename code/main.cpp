#include <iostream>
#include <exception>
#include <Windows.h>
#include "utils.hpp"
#include "string.hpp"

int main() {
	if(!SetConsoleOutputCP(CP_UTF8)) {
		
	}
	if(!SetConsoleCP(CP_UTF8)) {
		
	}
	DWORD console_mode = 0;
	if(!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),&console_mode)) {
		
	}
	console_mode |= (ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
	if(!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),console_mode)) {
		
	}

	try {

	}
	catch(...) {

	}
	return 0;
}
