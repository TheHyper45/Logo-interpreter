#include <climits>
#include "utils.hpp"
#include "debug.hpp"
#include "parser.hpp"
#include "heap_array.hpp"
#include "memory_arena.hpp"
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
	#define PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
	#undef near
	#undef far
#else
	#include <fcntl.h>
	#include <unistd.h>
	#include <sys/stat.h>
#endif

static_assert(CHAR_BIT == 8);

namespace logo {
	[[nodiscard]] static Option<Heap_Array<char>> read_file(String_View path) {
#ifdef PLATFORM_WINDOWS
		HANDLE file = CreateFileA(path.begin_ptr,GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if(file == INVALID_HANDLE_VALUE) {
			Report_Error("File \"%\" couldn't be opened.",path);
			return {};
		}
		defer[&]{CloseHandle(file);};

		std::size_t file_size = 0;
		{
			LARGE_INTEGER raw_file_size{};
			if(!GetFileSizeEx(file,&raw_file_size)) {
				Report_Error("Couldn't obtain the size of file \"%\".",path);
				return {};
			}
			file_size = static_cast<std::size_t>(raw_file_size.QuadPart);
			if(file_size > MAXDWORD) {
				Report_Error("File \"%\" is too big (max % bytes).",path,MAXDWORD);
				return {};
			}
		}

		Heap_Array<char> bytes{};
		if(!bytes.resize(file_size)) {
			Report_Error("Couldn't allocate % bytes of memory.",file_size);
			return {};
		}
		DWORD read_bytes{};
		if(!ReadFile(file,bytes.data,static_cast<DWORD>(file_size),&read_bytes,nullptr) || read_bytes != file_size) {
			bytes.destroy();
			Report_Error("Couldn't read data from file \"%\".",path);
			return {};
		}
		return bytes;
#else
		int file = open64(path.begin_ptr,O_RDONLY);
		if(file == -1) {
			Report_Error("File \"%\" couldn't be opened.",path);
			return {};
		}
		defer[&]{close(file);};

		struct stat64 file_stat{};
		if(fstat64(file,&file_stat) == -1) {
			Report_Error("Couldn't obtain the size of file \"%\".",path);
			return {};
		}

		Heap_Array<char> bytes{};
		if(!bytes.resize(file_stat.st_size)) {
			Report_Error("Couldn't allocate % bytes of memory.",static_cast<std::size_t>(file_stat.st_size));
			return {};
		}
		if(read(file,bytes.data,file_stat.st_size) != file_stat.st_size) {
			bytes.destroy();
			Report_Error("Couldn't read data from file \"%\".",path);
			return {};
		}
		return bytes;
#endif
	}
}

int main() {
	if(!logo::debug_init()) {
		logo::eprint("%\n",logo::get_reported_error());
		return 1;
	}
	defer[]{logo::debug_term();};

	auto [file_bytes,file_opened] = logo::read_file("./script0.txt");
	if(!file_opened) {
		logo::eprint("%\n",logo::get_reported_error());
		return 1;
	}
	defer[&]{file_bytes.destroy();};

	auto [parsing_result,parsing_successful] = logo::parse_input({file_bytes.data,file_bytes.length});
	if(!parsing_successful) {
		logo::eprint("%\n",logo::get_reported_error());
		return 1;
	}
	defer[&]{parsing_result.destroy();};
	return 0;
}
