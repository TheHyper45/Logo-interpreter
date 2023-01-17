#include <cstdlib>
#include <cstring>
#include "debug.hpp"
#include "lexer.hpp"
#include "heap_array.hpp"
#include "static_array.hpp"

namespace logo {
	enum struct Lexing_Token_Status {
		Not_Important,
		Number_Integer,
		Number_Floating_Point,
		Identifier,
		String_Literal,
		Comment,
		Whitespace
	};
	static struct {
		Heap_Array<char> token_string_bytes;
		std::size_t current_token_string_index;
		char32_t last_token_code_point;
		Lexing_Token_Status token_status;
		bool escape_next_character;
		Heap_Array<Token> tokens;
		std::size_t current_line_index;
		std::size_t current_token_index;
	} lexer;

	bool is_token_type_binary_operator(Token_Type type) {
		switch(type) {
			case Token_Type::Plus:
			case Token_Type::Minus:
			case Token_Type::Asterisk:
			case Token_Type::Slash:
			case Token_Type::Percent:
			case Token_Type::Caret:
			case Token_Type::Logical_And:
			case Token_Type::Logical_Or:
			case Token_Type::Compare_Equal:
			case Token_Type::Compare_Unequal:
			case Token_Type::Compare_Less_Than:
			case Token_Type::Compare_Less_Than_Or_Equal:
			case Token_Type::Compare_Greater_Than:
			case Token_Type::Compare_Greater_Than_Or_Equal:
				return true;
			default: return false;
		}
	}
	bool is_token_type_literal(Token_Type type) {
		switch(type) {
			case Token_Type::Int_Literal:
			case Token_Type::Float_Literal:
			case Token_Type::Bool_Literal:
			case Token_Type::String_Literal:
				return true;
			default: return false;
		}
	}
	bool is_token_type_value_like(Token_Type type) {
		return logo::is_token_type_literal(type) || type == Token_Type::Identifier;
	}
	bool is_token_type_assignment(Token_Type type) {
		switch(type) {
			case Token_Type::Equals_Sign:
			case Token_Type::Compound_Plus:
			case Token_Type::Compound_Minus:
			case Token_Type::Compound_Multiply:
			case Token_Type::Compound_Divide:
			case Token_Type::Compound_Remainder:
			case Token_Type::Compound_Exponentiate:
				return true;
			default: return false;
		}
	}

	template<typename... Args>
	static void report_lexer_error(Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::write_char32_t_to_error_message,"[Lexer error] Line %: ",lexer.current_line_index);
		logo::format_into(logo::write_char32_t_to_error_message,format,std::forward<Args>(args)...);
		logo::write_char32_t_to_error_message('\n');
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
			code_point == '{' || code_point == '}' || code_point == ',' || code_point == '<' ||
			code_point == '>' || code_point == '+' || code_point == '-' || code_point == '*' ||
			code_point == '/' || code_point == '^' || code_point == '!' || code_point == ';' ||
			code_point == '\'' || code_point == ':' || code_point == '%';
	}

	static bool append_code_point_to_token(char32_t code_point) {
		auto bytes = logo::make_code_units(code_point);
		logo::assert(lexer.token_string_bytes.push_back({bytes.data,bytes.length}));
		lexer.last_token_code_point = code_point;
		return true;
	}

	static bool finish_token() {
		if(lexer.last_token_code_point == '\0') {
			return true;
		}
		auto null_bytes = logo::make_code_units('\0');
		logo::assert(lexer.token_string_bytes.push_back({null_bytes.data,null_bytes.length}));

		Token token{};
		token.string = String_View(&lexer.token_string_bytes[lexer.current_token_string_index],(lexer.token_string_bytes.length - 1) - lexer.current_token_string_index);
		token.line_index = lexer.current_line_index;
		token.type = Token_Type::None;
		if(lexer.token_status == Lexing_Token_Status::Number_Integer) {
			char* end_ptr = nullptr;
			token.int_value = std::strtoll(token.string.begin_ptr,&end_ptr,10);
			if(token.string.begin_ptr == end_ptr) {
				Report_Error("Couldn't convert '%' to a 64 bit integer.",token.string);
				return false;
			}
			token.type = Token_Type::Int_Literal;
		}
		else if(lexer.token_status == Lexing_Token_Status::Number_Floating_Point) {
			char* end_ptr = nullptr;
			token.float_value = std::strtod(token.string.begin_ptr,&end_ptr);
			if(token.string.begin_ptr == end_ptr) {
				Report_Error("Couldn't convert '%' to a double float.",token.string);
				return false;
			}
			token.type = Token_Type::Float_Literal;
		}
		else if(std::strcmp(token.string.begin_ptr,"if") == 0) {
			token.type = Token_Type::Keyword_If;
		}
		else if(std::strcmp(token.string.begin_ptr,"for") == 0) {
			token.type = Token_Type::Keyword_For;
		}
		else if(std::strcmp(token.string.begin_ptr,"while") == 0) {
			token.type = Token_Type::Keyword_While;
		}
		else if(std::strcmp(token.string.begin_ptr,"let") == 0) {
			token.type = Token_Type::Keyword_Let;
		}
		else if(std::strcmp(token.string.begin_ptr,"return") == 0) {
			token.type = Token_Type::Keyword_Return;
		}
		else if(std::strcmp(token.string.begin_ptr,"break") == 0) {
			token.type = Token_Type::Keyword_Break;
		}
		else if(std::strcmp(token.string.begin_ptr,"continue") == 0) {
			token.type = Token_Type::Keyword_Continue;
		}
		else if(std::strcmp(token.string.begin_ptr,"func") == 0) {
			token.type = Token_Type::Keyword_Func;
		}
		else if(std::strcmp(token.string.begin_ptr,"and") == 0) {
			token.type = Token_Type::Logical_And;
		}
		else if(std::strcmp(token.string.begin_ptr,"or") == 0) {
			token.type = Token_Type::Logical_Or;
		}
		else if(std::strcmp(token.string.begin_ptr,"not") == 0) {
			token.type = Token_Type::Logical_Not;
		}
		else if(std::strcmp(token.string.begin_ptr,"true") == 0) {
			token.type = Token_Type::Bool_Literal;
			token.bool_value = true;
		}
		else if(std::strcmp(token.string.begin_ptr,"false") == 0) {
			token.type = Token_Type::Bool_Literal;
			token.bool_value = false;
		}
		else if(lexer.token_status == Lexing_Token_Status::Identifier) {
			token.type = Token_Type::Identifier;
		}
		else if(lexer.token_status == Lexing_Token_Status::String_Literal) {
			token.type = Token_Type::String_Literal;
		}
		else if(lexer.token_status == Lexing_Token_Status::Whitespace) {
			token.type = Token_Type::Whitespace;
		}
		else if(lexer.token_status == Lexing_Token_Status::Comment) {
			token.type = Token_Type::Comment;
		}
		else if(std::strcmp(token.string.begin_ptr,"==") == 0) {
			token.type = Token_Type::Compare_Equal;
		}
		else if(std::strcmp(token.string.begin_ptr,"!=") == 0) {
			token.type = Token_Type::Compare_Unequal;
		}
		else if(std::strcmp(token.string.begin_ptr,"<=") == 0) {
			token.type = Token_Type::Compare_Less_Than_Or_Equal;
		}
		else if(std::strcmp(token.string.begin_ptr,">=") == 0) {
			token.type = Token_Type::Compare_Greater_Than_Or_Equal;
		}
		else if(std::strcmp(token.string.begin_ptr,"+=") == 0) {
			token.type = Token_Type::Compound_Plus;
		}
		else if(std::strcmp(token.string.begin_ptr,"-=") == 0) {
			token.type = Token_Type::Compound_Minus;
		}
		else if(std::strcmp(token.string.begin_ptr,"*=") == 0) {
			token.type = Token_Type::Compound_Multiply;
		}
		else if(std::strcmp(token.string.begin_ptr,"/=") == 0) {
			token.type = Token_Type::Compound_Divide;
		}
		else if(std::strcmp(token.string.begin_ptr,"%=") == 0) {
			token.type = Token_Type::Compound_Remainder;
		}
		else if(std::strcmp(token.string.begin_ptr,"^=") == 0) {
			token.type = Token_Type::Compound_Exponentiate;
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
		else if(lexer.last_token_code_point == '%') {
			token.type = Token_Type::Percent;
		}
		else if(lexer.last_token_code_point == '^') {
			token.type = Token_Type::Caret;
		}
		else if(lexer.last_token_code_point == '=') {
			token.type = Token_Type::Equals_Sign;
		}
		else if(lexer.last_token_code_point == '<') {
			token.type = Token_Type::Compare_Less_Than;
		}
		else if(lexer.last_token_code_point == '>') {
			token.type = Token_Type::Compare_Greater_Than;
		}
		if(!lexer.tokens.push_back(token)) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(token));
			return false;
		}
		lexer.last_token_code_point = '\0';
		lexer.current_token_string_index = lexer.token_string_bytes.length;
		lexer.token_status = Lexing_Token_Status::Not_Important;
		return true;
	}

	static bool finish_token_then_append(char32_t code_point) {
		if(!logo::finish_token()) {
			return false;
		}
		if(code_point == '\"') {
			lexer.token_status = Lexing_Token_Status::String_Literal;
			return true;
		}
		auto bytes = logo::make_code_units(code_point);
		logo::assert(lexer.token_string_bytes.push_back({bytes.data,bytes.length}));
		lexer.last_token_code_point = code_point;
		if(logo::is_code_point_alpha(code_point)) {
			lexer.token_status = Lexing_Token_Status::Identifier;
		}
		else if(logo::is_code_point_digit(code_point)) {
			lexer.token_status = Lexing_Token_Status::Number_Integer;
		}
		else if(logo::is_code_point_whitespace(code_point)) {
			lexer.token_status = Lexing_Token_Status::Whitespace;
		}
		else if(code_point == '#') {
			lexer.token_status = Lexing_Token_Status::Comment;
		}
		else if(code_point == '\"') {
			lexer.token_status = Lexing_Token_Status::String_Literal;
		}
		return true;
	}

	static bool process_code_point(char32_t code_point) {
		if(lexer.last_token_code_point == '\0' && lexer.token_status != Lexing_Token_Status::String_Literal) {
			if(logo::is_code_point_alpha(code_point)) {
				lexer.token_status = Lexing_Token_Status::Identifier;
			}
			else if(logo::is_code_point_digit(code_point)) {
				lexer.token_status = Lexing_Token_Status::Number_Integer;
			}
			else if(logo::is_code_point_whitespace(code_point)) {
				lexer.token_status = Lexing_Token_Status::Whitespace;
			}
			else if(code_point == '#') {
				lexer.token_status = Lexing_Token_Status::Comment;
			}
			else if(code_point == '\"') {
				lexer.token_status = Lexing_Token_Status::String_Literal;
				return true;
			}
			return logo::append_code_point_to_token(code_point);
		}
		if(lexer.token_status == Lexing_Token_Status::Comment) {
			if(code_point == '\n') {
				return logo::finish_token_then_append(code_point);
			}
			return logo::append_code_point_to_token(code_point);
		}
		if(lexer.token_status == Lexing_Token_Status::String_Literal) {
			if(lexer.escape_next_character) {
				if(code_point == 'n') {
					code_point = '\n';
				}
				else if(code_point != '\"' && code_point != '\\') {
					logo::report_lexer_error("Invalid escape sequence \"\\%\" in a string literal.",code_point);
					return false;
				}
				lexer.escape_next_character = false;
				return logo::append_code_point_to_token(code_point);
			}
			if(code_point == '\\') {
				lexer.escape_next_character = true;
				return true;
			}
			if(code_point == '\"') {
				return logo::finish_token();
			}
			return logo::append_code_point_to_token(code_point);
		}
		if(lexer.token_status == Lexing_Token_Status::Identifier) {
			if(logo::is_code_point_alpha(code_point) || logo::is_code_point_digit(code_point)) {
				return logo::append_code_point_to_token(code_point);
			}
			return logo::finish_token_then_append(code_point);
		}
		if(lexer.token_status == Lexing_Token_Status::Number_Integer || lexer.token_status == Lexing_Token_Status::Number_Floating_Point) {
			if(logo::is_code_point_digit(code_point)) {
				return logo::append_code_point_to_token(code_point);
			}
			if(code_point == '.' && lexer.token_status == Lexing_Token_Status::Number_Integer) {
				lexer.token_status = Lexing_Token_Status::Number_Floating_Point;
				return logo::append_code_point_to_token(code_point);
			}
			return logo::finish_token_then_append(code_point);
		}
		if(lexer.token_status == Lexing_Token_Status::Whitespace) {
			if(logo::is_code_point_whitespace(code_point)) {
				return logo::append_code_point_to_token(code_point);
			}
			return logo::finish_token_then_append(code_point);
		}
		if(lexer.last_token_code_point == '.') {
			if(lexer.token_status == Lexing_Token_Status::Number_Floating_Point) {
				return logo::append_code_point_to_token(code_point);
			}
			return logo::finish_token_then_append(code_point);
		}
		if(logo::is_one_of(lexer.last_token_code_point,U'+',U'-',U'*',U'/',U'^',U'%',U'=',U'!',U'<',U'>')) {
			if(code_point == '=') {
				return logo::append_code_point_to_token(code_point);
			}
			return logo::finish_token_then_append(code_point);
		}
		return logo::finish_token_then_append(code_point);
	}

	bool init_lexer(Array_View<char> input) {
		lexer = {};
		lexer.current_line_index = 1;
		if(!lexer.token_string_bytes.reserve(input.length * 2)) {
			Report_Error("Couldn't allocate % bytes of memory.",input.length * 2);
			return false;
		}
		bool successful_return = false;
		defer[&]{ if(!successful_return) logo::term_lexer(); };

		char32_t current_code_point = '\0';
		std::size_t remaining_code_point_byte_count = 0;
		for(char byte : input) {
			if(current_code_point == '\0') {
				if((byte & 0b10000000) == 0) {
					current_code_point = static_cast<char32_t>(byte);
					remaining_code_point_byte_count = 0;
				}
				else if((byte & 0b11100000) == 0b11000000) {
					current_code_point = static_cast<char32_t>(byte & 0b00011111);
					remaining_code_point_byte_count = 1;
				}
				else if((byte & 0b11110000) == 0b11100000) {
					current_code_point = static_cast<char32_t>(byte & 0b00001111);
					remaining_code_point_byte_count = 2;
				}
				else if((byte & 0b11111000) == 0b11110000) {
					current_code_point = static_cast<char32_t>(byte & 0b00000111);
					remaining_code_point_byte_count = 3;
				}
				else {
					logo::report_lexer_error("Invalid byte (%) in an UTF-8 sequence.",byte);
					return false;
				}
			}
			else {
				remaining_code_point_byte_count -= 1;
				if((byte & 0b11000000) != 0b10000000) {
					logo::report_lexer_error("Invalid byte (%) in an UTF-8 sequence.",byte);
					return false;
				}
				current_code_point = static_cast<char32_t>((current_code_point << 6) | (byte & 0b00111111));
			}
			if(remaining_code_point_byte_count == 0) {
				if(current_code_point == '\0') {
					logo::report_lexer_error("Null bytes are not allowed.");
					return false;
				}
				if(!logo::process_code_point(current_code_point)) {
					return false;
				}
				current_code_point = '\0';
			}
		}
		if(current_code_point != '\0') {
			if(!logo::process_code_point(current_code_point)) {
				return false;
			}
		}
		if(lexer.token_status == Lexing_Token_Status::String_Literal) {
			logo::report_lexer_error("Unmatched string literal.");
			return false;
		}
		if(!logo::finish_token()) {
			return false;
		}
		lexer.current_line_index = 1;
		successful_return = true;
		return true;
	}

	void term_lexer() {
		lexer.tokens.destroy();
		lexer.token_string_bytes.destroy();
	}

	Lexing_Result get_next_token() {
		while(true) {
			if(lexer.current_token_index >= lexer.tokens.length) return Lexing_Status::Out_Of_Tokens;
			const Token& token = lexer.tokens[lexer.current_token_index++];
			if(token.type != Token_Type::Comment && token.type != Token_Type::Whitespace && token.type != Token_Type::Newline) {
				lexer.current_line_index = token.line_index;
				return token;
			}
		}
	}

	void discard_next_token() {
		(void) get_next_token();
	}

	Lexing_Result peek_next_token(std::size_t count) {
		auto current_token_index = lexer.current_token_index;
		std::size_t index = 0;
		while(true) {
			if(current_token_index >= lexer.tokens.length) return Lexing_Status::Out_Of_Tokens;
			const Token& token = lexer.tokens[current_token_index++];
			if(token.type != Token_Type::Comment && token.type != Token_Type::Whitespace && token.type != Token_Type::Newline) {
				index += 1;
				if(index >= count) {
					lexer.current_line_index = token.line_index;
					return token;
				}
			}
		}
	}

	std::size_t get_token_line_index() {
		return lexer.current_line_index;
	}
}
