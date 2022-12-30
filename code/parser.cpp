#include "utils.hpp"
#include "lexer.hpp"
#include "debug.hpp"
#include "parser.hpp"

/*
#define LOGO_CHECK_LEXING_RESULT(VAR)\
	do {\
		if((VAR).status == Lexing_Status::Error || (VAR).status == Lexing_Status::Out_Of_Tokens) {\
			return false;\
		}\
	}\
	while(false)*/

namespace logo {
	static struct {
		std::size_t last_token_line_index;
	} parser;

	/*template<typename... Args>
	void report_parser_error(Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::write_char32_t_to_error_message,"[Syntax error: %] ",parser.last_token_line_index);
		logo::format_into(logo::write_char32_t_to_error_message,format,std::forward<Args>(args)...);
		logo::write_char32_t_to_error_message('\n');
	}

	template<typename... Args>
	[[nodiscard]] static Lexing_Result require_next_token(Token_Type type,Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		while(true) {
			auto result = logo::get_next_token();
			switch(result.status) {
				case Lexing_Status::Out_Of_Tokens: {
					logo::report_parser_error(format,args...);
					return result.status;
				}
				case Lexing_Status::Error: {
					return result.status;
				}
				default: {
					if(result.token.type != type) {
						parser.last_token_line_index = result.token.line_index;
						logo::report_parser_error(format,args...);
						return Lexing_Status::Error;
					}
					return result;
				}
			}
		}
	}

	static bool parse_expression() {
		//TODO: Implement parsing expressions.
		return true;
	}*/

	bool parse_input(Array_View<char> input) {
		if(!logo::init_lexer(input)) {
			return false;
		}
		defer[]{logo::term_lexer();};

		while(true) {
			auto result = logo::get_next_token();
			if(result.status == Lexing_Status::Out_Of_Tokens) break;
			if(result.token->type == Token_Type::String_Literal) {
				logo::print("(%) \"%\"\n",result.token->line_index,result.token->string);
			}
			else logo::print("(%) %\n",result.token->line_index,result.token->string);
		}

		/*logo::init_lexer(begin,end);
		defer[]{logo::term_lexer();};

		{
			auto first = logo::get_next_token();
			if(first.status == Lexing_Status::Error) {
				return false;
			}
			if(first.status == Lexing_Status::Out_Of_Tokens) {
				return true;
			}

			if(first.token.type == Token_Type::Keyword_Let) {
				auto identifier = logo::require_next_token(Token_Type::Identifier,"After 'let' an identifier is required.");
				LOGO_CHECK_LEXING_RESULT(identifier);
				
				auto assignment = logo::require_next_token(Token_Type::Equals_Sign,"After an identifier in a variable declaration, '=' is required.");
				LOGO_CHECK_LEXING_RESULT(assignment);

				if(!logo::parse_expression()) {
					return false;
				}

				auto semicolon = logo::require_next_token(Token_Type::Semicolon,"Missing ';' at the end of a line.");
				LOGO_CHECK_LEXING_RESULT(semicolon);
			}
		}*/

		return true;
	}
}
