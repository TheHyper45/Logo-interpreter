#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
	#define PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
	#undef near
	#undef far
#else
	#include <unistd.h>
#endif
#include "debug.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
	#include <intrin.h>
	#define LOGO_BREAKPOINT __debugbreak()
#else
	//TODO: Debug breakpoint for other compilers.
	#include <cstdlib>
	#define LOGO_BREAKPOINT std::abort()
#endif

namespace logo {
	static Array_String<2048> reported_error_message;

	bool debug_init() {
#ifdef PLATFORM_WINDOWS
		if(!SetConsoleOutputCP(CP_UTF8)) {
			Report_Error("Couldn't set console output code page to UTF-8.");
			return false;
		}
		if(!SetConsoleCP(CP_UTF8)) {
			Report_Error("Couldn't set console code page to UTF-8.");
			return false;
		}
		DWORD console_mode = 0;
		if(!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),&console_mode)) {
			Report_Error("Couldn't query console mode.");
			return false;
		}
		console_mode |= (ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
		if(!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),console_mode)) {
			Report_Error("Couldn't enable ANSI escape codes.");
			return false;
		}
#endif
		return true;
	}

	bool _print_stdout_char32_t(char32_t c) {
		Array_String<sizeof(c)> code_point{};
		code_point.append(c);
		String_View code = "\x1B[38;5;15m";
#ifdef PLATFORM_WINDOWS
		DWORD written_char_count = 0;
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE),code.begin_ptr,static_cast<DWORD>(code.length()),&written_char_count,nullptr);
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE),code_point.buffer,static_cast<DWORD>(code_point.byte_length),&written_char_count,nullptr);
#else
		write(STDOUT_FILENO,code.begin_ptr,code.length());
		write(STDOUT_FILENO,code_point.buffer,code_point.byte_length);
#endif
		return true;
	}

	bool _print_stderr_char32_t(char32_t c) {
		Array_String<sizeof(c)> code_point{};
		code_point.append(c);
		String_View code = "\x1B[38;5;9m";
#ifdef PLATFORM_WINDOWS
		DWORD written_char_count = 0;
		WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE),code.begin_ptr,static_cast<DWORD>(code.length()),&written_char_count,nullptr);
		WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE),code_point.buffer,static_cast<DWORD>(code_point.byte_length),&written_char_count,nullptr);
#else
		write(STDERR_FILENO,code.begin_ptr,code.length());
		write(STDERR_FILENO,code_point.buffer,code_point.byte_length);
#endif
		return true;
	}

	bool _write_char32_t_to_error_message(char32_t code_point) {
		return reported_error_message.append(code_point);
	}

	String_View get_reported_error() {
		return {reported_error_message.buffer,reported_error_message.byte_length};
	}

	void assert(bool condition,std::source_location loc) {
		if(condition) return;
		logo::eprint("******** Assertion failed at %:% ********\n",loc.file_name(),loc.line());
		LOGO_BREAKPOINT;
	}

	void unreachable(std::source_location loc) {
		logo::eprint("******** Unreachable block at %:% ********\n",loc.file_name(),loc.line());
		LOGO_BREAKPOINT;
	}
}
