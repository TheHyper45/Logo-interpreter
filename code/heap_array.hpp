#ifndef HEAP_ARRAY_HPP
#define HEAP_ARRAY_HPP

#include <new>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include "utils.hpp"

namespace logo {
	template<typename T>
	struct Heap_Array {
		T* data;
		std::size_t capacity;
		std::size_t length;

		void destroy() {
			delete[] data;
			data = nullptr;
			capacity = 0;
			length = 0;
		}
		bool reserve(std::size_t new_capacity) {
			if(new_capacity <= capacity) return true;
			T* tmp = new(std::nothrow) T[new_capacity];
			if(!tmp) return false;
			if constexpr(std::is_trivially_copyable_v<T>) {
				if(length > 0) std::memcpy(tmp,data,length * sizeof(T));
			}
			else for(auto i : Range(length)) { tmp[i] = data[i]; }
			delete[] data;
			data = tmp;
			capacity = new_capacity;
			return true;
		}
		bool resize(std::size_t new_capacity) {
			if(!reserve(new_capacity)) return false;
			for(auto i : Range(length,capacity)) data[i] = {};
			length = capacity;
			return true;
		}
		bool push_back(const T& value) {
			if((length + 1) > capacity) {
				std::size_t new_capacity = (capacity == 0) ? 1 : (capacity * 2);
				if(!reserve(new_capacity)) return false;
			}
			data[length] = value;
			length += 1;
			return true;
		}

		[[nodiscard]] T* begin() { return &data[0]; }
		[[nodiscard]] T* end() { return &data[length]; }
		[[nodiscard]] T& operator[](std::size_t index) { return data[index]; }
		[[nodiscard]] const T* begin() const { return &data[0]; }
		[[nodiscard]] const T* end() const { return &data[length]; }
		[[nodiscard]] const T& operator[](std::size_t index) const { return data[index]; }
	};
}

#endif
