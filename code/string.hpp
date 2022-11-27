#ifndef STRING_HPP
#define STRING_HPP

#include <cstddef>

namespace logo {
	struct String_Const_Iterator {
		char* ptr;
		String_Const_Iterator& operator++();
		[[nodiscard]] bool operator==(String_Const_Iterator iterator) const noexcept;
		[[nodiscard]] char32_t operator*();
	};

	template<std::size_t Capacity>
	struct Array_String {
		char buffer[Capacity];
		std::size_t byte_length;
		[[nodiscard]] String_Const_Iterator begin() const noexcept { return {buffer}; }
		[[nodiscard]] String_Const_Iterator end() const noexcept { return {buffer + byte_length}; }
	};
}

#endif
