#ifndef LOGO_PARSER_HPP
#define LOGO_PARSER_HPP

#include "array_view.hpp"

namespace logo {
	enum struct Binary_Operator_Type {
		Plus,
		Minus,
		Multiply,
		Divide,
		Exponentiate,
		Logical_And,
		Logical_Or,
		Logical_Not
	};
	struct Binary_Operator {
		Binary_Operator_Type type;
	};

	enum struct Unary_Operator {

	};

	struct Function_Call {

	};

	bool parse_input(Array_View<char> input);
}

#endif
