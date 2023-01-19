#include <cstdio>
#include <cstring>
#include <cinttypes>
#include "utils.hpp"
#include "string.hpp"
#include "debug.hpp"

namespace logo {
	char32_t make_code_point(Array_View<char> bytes) {
		if(bytes.length == 1)
			return static_cast<char32_t>(bytes[0]);
		else if(bytes.length == 2)
			return static_cast<char32_t>(((bytes[0] & 0b00011111) << 6) | (bytes[1] & 0b00111111));
		else if(bytes.length == 3)
			return static_cast<char32_t>(((bytes[0] & 0b00001111) << 12) | ((bytes[1] & 0b00111111) << 6) | (bytes[2] & 0b00111111));
		else if(bytes.length == 4)
			return static_cast<char32_t>(((bytes[0] & 0b00000111) << 18) | ((bytes[1] & 0b00111111) << 12) | ((bytes[2] & 0b00111111) << 6) | (bytes[3] & 0b00111111));
		logo::unreachable();
	}
	Static_Array<char,4> make_code_units(char32_t code_point) {
		Static_Array<char,4> bytes{};
		if(code_point <= 0x7F) {
			bytes.push_back(static_cast<char>(code_point));
		}
		else if(code_point <= 0x7FF) {
			bytes.push_back(static_cast<char>(0b11000000 | ((code_point >> 6) & 0b00011111)));
			bytes.push_back(static_cast<char>(0b10000000 | ((code_point >> 0) & 0b00111111)));
		}
		else if(code_point <= 0xFFFF) {
			bytes.push_back(static_cast<char>(0b11100000 | ((code_point >> 12) & 0b00001111)));
			bytes.push_back(static_cast<char>(0b10000000 | ((code_point >> 6) & 0b00111111)));
			bytes.push_back(static_cast<char>(0b10000000 | ((code_point >> 0) & 0b00111111)));
		}
		else if(code_point <= 0x10FFFF) {
			bytes.push_back(static_cast<char>(0b11110000 | ((code_point >> 18) & 0b00000111)));
			bytes.push_back(static_cast<char>(0b10000000 | ((code_point >> 12) & 0b00111111)));
			bytes.push_back(static_cast<char>(0b10000000 | ((code_point >> 6) & 0b00111111)));
			bytes.push_back(static_cast<char>(0b10000000 | ((code_point >> 0) & 0b00111111)));
		}
		return bytes;
	}
	std::size_t code_point_byte_length(char32_t code_point) {
		if(code_point <= 0x7F) return 1;
		else if(code_point <= 0x7FF) return 2;
		else if(code_point <= 0xFFFF) return 3;
		else if(code_point <= 0x10FFFF) return 4;
		logo::unreachable();
	}

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
		logo::unreachable();
	}

	String_View::String_View() : begin_ptr(),end_ptr() {}
	String_View::String_View(const char* str,std::size_t str_len) : begin_ptr(str) {
		if(str_len == String_Error) str_len = std::strlen(str);
		end_ptr = str + str_len;
	}
	std::size_t String_View::byte_length() const { return static_cast<std::size_t>(end_ptr - begin_ptr); }
	String_Const_Iterator String_View::begin() const { return {begin_ptr}; }
	String_Const_Iterator String_View::end() const { return {end_ptr}; }

	bool append_char(char* string,std::size_t* byte_length,std::size_t byte_capacity,char32_t code_point) {
		auto code_units = logo::make_code_units(code_point);
		if((*byte_length + code_units.length) >= byte_capacity) return false;
		std::memcpy(string + *byte_length,code_units.data,code_units.length);
		*byte_length += code_units.length;
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

	Runtime_Format_String::Runtime_Format_String(String_View string) : buffer(string.begin_ptr),length(string.byte_length()) {}

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
	String_Format_Arg make_string_format_arg(char value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::Char;
		arg.value.char_v = value;
		return arg;
	}
	String_Format_Arg make_string_format_arg(char32_t value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::Char32_T;
		arg.value.char32_t_v = value;
		return arg;
	}
	String_Format_Arg make_string_format_arg(double value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::Double;
		arg.value.double_v = value;
		return arg;
	}
	String_Format_Arg make_string_format_arg(std::int32_t value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::Int32;
		arg.value.int32_v = value;
		return arg;
	}
	String_Format_Arg make_string_format_arg(std::int64_t value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::Int64;
		arg.value.int64_v = value;
		return arg;
	}
	String_Format_Arg make_string_format_arg(bool value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::Bool;
		arg.value.bool_v = value;
		return arg;
	}
	String_Format_Arg make_string_format_arg(const char* value) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::String_View;
		arg.value.string_view_v = String_View(value,std::strlen(value));
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
						char buffer[32]{};
						int result = std::snprintf(buffer,sizeof(buffer) - 1,"%zu",arg.value.size_t_v);
						if(result < 0) return count;
						for(auto i : Range(result)) {
							if(!callback(static_cast<char32_t>(buffer[i]),callback_arg)) return count;
							count += 1;
						}
						break;
					}
					case String_Format_Arg::Type::Uint_Least_32_T: {
						char buffer[32]{};
						int result = std::snprintf(buffer,sizeof(buffer) - 1,"%" PRIuLEAST32,arg.value.uint_least32_t_v);
						if(result < 0) return count;
						for(auto i : Range(result)) {
							if(!callback(static_cast<char32_t>(buffer[i]),callback_arg)) return count;
							count += 1;
						}
						break;
					}
					case String_Format_Arg::Type::String_View: {
						for(auto arg_c : arg.value.string_view_v) {
							if(!callback(arg_c,callback_arg)) return count;
							count += logo::code_point_byte_length(arg_c);
						}
						break;
					}
					case String_Format_Arg::Type::Char: {
						char buffer[32]{};
						int result = std::snprintf(buffer,sizeof(buffer) - 1,"%d",arg.value.char_v);
						if(result < 0) return count;
						for(auto i : Range(result)) {
							if(!callback(static_cast<char32_t>(buffer[i]),callback_arg)) return count;
							count += 1;
						}
						break;
					}
					case String_Format_Arg::Type::Char32_T: {
						if(!callback(arg.value.char32_t_v,callback_arg)) return count;
						count += logo::code_point_byte_length(arg.value.char32_t_v);
						break;
					}
					case String_Format_Arg::Type::Double: {
						char buffer[128]{};
						int result = std::snprintf(buffer,sizeof(buffer) - 1,"%f",arg.value.double_v);
						if(result < 0) return count;
						for(auto i : Range(result)) {
							if(!callback(static_cast<char32_t>(buffer[i]),callback_arg)) return count;
							count += 1;
						}
						break;
					}
					case String_Format_Arg::Type::Int32: {
						char buffer[32]{};
						int result = std::snprintf(buffer,sizeof(buffer) - 1,"%" PRId32,arg.value.int32_v);
						if(result < 0) return count;
						for(auto i : Range(result)) {
							if(!callback(static_cast<char32_t>(buffer[i]),callback_arg)) return count;
							count += 1;
						}
						break;
					}
					case String_Format_Arg::Type::Int64: {
						char buffer[32]{};
						int result = std::snprintf(buffer,sizeof(buffer) - 1,"%" PRId64,arg.value.int64_v);
						if(result < 0) return count;
						for(auto i : Range(result)) {
							if(!callback(static_cast<char32_t>(buffer[i]),callback_arg)) return count;
							count += 1;
						}
						break;
					}
					case String_Format_Arg::Type::Bool: {
						if(arg.value.bool_v) {
							char buffer[] = "true";
							for(auto i : Range(sizeof(buffer) - 1)) {
								if(!callback(static_cast<char32_t>(buffer[i]),callback_arg)) return count;
								count += 1;
							}
						}
						else {
							char buffer[] = "false";
							for(auto i : Range(sizeof(buffer) - 1)) {
								if(!callback(static_cast<char32_t>(buffer[i]),callback_arg)) return count;
								count += 1;
							}
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
