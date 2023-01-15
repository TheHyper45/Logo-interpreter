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
		None,
		Identifier,
		Int_Literal,
		Float_Literal,
		Bool_Literal,
		String_Literal
	};
	struct Ast_Value {
		Ast_Value_Type type;
		union {
			String_View identfier_name;
			String_View string;
			std::int64_t int_value;
			double float_value;
			bool bool_value;
		};
		Ast_Value() : type(),identfier_name() {}
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
		Ast_Expression() : type(),value() {}
	};

	enum struct Ast_Binary_Operator_Type {
		Plus,
		Minus,
		Multiply,
		Divide,
		Remainder,
		Exponentiate,
		Logical_And,
		Logical_Or,
		Compare_Equal,
		Compare_Unequal,
		Compare_Less_Than,
		Compare_Less_Than_Or_Equal,
		Compare_Greater_Than,
		Compare_Greater_Than_Or_Equal
	};
	struct Ast_Binary_Operator {
		Ast_Binary_Operator_Type type;
		Ast_Expression* left;
		Ast_Expression* right;
	};

	enum struct Ast_Unary_Prefix_Operator_Type {
		Plus,
		Minus,
		Logical_Not
	};
	struct Ast_Unary_Prefix_Operator {
		Ast_Unary_Prefix_Operator_Type type;
		Ast_Expression* child;
	};

	struct Parsing_Result {
		Arena_Allocator memory;
		void destroy();
	};
	[[nodiscard]] Option<Parsing_Result> parse_input(Array_View<char> input);
}

#endif
