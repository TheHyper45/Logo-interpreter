#ifndef LOGO_MEMORY_ARENA_HPP
#define LOGO_MEMORY_ARENA_HPP

#include <new>
#include "utils.hpp"
#include "heap_array.hpp"

namespace logo {
	struct Arena_Allocator {
		struct Arena {
			char* buffer;
			std::size_t capacity;
			std::size_t head_index;
		};

		std::size_t arena_size;
		Heap_Array<Arena> arenas;

		void destroy();
		template<typename T>
		[[nodiscard]] T* construct() {
			static_assert(std::is_trivially_destructible_v<T>);
			void* ptr = allocate_memory(sizeof(T),alignof(T));
			return new(ptr) T{};
		}
		[[nodiscard]] char* construct_string(std::size_t length);
	private:
		bool create_new_arena();
		[[nodiscard]] void* allocate_memory(std::size_t size,std::size_t alignment);
	};
}

#endif
