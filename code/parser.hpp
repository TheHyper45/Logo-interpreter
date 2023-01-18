#ifndef LOGO_PARSER_HPP
#define LOGO_PARSER_HPP

#include "utils.hpp"
#include "string.hpp"
#include "heap_array.hpp"
#include "array_view.hpp"
#include "memory_arena.hpp"

namespace logo {
	struct Ast_Binary_Operator;
	struct Ast_Unary_Prefix_Operator;
	struct Ast_Function_Call;
	struct Ast_Statement;

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
		std::size_t line_index;
		union {
			String_View identfier_name;
			String_View string_value;
			std::int64_t int_value;
			double float_value;
			bool bool_value;
		};
		Ast_Value() : type(),line_index(),identfier_name() {}
	};

	enum struct Ast_Expression_Type {
		None,
		Value,
		Binary_Operator,
		Unary_Prefix_Operator,
		Function_Call
	};
	struct Ast_Expression {
		Ast_Expression_Type type;
		union {
			Ast_Value value;
			Ast_Binary_Operator* binary_operator;
			Ast_Unary_Prefix_Operator* unary_prefix_operator;
			Ast_Function_Call* function_call;
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
		std::size_t line_index;
	};

	enum struct Ast_Unary_Prefix_Operator_Type {
		Plus,
		Minus,
		Logical_Not
	};
	struct Ast_Unary_Prefix_Operator {
		Ast_Unary_Prefix_Operator_Type type;
		Ast_Expression* child;
		std::size_t line_index;
	};

	struct Ast_Function_Call {
		String_View name;
		Heap_Array<Ast_Expression*> arguments;
		std::size_t line_index;
	};

	enum struct Ast_Assignment_Type {
		Assignment,
		Compound_Plus,
		Compound_Minus,
		Compound_Multiply,
		Compound_Divide,
		Compound_Remainder,
		Compound_Exponentiate
	};
	struct Ast_Assignment {
		Ast_Assignment_Type type;
		String_View name;
		Ast_Expression value_expr;
		std::size_t line_index;
	};

	struct Ast_Declaration {
		String_View name;
		Ast_Expression initial_value_expr;
	};

	struct Ast_If_Statement {
		Ast_Expression condition_expr;
		Heap_Array<Ast_Statement> if_true_statements;
		Heap_Array<Ast_Statement> if_false_statements;
	};

	struct Ast_While_Statement {
		Ast_Expression condition_expr;
		Heap_Array<Ast_Statement> body_statements;
	};

	enum struct Ast_Statement_Type {
		Expression,
		Declaration,
		Assignment,
		If_Statement,
		While_Statement,
		Break_Stetement,
		Continue_Statement
	};
	struct Ast_Statement {
		Ast_Statement_Type type;
		union {
			Ast_Expression expression;
			Ast_Declaration declaration;
			Ast_Assignment assignment;
			Ast_If_Statement if_statement;
			Ast_While_Statement while_statement;
		};
		std::size_t line_index;
		Ast_Statement() : type(),expression(),line_index() {}
	};

	struct Parsing_Result {
		Arena_Allocator memory;
		Heap_Array<Ast_Statement> statements;
		void destroy();
	};
	[[nodiscard]] Option<Parsing_Result> parse_input(Array_View<char> input);
}

#endif
