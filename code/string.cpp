#include "utils.hpp"
#include "string.hpp"

namespace logo {
	String_Const_Iterator& String_Const_Iterator::operator++() {
		char code_unit0 = *ptr;
		if((code_unit0 & 0b10000000) == 0) ptr += 1;
		else if((code_unit0 & 0b11100000) == 0b11000000) {
			char code_unit1 = *(ptr + 1);
			if((code_unit1 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			ptr += 2;
		}
		else if((code_unit0 & 0b11110000) == 0b11100000) {
			char code_unit1 = *(ptr + 1);
			if((code_unit1 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			char code_unit2 = *(ptr + 2);
			if((code_unit2 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			ptr += 3;
		}
		else if((code_unit0 & 0b11111000) == 0b11110000) {
			char code_unit1 = *(ptr + 1);
			if((code_unit1 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			char code_unit2 = *(ptr + 2);
			if((code_unit2 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			char code_unit3 = *(ptr + 3);
			if((code_unit3 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			ptr += 4;
		}
		else throw Invalid_Utf8_Sequence{};
	}

	bool String_Const_Iterator::operator==(String_Const_Iterator iterator) const noexcept {
		return ptr == iterator.ptr;
	}

	char32_t String_Const_Iterator::operator*() {
		char code_unit0 = *ptr;
		if((code_unit0 & 0b10000000) == 0) {
			return static_cast<char32_t>(code_unit0);
		}
		else if((code_unit0 & 0b11100000) == 0b11000000) {
			char code_unit1 = *(ptr + 1);
			if((code_unit1 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			return static_cast<char32_t>(((code_unit0 & 0b00011111) << 6) | (code_unit1 & 0b00111111));
		}
		else if((code_unit0 & 0b11110000) == 0b11100000) {
			char code_unit1 = *(ptr + 1);
			if((code_unit1 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			char code_unit2 = *(ptr + 2);
			if((code_unit2 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			return static_cast<char32_t>(((code_unit0 & 0b00001111) << 12) | ((code_unit1 & 0b00111111) << 6) | (code_unit2 & 0b00111111));
		}
		else if((code_unit0 & 0b11111000) == 0b11110000) {
			char code_unit1 = *(ptr + 1);
			if((code_unit1 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			char code_unit2 = *(ptr + 2);
			if((code_unit2 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			char code_unit3 = *(ptr + 3);
			if((code_unit3 & 0b11000000) != 0b10000000) throw Invalid_Utf8_Sequence{};
			return static_cast<char32_t>(((code_unit0 & 0b00000111) << 18) | ((code_unit1 & 0b00111111) << 12) | ((code_unit2 & 0b00111111) << 6) | (code_unit3 & 0b00111111));
		}
		else throw Invalid_Utf8_Sequence{};
	}
}
