#ifndef LOGO_ARRAY_VIEW
#define LOGO_ARRAY_VIEW

#include <cstddef>

namespace logo {
	template<typename T>
	struct Array_View {
		const T* ptr;
		std::size_t length;
		Array_View() : ptr(),length() {}
		Array_View(const T* _ptr,std::size_t _length) : ptr(_ptr),length(_length) {}
		[[nodiscard]] const T* begin() const { return &ptr[0]; }
		[[nodiscard]] const T* end() const { return &ptr[length]; }
		[[nodiscard]] const T& operator[](std::size_t index) const { return ptr[index]; }
	};
}

#endif
