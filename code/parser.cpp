#include "lexer.hpp"
#include "debug.hpp"
#include "parser.hpp"
#include "heap_array.hpp"

namespace logo {
	void Parsing_Result::destroy() {
		memory.destroy();
	}

	template<typename... Args>
	static void report_parser_error(Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::write_char32_t_to_error_message,"[Syntax error] Line %: ",logo::get_token_line_index());
		logo::format_into(logo::write_char32_t_to_error_message,format,std::forward<Args>(args)...);
		logo::write_char32_t_to_error_message('\n');
	}

	template<typename... Args>
	[[nodiscard]] static Lexing_Result require_next_token(Token_Type type,Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		auto token = logo::get_next_token();
		if(token.status == Lexing_Status::Error) return Lexing_Status::Error;
		if(token.status == Lexing_Status::Out_Of_Tokens) {
			logo::report_parser_error(format,args...);
			return Lexing_Status::Error;
		}
		if(token.token->type != type) {
			logo::report_parser_error(format,args...);
			return Lexing_Status::Error;
		}
		return token;
	}

	enum struct Parsing_Status {
		Continue,
		Error,
		Complete
	};

	[[nodiscard]] Option<Parsing_Result> parse_input(Array_View<char> input) {
		if(input.length == 0) {
			logo::report_parser_error("Empty input file.");
			return {};
		}
		if(!logo::init_lexer(input)) {
			return {};
		}
		defer[]{logo::term_lexer();};
		
		while(true) {
			auto token = logo::get_next_token();
			if(token.status == Lexing_Status::Out_Of_Tokens) break;
			logo::print("(%,%) %\n",token.token->line_index,static_cast<std::size_t>(token.token->type),token.token->string);
		}
		return {};
		
		/*Parsing_Result result{};
		bool successful_return = false;
		defer[&]{if(!successful_return) result.destroy();};

		while(true) {
			auto status = logo::parse_statement(&result);
			if(status == Parsing_Status::Error) return {};
			if(status == Parsing_Status::Complete) break;
		}*/

		//successful_return = true;
		//return result;
	}
}
