#ifndef LOGO_INTERPRETER_HPP
#define LOGO_INTERPRETER_HPP

#include "array_view.hpp"

namespace logo {
	struct Ast_Statement;
	bool interpret_ast(Array_View<Ast_Statement> statements);
}

#endif
