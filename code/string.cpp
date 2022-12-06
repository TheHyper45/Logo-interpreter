#include <cstdio>
#include <cstring>
#include <cinttypes>
#include "utils.hpp"
#include "string.hpp"

namespace logo {
	String_Const_Iterator& String_Const_Iterator::operator++() {
		char code_unit0 = *ptr;
		if((code_unit0 & 0b10000000) == 0) ptr += 1;
		else if((code_unit0 & 0b11100000) == 0b11000000) ptr += 2;
		else if((code_unit0 & 0b11110000) == 0b11100000) ptr += 3;
		else if((code_unit0 & 0b11111000) == 0b11110000) ptr += 4;
		return *this;
	}

	bool String_Const_Iterator::operator==(String_Const_Iterator iterator) const {
		return ptr == iterator.ptr;
	}

	char32_t String_Const_Iterator::operator*() const {
		char code_unit0 = *ptr;
		if((code_unit0 & 0b10000000) == 0) {
			return static_cast<char32_t>(code_unit0);
		}
		else if((code_unit0 & 0b11100000) == 0b11000000) {
			char code_unit1 = *(ptr + 1);
			return static_cast<char32_t>(((code_unit0 & 0b00011111) << 6) | (code_unit1 & 0b00111111));
		}
		else if((code_unit0 & 0b11110000) == 0b11100000) {
			char code_unit1 = *(ptr + 1);
			char code_unit2 = *(ptr + 2);
			return static_cast<char32_t>(((code_unit0 & 0b00001111) << 12) | ((code_unit1 & 0b00111111) << 6) | (code_unit2 & 0b00111111));
		}
		else if((code_unit0 & 0b11111000) == 0b11110000) {
			char code_unit1 = *(ptr + 1);
			char code_unit2 = *(ptr + 2);
			char code_unit3 = *(ptr + 3);
			return static_cast<char32_t>(((code_unit0 & 0b00000111) << 18) | ((code_unit1 & 0b00111111) << 12) | ((code_unit2 & 0b00111111) << 6) | (code_unit3 & 0b00111111));
		}
	}

	String_View::String_View() : begin_ptr(),end_ptr() {}
	String_View::String_View(const char* str,std::size_t str_len) : begin_ptr(str) {
		if(str_len == String_Error) str_len = std::strlen(str);
		end_ptr = str + str_len;
	}
	std::size_t String_View::length() const { return static_cast<std::size_t>(end_ptr - begin_ptr); }
	String_Const_Iterator String_View::begin() const { return {begin_ptr}; }
	String_Const_Iterator String_View::end() const { return {end_ptr}; }

	bool append_char(char* string,std::size_t* byte_length,std::size_t byte_capacity,char32_t code_point) {
		char code_units[4]{};
		std::size_t code_unit_count = 0;
		if(code_point <= 0x7F) {
			code_unit_count = 1;
			code_units[0] = static_cast<char>(code_point);
		}
		else if(code_point <= 0x7FF) {
			code_unit_count = 2;
			code_units[1] = static_cast<char>(0b10000000 | ((code_point >> 0) & 0b00111111));
			code_units[0] = static_cast<char>(0b11000000 | ((code_point >> 6) & 0b00011111));
		}
		else if(code_point <= 0xFFFF) {
			code_unit_count = 3;
			code_units[2] = static_cast<char>(0b10000000 | ((code_point >> 0) & 0b00111111));
			code_units[1] = static_cast<char>(0b10000000 | ((code_point >> 6) & 0b00111111));
			code_units[0] = static_cast<char>(0b11100000 | ((code_point >> 12) & 0b00001111));
		}
		else if(code_point <= 0x10FFFF) {
			code_unit_count = 4;
			code_units[3] = static_cast<char>(0b10000000 | ((code_point >> 0) & 0b00111111));
			code_units[2] = static_cast<char>(0b10000000 | ((code_point >> 6) & 0b00111111));
			code_units[1] = static_cast<char>(0b10000000 | ((code_point >> 12) & 0b00111111));
			code_units[0] = static_cast<char>(0b11110000 | ((code_point >> 18) & 0b00000111));
		}

		if((*byte_length + code_unit_count) >= byte_capacity) return false;
		std::memcpy(string + *byte_length,code_units,code_unit_count);
		*byte_length += code_unit_count;
		string[*byte_length] = '\0';
		return true;
	}

	bool append_string(char* string,std::size_t* byte_length,std::size_t byte_capacity,const char* to_append,std::size_t to_append_byte_length) {
		if((*byte_length + to_append_byte_length) >= byte_capacity) return false;
		std::memcpy(string + *byte_length,to_append,to_append_byte_length);
		*byte_length += to_append_byte_length;
		string[*byte_length] = '\0';
		return true;
	}

	Runtime_Format_String::Runtime_Format_String(String_View string) : buffer(string.begin_ptr),length(string.length()) {}

	String_Format_Arg make_string_format_arg(std::size_t value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::Size_T;
		arg.value.size_t_v = value;
		return arg;
	}
	String_Format_Arg make_string_format_arg(std::uint_least32_t value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::Uint_Least_32_T;
		arg.value.uint_least32_t_v = value;
		return arg;
	}
	String_Format_Arg make_string_format_arg(String_View value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::String_View;
		arg.value.string_view_v = value;
		return arg;
	}

	std::size_t _format_into(bool(*callback)(char32_t,const void*),const void* callback_arg,String_View format,Array_View<String_Format_Arg> args) {
		std::size_t count = 0;
		std::size_t current_arg_index = 0;
		for(auto c : format) {
			if(c == '%') {
				if(current_arg_index >= args.length) return count;
				const String_Format_Arg& arg = args[current_arg_index];
				switch(arg.type) {
					case String_Format_Arg::Type::Size_T: {
						static constexpr auto buffer_size = logo::max_int_char_count<std::size_t>();
						char buffer[buffer_size + 1]{};
						int result = std::snprintf(buffer,buffer_size,"%zu",arg.value.size_t_v);
						if(result < 0) return count;
						for(auto i : Range(result)) {
							if(!callback(buffer[i],callback_arg)) return count;
							count += 1;
						}
						break;
					}
					case String_Format_Arg::Type::Uint_Least_32_T: {
						static constexpr auto buffer_size = logo::max_int_char_count<std::uint_least32_t>();
						char buffer[buffer_size + 1]{};
						int result = std::snprintf(buffer,buffer_size,"%" PRIuLEAST32,arg.value.uint_least32_t_v);
						if(result < 0) return count;
						for(auto i : Range(result)) {
							if(!callback(buffer[i],callback_arg)) return count;
							count += 1;
						}
						break;
					}
					case String_Format_Arg::Type::String_View: {
						for(auto arg_c : arg.value.string_view_v) {
							if(!callback(arg_c,callback_arg)) return count;
							count += 1;
						}
						break;
					}
				}
				current_arg_index += 1;
			}
			else {
				if(!callback(c,callback_arg)) return count;
				count += 1;
			}
		}
		return count;
	}
}
