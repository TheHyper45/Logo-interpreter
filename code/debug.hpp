#ifndef LOGO_DEBUG_HPP
#define LOGO_DEBUG_HPP

#include <source_location>
#include "string.hpp"

namespace logo {
	bool debug_init();
	void debug_term();
	bool _print_stdout_char32_t(char32_t c);
	bool _print_stderr_char32_t(char32_t c);

	template<typename... Args>
	void print(Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::_print_stdout_char32_t,format,std::forward<Args>(args)...);
	}
	template<typename... Args>
	void eprint(Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::_print_stderr_char32_t,format,std::forward<Args>(args)...);
	}

	bool write_char32_t_to_error_message(char32_t code_point);
	template<typename... Args>
	struct Report_Error {
		Report_Error(Format_String<std::type_identity_t<Args>...> format,Args&&... args,std::source_location loc = std::source_location::current()) {
			logo::format_into(logo::write_char32_t_to_error_message,"[Error: %:%] ",loc.file_name(),loc.line());
			logo::format_into(logo::write_char32_t_to_error_message,format,std::forward<Args>(args)...);
			logo::write_char32_t_to_error_message('\n');
		}
	};
	template<typename... Args>
	Report_Error(Format_String<std::type_identity_t<Args>...>,Args&&...) -> Report_Error<Args...>;

	[[nodiscard]] String_View get_reported_error();
	void assert(bool condition,std::source_location loc = std::source_location::current());
	[[noreturn]] void unreachable(std::source_location loc = std::source_location::current());
}

#endif
