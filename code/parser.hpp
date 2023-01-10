#ifndef LOGO_PARSER_HPP
#define LOGO_PARSER_HPP

#include "utils.hpp"
#include "heap_array.hpp"
#include "array_view.hpp"
#include "memory_arena.hpp"

namespace logo {
	struct Ast_Binary_Operator;
	struct Ast_Unary_Prefix_Operator;

	enum struct Ast_Value_Type {
		Number_Literal,
		Identifier
	};
	struct Ast_Value {
		Ast_Value_Type type;
		union {
			String_View identfier_name;
			double number;
		};
	};

	enum struct Ast_Expression_Type {
		None,
		Value,
		Binary_Operator,
		Unary_Prefix_Operator
	};
	struct Ast_Expression {
		Ast_Expression_Type type;
		union {
			Ast_Value value;
			Ast_Binary_Operator* binary_operator;
			Ast_Unary_Prefix_Operator* unary_prefix_operator;
		};
	};

	enum struct Ast_Binary_Operator_Type {
		Plus,
		Minus
	};
	struct Ast_Binary_Operator {
		Ast_Binary_Operator_Type type;
		Ast_Expression* left;
		Ast_Expression* right;
	};

	enum struct Ast_Unary_Prefix_Operator_Type {
		Plus,
		Minus
	};
	struct Ast_Unary_Prefix_Operator {
		Ast_Unary_Prefix_Operator_Type type;
		Ast_Expression* expr;
	};

	struct Parsing_Result {
		Arena_Allocator memory;
		void destroy();
	};
	[[nodiscard]] Option<Parsing_Result> parse_input(Array_View<char> input);
}

#endif
