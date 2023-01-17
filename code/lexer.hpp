#ifndef LOGO_LEXER_HPP
#define LOGO_LEXER_HPP

#include <cstddef>
#include "string.hpp"

namespace logo {
	enum struct Token_Type {
		None,
		Whitespace,
		Comment,
		Identifier,
		Int_Literal,
		Float_Literal,
		String_Literal,
		Bool_Literal,
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
		Keyword_Else,
		Keyword_While,
		Keyword_For,
		Keyword_Let,
		Keyword_Break,
		Keyword_Continue,
		Keyword_Return,
		Keyword_Func
	};
	[[nodiscard]] bool is_token_type_binary_operator(Token_Type type);
	[[nodiscard]] bool is_token_type_literal(Token_Type type);
	[[nodiscard]] bool is_token_type_value_like(Token_Type type);
	[[nodiscard]] bool is_token_type_assignment(Token_Type type);

	struct Token {
		Token_Type type;
		String_View string;
		std::size_t line_index;
		std::int64_t int_value;
		double float_value;
		bool bool_value;
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
	void discard_next_token();
	[[nodiscard]] Lexing_Result peek_next_token(std::size_t count);
	[[nodiscard]] std::size_t get_token_line_index();
}

#endif
