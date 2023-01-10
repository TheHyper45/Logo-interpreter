#ifndef LOGO_LEXER_HPP
#define LOGO_LEXER_HPP

#include "string.hpp"

namespace logo {
	enum struct Token_Type {
		None,
		Whitespace,
		Comment,
		Identifier,
		Number_Literal,
		String_Literal,
		Newline,
		Left_Paren,
		Right_Paren,
		Left_Bracket,
		Right_Bracket,
		Left_Brace,
		Right_Brace,
		Comma,
		Semicolon,
		Colon,
		Equals_Sign,
		Compare_Equal,
		Compare_Unequal,
		Compare_Less_Than,
		Compare_Less_Than_Or_Equal,
		Compare_Greater_Than,
		Compare_Greater_Than_Or_Equal,
		Logical_And,
		Logical_Or,
		Logical_Not,
		Plus,
		Minus,
		Asterisk,
		Slash,
		Percent,
		Caret,
		Compound_Plus,
		Compound_Minus,
		Compound_Multiply,
		Compound_Divide,
		Compound_Remainder,
		Compound_Exponentiate,
		Keyword_If,
		Keyword_While,
		Keyword_For,
		Keyword_Let,
		Keyword_Break,
		Keyword_Continue,
		Keyword_Return
	};
	[[nodiscard]] std::size_t get_operator_precedence(Token_Type type);

	struct Token {
		Token_Type type;
		String_View string;
		std::size_t line_index;
		double number_value;
	};

	enum struct Lexing_Status {
		Success,
		Error,
		Out_Of_Tokens
	};
	struct Lexing_Result {
		const Token* token;
		Lexing_Status status;
		Lexing_Result(const Token& _token) : token(&_token),status(Lexing_Status::Success) {}
		Lexing_Result(Lexing_Status _status) : token(),status(_status) {}
	};

	bool init_lexer(Array_View<char> input);
	void term_lexer();
	[[nodiscard]] Lexing_Result get_next_token();
	[[nodiscard]] Lexing_Result peek_next_token();
	[[nodiscard]] std::size_t get_token_line_index();
}

#endif
