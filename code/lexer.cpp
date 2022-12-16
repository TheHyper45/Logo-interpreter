#include "debug.hpp"
#include "lexer.hpp"
#include "heap_array.hpp"
#include "static_array.hpp"

namespace logo {
	static struct {
		const char* begin;
		const char* end;
		std::size_t token_start_byte_index;
		std::size_t token_byte_count;
		Heap_Array<char> token_string_bytes;
		char32_t last_token_code_point;
		bool is_number;
		bool is_string_literal;
		bool is_comment;
		bool ignore_first_quote;
		bool escape_next_character;
		std::size_t current_line_index;
	} lexer{};

	void init_lexer(const char* begin,const char* end) {
		lexer.begin = begin;
		lexer.end = end;
		lexer.current_line_index = 1;
	}

	void term_lexer() {
		lexer.token_string_bytes.destroy();
	}

	String_View Token::string_view() const {
		return String_View(&lexer.token_string_bytes[start_index],length);
	}

	template<typename... Args>
	Lexing_Status report_lexer_error(Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::write_char32_t_to_error_message,"[Lexer error: %] ",lexer.current_line_index);
		logo::format_into(logo::write_char32_t_to_error_message,format,std::forward<Args>(args)...);
		logo::write_char32_t_to_error_message('\n');
		return Lexing_Status::Error;
	}

	[[nodiscard]] static bool is_code_point_alpha(char32_t code_point) {
		return (code_point >= 'a' && code_point <= 'z') || (code_point >= 'A' && code_point <= 'Z') ||
			code_point == U'ą' || code_point == U'ć' || code_point == U'ę' || code_point == U'ł' ||
			code_point == U'ń' || code_point == U'ó' || code_point == U'ś' || code_point == U'ź' ||
			code_point == U'ż' || code_point == U'Ą' || code_point == U'Ć' || code_point == U'Ę' ||
			code_point == U'Ł' || code_point == U'Ń' || code_point == U'Ó' || code_point == U'Ś' ||
			code_point == U'Ź' || code_point == U'Ż' || (code_point >= U'Α' && code_point <= U'ω') ||
			code_point == '_';
	}
	[[nodiscard]] static bool is_code_point_digit(char32_t code_point) {
		return code_point >= '0' && code_point <= '9';
	}
	[[nodiscard]] static bool is_code_point_whitespace(char32_t code_point) {
		return code_point == ' ' || code_point == '\t' || code_point == '\r';
	}
	[[nodiscard]] static bool is_code_point_special(char32_t code_point) {
		return code_point == '(' || code_point == ')' || code_point == '[' || code_point == ']' ||
			code_point == '{' || code_point == '}' || code_point == ',' || code_point == U'°' ||
			code_point == U'√' || code_point == U'∛' || code_point == U'∜' || code_point == '=' ||
			code_point == U'≠' || code_point == U'∧' || code_point == U'∨' || code_point == U'¬' ||
			code_point == '<' || code_point == U'≤' || code_point == '>' || code_point == U'≥' || 
			code_point == '+' || code_point == '-' || code_point == '*' || code_point == '/' ||
			code_point == '^' || code_point == '!' || code_point == ';' || code_point == '\'' ||
			code_point == ':' || code_point == '%' || code_point == '\0';
	}

	[[nodiscard]] static Lexing_Status append_code_point_bytes(char32_t code_point) {
		auto bytes = logo::make_code_units(code_point);
		if(!lexer.token_string_bytes.push_back(Array_View(bytes.data,bytes.length))) {
			Report_Error("Couldn't allocate % bytes of memory.",bytes.length);
			return Lexing_Status::Error;
		}
		lexer.last_token_code_point = code_point;
		lexer.token_byte_count += bytes.length;
		return Lexing_Status::Continue;
	}

	[[nodiscard]] static Lexing_Result create_token(char32_t code_point) {
		auto bytes = logo::make_code_units(code_point);
		if(!lexer.token_string_bytes.push_back('\0')) {
			Report_Error("Couldn't allocate a byte of memory.");
			return Lexing_Status::Error;
		}
		if(!lexer.token_string_bytes.push_back(Array_View(bytes.data,bytes.length))) {
			lexer.token_string_bytes.pop_back();
			Report_Error("Couldn't allocate a byte of memory.");
			return Lexing_Status::Error;
		}

		Token token{Token_Type::None,lexer.token_start_byte_index,lexer.token_byte_count,lexer.current_line_index};
		if(lexer.is_number) {
			token.type = Token_Type::Number_Literal;
		}
		else if(lexer.is_string_literal) {
			token.type = Token_Type::String_Literal;
		}
		else if(lexer.is_comment) {
			token.type = Token_Type::Comment;
		}
		else if(logo::compare_strings_equal(token.string_view(),"==")) {
			token.type = Token_Type::Compare_Equal;
		}
		else if(logo::compare_strings_equal(token.string_view(),"!=")) {
			token.type = Token_Type::Compare_Unequal;
		}
		else if(logo::compare_strings_equal(token.string_view(),"<=")) {
			token.type = Token_Type::Compare_Less_Than_Or_Equal;
		}
		else if(logo::compare_strings_equal(token.string_view(),">=")) {
			token.type = Token_Type::Compare_Greater_Than_Or_Equal;
		}
		else if(logo::compare_strings_equal(token.string_view(),"+=")) {
			token.type = Token_Type::Compound_Plus;
		}
		else if(logo::compare_strings_equal(token.string_view(),"-=")) {
			token.type = Token_Type::Compound_Minus;
		}
		else if(logo::compare_strings_equal(token.string_view(),"*=")) {
			token.type = Token_Type::Compound_Multiply;
		}
		else if(logo::compare_strings_equal(token.string_view(),"/=")) {
			token.type = Token_Type::Compound_Divide;
		}
		else if(logo::compare_strings_equal(token.string_view(),"%=")) {
			token.type = Token_Type::Compound_Remainder;
		}
		else if(logo::compare_strings_equal(token.string_view(),"^=")) {
			token.type = Token_Type::Compound_Exponentiate;
		}
		else if(logo::compare_strings_equal(token.string_view(),"if")) {
			token.type = Token_Type::Keyword_If;
		}
		else if(logo::compare_strings_equal(token.string_view(),"for")) {
			token.type = Token_Type::Keyword_For;
		}
		else if(logo::compare_strings_equal(token.string_view(),"while")) {
			token.type = Token_Type::Keyword_While;
		}
		else if(logo::compare_strings_equal(token.string_view(),"let")) {
			token.type = Token_Type::Keyword_Let;
		}
		else if(logo::compare_strings_equal(token.string_view(),"return")) {
			token.type = Token_Type::Keyword_Return;
		}
		else if(logo::compare_strings_equal(token.string_view(),"break")) {
			token.type = Token_Type::Keyword_Break;
		}
		else if(logo::compare_strings_equal(token.string_view(),"continue")) {
			token.type = Token_Type::Keyword_Continue;
		}
		else if(logo::compare_strings_equal(token.string_view(),"and")) {
			token.type = Token_Type::Logical_And;
		}
		else if(logo::compare_strings_equal(token.string_view(),"or")) {
			token.type = Token_Type::Logical_Or;
		}
		else if(logo::compare_strings_equal(token.string_view(),"not")) {
			token.type = Token_Type::Logical_Not;
		}
		else if(lexer.last_token_code_point == '\n') {
			lexer.current_line_index += 1;
			token.type = Token_Type::Newline;
		}
		else if(lexer.last_token_code_point == '(') {
			token.type = Token_Type::Left_Paren;
		}
		else if(lexer.last_token_code_point == ')') {
			token.type = Token_Type::Right_Paren;
		}
		else if(lexer.last_token_code_point == '[') {
			token.type = Token_Type::Left_Bracket;
		}
		else if(lexer.last_token_code_point == ']') {
			token.type = Token_Type::Right_Bracket;
		}
		else if(lexer.last_token_code_point == '{') {
			token.type = Token_Type::Left_Brace;
		}
		else if(lexer.last_token_code_point == '}') {
			token.type = Token_Type::Right_Brace;
		}
		else if(lexer.last_token_code_point == ',') {
			token.type = Token_Type::Comma;
		}
		else if(lexer.last_token_code_point == ';') {
			token.type = Token_Type::Semicolon;
		}
		else if(lexer.last_token_code_point == ':') {
			token.type = Token_Type::Colon;
		}
		else if(lexer.last_token_code_point == '+') {
			token.type = Token_Type::Plus;
		}
		else if(lexer.last_token_code_point == '-') {
			token.type = Token_Type::Minus;
		}
		else if(lexer.last_token_code_point == '*') {
			token.type = Token_Type::Asterisk;
		}
		else if(lexer.last_token_code_point == '/') {
			token.type = Token_Type::Slash;
		}
		else if(lexer.last_token_code_point == '^') {
			token.type = Token_Type::Caret;
		}
		else if(lexer.last_token_code_point == '=') {
			token.type = Token_Type::Equals_Sign;
		}
		else if(lexer.last_token_code_point == U'≠') {
			token.type = Token_Type::Compare_Unequal;
		}
		else if(lexer.last_token_code_point == '<') {
			token.type = Token_Type::Compare_Less_Than;
		}
		else if(lexer.last_token_code_point == '>') {
			token.type = Token_Type::Compare_Greater_Than;
		}
		else if(lexer.last_token_code_point == U'≤') {
			token.type = Token_Type::Compare_Less_Than_Or_Equal;
		}
		else if(lexer.last_token_code_point == U'≥') {
			token.type = Token_Type::Compare_Greater_Than_Or_Equal;
		}
		else if(lexer.last_token_code_point == U'∧') {
			token.type = Token_Type::Logical_And;
		}
		else if(lexer.last_token_code_point == U'∨') {
			token.type = Token_Type::Logical_Or;
		}
		else if(lexer.last_token_code_point == U'¬') {
			token.type = Token_Type::Logical_Not;
		}
		else if(lexer.last_token_code_point == U'°') {
			token.type = Token_Type::Degree_Sign;
		}
		else if(lexer.last_token_code_point == U'√') {
			token.type = Token_Type::Square_Root;
		}
		else if(lexer.last_token_code_point == U'∛') {
			token.type = Token_Type::Cube_Root;
		}
		else if(lexer.last_token_code_point == U'∜') {
			token.type = Token_Type::Fourth_Root;
		}
		else if(logo::is_code_point_whitespace(lexer.last_token_code_point)) {
			token.type = Token_Type::Whitespace;
		}
		else token.type = Token_Type::Identifier;

		lexer.token_start_byte_index += lexer.token_byte_count + 1;
		lexer.last_token_code_point = code_point;
		lexer.token_byte_count = bytes.length;
		return token;
	}
	
	[[nodiscard]] static Lexing_Result process_code_point(char32_t code_point) {
		if(!lexer.ignore_first_quote && lexer.last_token_code_point == '\"') {
			auto result = logo::create_token(code_point);
			lexer.is_string_literal = false;
			if(logo::is_code_point_digit(code_point)) {
				lexer.is_number = true;
			}
			else if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			else if(code_point == '#') {
				lexer.is_comment = true;
			}
			return result;
		}
		if(lexer.is_string_literal) {
			if(code_point == '\n') {
				return logo::report_lexer_error("String literals cannot contain newlines. Use escape sequence '\\n' instead.");
			}
			lexer.ignore_first_quote = false;
			if(lexer.escape_next_character) {
				if(code_point == 'n') {
					code_point = '\n';
				}
				else if(code_point == '\"') {
					lexer.ignore_first_quote = true;
				}
				lexer.escape_next_character = false;
				return logo::append_code_point_bytes(code_point);
			}
			if(code_point == '\\') {
				lexer.escape_next_character = true;
				return Lexing_Status::Continue;
			}
			return logo::append_code_point_bytes(code_point);
		}
		if(lexer.is_comment) {
			if(code_point == '\n') {
				auto result = logo::create_token(code_point);
				lexer.is_comment = false;
				return result;
			}
			return logo::append_code_point_bytes(code_point);
		}
		if(lexer.last_token_code_point == '\0') {
			if(logo::is_code_point_digit(code_point)) {
				lexer.is_number = true;
			}
			else if(code_point == '#') {
				lexer.is_comment = true;
			}
			else if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			return logo::append_code_point_bytes(code_point);
		}
		if(lexer.last_token_code_point == '.') {
			if(lexer.is_number && logo::is_code_point_digit(code_point)) {
				return logo::append_code_point_bytes(code_point);
			}
			auto result = logo::create_token(code_point);
			if(code_point == '#') {
				lexer.is_comment = true;
			}
			else if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			return result;
		}
		if(lexer.last_token_code_point == '\n') {
			auto result = logo::create_token(code_point);
			if(logo::is_code_point_digit(code_point)) {
				lexer.is_number = true;
			}
			else if(code_point == '#') {
				lexer.is_comment = true;
			}
			else if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			return result;
		}
		if(logo::is_code_point_alpha(lexer.last_token_code_point)) {
			if(logo::is_code_point_alpha(code_point) || logo::is_code_point_digit(code_point)) {
				return logo::append_code_point_bytes(code_point);
			}
			auto result = logo::create_token(code_point);
			if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			else if(code_point == '#') {
				lexer.is_comment = true;
			}
			return result;
		}
		if(logo::is_code_point_digit(lexer.last_token_code_point)) {
			if(logo::is_code_point_digit(code_point)) {
				return logo::append_code_point_bytes(code_point);
			}
			if(code_point == '.' && lexer.is_number) {
				return logo::append_code_point_bytes(code_point);
			}
			auto result = logo::create_token(code_point);
			lexer.is_number = false;
			if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			else if(code_point == '#') {
				lexer.is_comment = true;
			}
			return result;
		}
		if(lexer.last_token_code_point == '=' || lexer.last_token_code_point == '!' || lexer.last_token_code_point == '<' || lexer.last_token_code_point == '>' ||
		   lexer.last_token_code_point == '+' || lexer.last_token_code_point == '-' || lexer.last_token_code_point == '*' || lexer.last_token_code_point == '/' ||
		   lexer.last_token_code_point == '%' || lexer.last_token_code_point == '^') {
			if(code_point == '=') {
				return logo::append_code_point_bytes(code_point);
			}
			auto result = logo::create_token(code_point);
			if(logo::is_code_point_digit(code_point)) {
				lexer.is_number = true;
			}
			else if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			else if(code_point == '#') {
				lexer.is_comment = true;
			}
			return result;
		}
		if(logo::is_code_point_special(lexer.last_token_code_point)) {
			auto result = logo::create_token(code_point);
			if(logo::is_code_point_digit(code_point)) {
				lexer.is_number = true;
			}
			else if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			else if(code_point == '#') {
				lexer.is_comment = true;
			}
			return result;
		}
		if(logo::is_code_point_whitespace(lexer.last_token_code_point)) {
			if(logo::is_code_point_whitespace(code_point)) {
				return logo::append_code_point_bytes(code_point);
			}
			auto result = logo::create_token(code_point);
			if(logo::is_code_point_digit(code_point)) {
				lexer.is_number = true;
			}
			else if(code_point == '\"') {
				lexer.is_string_literal = true;
				lexer.ignore_first_quote = true;
			}
			else if(code_point == '#') {
				lexer.is_comment = true;
			}
			return result;
		}
		Report_Error("Unrecognized UTF-8 code point (%).",lexer.last_token_code_point);
		return Lexing_Status::Error;
	}

	Lexing_Result get_next_token() {
		char32_t code_point = 0;
		std::size_t code_point_length = 0;
		std::size_t code_point_remaining_byte_count = 0;
		for(const char* ptr = lexer.begin;ptr != lexer.end;ptr += 1) {
			char code_unit = *ptr;
			if(code_point_length == 0) {
				if((code_unit & 0b10000000) == 0) {
					code_point = static_cast<char32_t>(code_unit);
					code_point_remaining_byte_count = 0;
				}
				else if((code_unit & 0b11100000) == 0b11000000) {
					code_point = static_cast<char32_t>(code_unit & 0b00011111);
					code_point_remaining_byte_count = 1;
				}
				else if((code_unit & 0b11110000) == 0b11100000) {
					code_point = static_cast<char32_t>(code_unit & 0b00001111);
					code_point_remaining_byte_count = 2;
				}
				else if((code_unit & 0b11111000) == 0b11110000) {
					code_point = static_cast<char32_t>(code_unit & 0b00000111);
					code_point_remaining_byte_count = 3;
				}
				else return logo::report_lexer_error("Invalid byte (%) in UTF-8 sequence (index 0).",code_unit);
			}
			else {
				if((code_unit & 0b11000000) != 0b10000000) return logo::report_lexer_error("Invalid byte (%) in UTF-8 sequence (index %).",code_unit,code_point_length);
				code_point = static_cast<char32_t>((code_point << 6) | (code_unit & 0b00111111));
			}

			code_point_length += 1;
			if(code_point_remaining_byte_count == 0) {
				auto [token,status] = logo::process_code_point(code_point);
				switch(status) {
					case Lexing_Status::Error: return Lexing_Status::Error;
					case Lexing_Status::Success: {
						lexer.begin = ptr + 1;
						return token;
					}
					case Lexing_Status::Out_Of_Tokens: logo::unreachable();
				}
				code_point_length = 0;
			}
			else code_point_remaining_byte_count -= 1;
		}
		if(lexer.is_string_literal) {
			return logo::report_lexer_error("Unmatched string literal.");
		}
		return Lexing_Status::Out_Of_Tokens;
	}
}
