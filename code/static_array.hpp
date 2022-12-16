#ifndef LOGO_STATIC_ARRAY_HPP
#define LOGO_STATIC_ARRAY_HPP

#include <cstddef>

namespace logo {
	template<typename T,std::size_t Capacity>
	struct Static_Array {
		T data[Capacity];
		std::size_t length;
		Static_Array() : length() {}

		bool push_back(const T& value) {
			if((length + 1) > Capacity) return false;
			data[length++] = value;
			return true;
		}

		[[nodiscard]] T& operator[](std::size_t index) { return data[index]; }
		[[nodiscard]] const T& operator[](std::size_t index) const { return data[index]; }
		[[nodiscard]] T* begin() { return &data[0]; }
		[[nodiscard]] const T* begin() const { return &data[0]; }
		[[nodiscard]] T* end() { return &data[length]; }
		[[nodiscard]] const T* end() const { return &data[length]; }
	};
}

#endif
