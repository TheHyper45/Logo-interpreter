#include <cstring>
#include "debug.hpp"
#include "memory_arena.hpp"

namespace logo {
	void Arena_Allocator::destroy() {
		for(std::size_t i = 0;i < arenas.length;i += 1) {
			delete[] arenas[i].buffer;
		}
		arenas.destroy();
	}

	bool Arena_Allocator::create_new_arena() {
		Arena arena{};
		arena.capacity = arena_size;
		arena.buffer = new(std::nothrow) char[arena.capacity];
		if(arena.buffer) {
			if(!arenas.push_back(arena)) {
				delete[] arena.buffer;
				return false;
			}
			return true;
		}
		return false;
	}

	char* Arena_Allocator::construct_string(std::size_t length) {
		auto* ptr = static_cast<char*>(allocate_memory(length + 1,alignof(char)));
		std::memset(ptr,0,length + 1);
		return ptr;
	}

	void* Arena_Allocator::allocate_memory(std::size_t size,std::size_t alignment) {
		if(arena_size == 0) arena_size = logo::megabytes(128);
		logo::assert(size <= arena_size && alignment >= 1);
		if(arenas.length == 0) {
			if(!create_new_arena()) {
				return nullptr;
			}
		}

		Arena* arena = &arenas[arenas.length - 1];
		std::size_t addr = static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(arena->buffer));
		std::size_t new_addr = ((addr + arena->head_index) + (alignment - 1)) & -alignment;

		std::size_t offset = ((arena->head_index + size) + (alignment - 1)) & -alignment;
		if(offset > arena->capacity) {
			if(!create_new_arena()) return nullptr;
			arena = &arenas[arenas.length - 1];
			addr = static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(arena->buffer));
			new_addr = ((addr + arena->head_index) + (alignment - 1)) & -alignment;
		}
		arena->head_index = offset;
		return reinterpret_cast<void*>(static_cast<std::uintptr_t>(new_addr));
	}
}
