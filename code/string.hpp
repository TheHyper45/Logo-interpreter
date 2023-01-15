#ifndef LOGO_STRING_HPP
#define LOGO_STRING_HPP

#include <cstddef>
#include <utility>
#include <cstdint>
#include <type_traits>
#include "array_view.hpp"
#include "static_array.hpp"

namespace logo {
	static inline constexpr std::size_t String_Error = static_cast<std::size_t>(-1);

	[[nodiscard]] char32_t make_code_point(Array_View<char> bytes);
	[[nodiscard]] Static_Array<char,4> make_code_units(char32_t code_point);
	[[nodiscard]] std::size_t code_point_byte_length(char32_t code_point);

	struct String_Const_Iterator {
		const char* ptr;
		String_Const_Iterator& operator++();
		[[nodiscard]] bool operator==(String_Const_Iterator iterator) const;
		[[nodiscard]] char32_t operator*() const;
	};

	struct String_View {
		const char* begin_ptr;
		const char* end_ptr;
		String_View();
		String_View(const char* str,std::size_t str_len = String_Error);
		template<std::size_t Count>
		String_View(const char(&literal)[Count]) : begin_ptr(literal),end_ptr(literal + Count - 1) {}
		[[nodiscard]] std::size_t byte_length() const;
		[[nodiscard]] String_Const_Iterator begin() const;
		[[nodiscard]] String_Const_Iterator end() const;
	};

	bool append_char(char* string,std::size_t* byte_length,std::size_t byte_capacity,char32_t code_point);
	bool append_string(char* string,std::size_t* byte_length,std::size_t byte_capacity,const char* to_append,std::size_t to_append_byte_length);

	template<std::size_t Capacity>
	struct Array_String {
		char buffer[Capacity];
		std::size_t byte_length;
		Array_String() : byte_length() { buffer[0] = '\0'; }
		[[nodiscard]] String_Const_Iterator begin() const { return {buffer}; }
		[[nodiscard]] String_Const_Iterator end() const { return {buffer + byte_length}; }
		bool append(char32_t code_point) { return logo::append_char(buffer,&byte_length,Capacity,code_point); }
		bool append(String_View string) { return logo::append_string(buffer,&byte_length,Capacity,string.begin_ptr,string.byte_length()); }
	};

	//Don't define this function.
	void _invalid_format_string_argument_count();

	struct Runtime_Format_String {
		const char* buffer;
		std::size_t length;
		Runtime_Format_String(String_View string);
	};

	template<typename... Args>
	struct Format_String {
		const char* buffer;
		std::size_t length;
		Format_String(const Runtime_Format_String& string) : buffer(string.buffer),length(string.length) {}
		template<std::size_t Count>
		consteval Format_String(const char(&literal)[Count]) : buffer(literal),length(Count - 1) {
			std::size_t arg_count = 0;
			for(auto c : literal) if(c == '%') arg_count += 1;
			if(arg_count != sizeof...(Args)) logo::_invalid_format_string_argument_count();
		}
	};

	struct String_Format_Arg {
		enum struct Type {
			Size_T,
			Uint_Least_32_T,
			String_View,
			Char,
			Char32_T,
			Double,
			Int64,
			Bool
		};
		union Value {
			std::size_t size_t_v;
			std::uint_least32_t uint_least32_t_v;
			String_View string_view_v;
			char char_v;
			char32_t char32_t_v;
			double double_v;
			std::int64_t int64_v;
			bool bool_v;
		};
		Type type;
		Value value;
	};

	[[nodiscard]] String_Format_Arg make_string_format_arg(std::size_t value);
	[[nodiscard]] String_Format_Arg make_string_format_arg(std::uint_least32_t value);
	[[nodiscard]] String_Format_Arg make_string_format_arg(String_View value);
	[[nodiscard]] String_Format_Arg make_string_format_arg(char value);
	[[nodiscard]] String_Format_Arg make_string_format_arg(char32_t value);
	[[nodiscard]] String_Format_Arg make_string_format_arg(double value);
	[[nodiscard]] String_Format_Arg make_string_format_arg(std::int64_t value);
	[[nodiscard]] String_Format_Arg make_string_format_arg(bool value);
	[[nodiscard]] String_Format_Arg make_string_format_arg(const char* value);
	template<std::size_t Count>
	[[nodiscard]] String_Format_Arg make_string_format_arg(const char(&value)[Count]) {
		String_Format_Arg arg{};
		arg.type = String_Format_Arg::Type::String_View;
		arg.value.string_view_v = String_View(value,Count - 1);
		return arg;
	}
	template<std::size_t Count>
	[[nodiscard]] String_Format_Arg make_string_format_arg(const Array_String<Count>& string) {
		String_Format_Arg result{};
		result.type = String_Format_Arg::Type::String_View;
		result.value.string_view_v = {string.buffer,string.byte_length};
		return result;
	}

	std::size_t _format_into(bool(*callback)(char32_t,const void*),const void* callback_arg,String_View format,Array_View<String_Format_Arg> args);

	template<typename Callback,typename... Args>
	std::size_t format_into(const Callback& callback,Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		auto lambda = [](char32_t c,const void* arg) -> bool { return (*reinterpret_cast<const Callback*>(arg))(c); };
		if constexpr(sizeof...(Args) > 0) {
			String_Format_Arg format_args[] = {logo::make_string_format_arg(std::forward<Args>(args))...};
			return logo::_format_into(lambda,reinterpret_cast<const void*>(&callback),{format.buffer,format.length},{format_args,sizeof...(Args)});
		}
		else return logo::_format_into(lambda,reinterpret_cast<const void*>(&callback),{format.buffer,format.length},{});
	}

	template<std::size_t Count,typename... Args>
	std::size_t format(Array_String<Count>* output,Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		auto lambda = [&](char32_t c) { return output->append(c); };
		return logo::format_into(lambda,format,std::forward<Args>(args)...);
	}
}

#endif
