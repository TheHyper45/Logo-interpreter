#include <climits>
#include "utils.hpp"
#include "debug.hpp"
#include "parser.hpp"
#include "heap_array.hpp"
#include "interpreter.hpp"
#include "memory_arena.hpp"
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
	#define PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
	#undef near
	#undef far
#else
	#include <fcntl.h>
	#include <unistd.h>
	#include <sys/stat.h>
#endif

static_assert(CHAR_BIT == 8);

namespace logo {
	[[nodiscard]] static Option<Heap_Array<char>> read_file(String_View path) {
#ifdef PLATFORM_WINDOWS
		HANDLE file = CreateFileA(path.begin_ptr,GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if(file == INVALID_HANDLE_VALUE) {
			Report_Error("File \"%\" couldn't be opened.",path);
			return {};
		}
		defer[&]{CloseHandle(file);};

		std::size_t file_size = 0;
		{
			LARGE_INTEGER raw_file_size{};
			if(!GetFileSizeEx(file,&raw_file_size)) {
				Report_Error("Couldn't obtain the size of file \"%\".",path);
				return {};
			}
			file_size = static_cast<std::size_t>(raw_file_size.QuadPart);
			if(file_size > MAXDWORD) {
				Report_Error("File \"%\" is too big (max % bytes).",path,MAXDWORD);
				return {};
			}
		}

		Heap_Array<char> bytes{};
		if(!bytes.resize(file_size)) {
			Report_Error("Couldn't allocate % bytes of memory.",file_size);
			return {};
		}
		DWORD read_bytes{};
		if(!ReadFile(file,bytes.data,static_cast<DWORD>(file_size),&read_bytes,nullptr) || read_bytes != file_size) {
			bytes.destroy();
			Report_Error("Couldn't read data from file \"%\".",path);
			return {};
		}
		return bytes;
#else
		int file = open64(path.begin_ptr,O_RDONLY);
		if(file == -1) {
			Report_Error("File \"%\" couldn't be opened.",path);
			return {};
		}
		defer[&]{close(file);};

		struct stat64 file_stat{};
		if(fstat64(file,&file_stat) == -1) {
			Report_Error("Couldn't obtain the size of file \"%\".",path);
			return {};
		}

		Heap_Array<char> bytes{};
		if(!bytes.resize(file_stat.st_size)) {
			Report_Error("Couldn't allocate % bytes of memory.",static_cast<std::size_t>(file_stat.st_size));
			return {};
		}
		if(read(file,bytes.data,file_stat.st_size) != file_stat.st_size) {
			bytes.destroy();
			Report_Error("Couldn't read data from file \"%\".",path);
			return {};
		}
		return bytes;
#endif
	}

	static void print_n_spaces(std::size_t count) {
		for(std::size_t i = 0;i < (count * 4);i += 1) logo::print(" ");
	}

	static void print_ast_expression(const Ast_Expression& expression,std::size_t depth = 0) {
		logo::print_n_spaces(depth);
		switch(expression.type) {
			case Ast_Expression_Type::Value: {
				logo::print("Value: ");
				if(expression.value.type == Ast_Value_Type::Identifier) {
					logo::print("(Identifier) %\n",expression.value.identfier_name);
				}
				else if(expression.value.type == Ast_Value_Type::String_Literal) {
					logo::print("(String) \"%\"\n",expression.value.string_value);
				}
				else if(expression.value.type == Ast_Value_Type::Int_Literal) {
					logo::print("(Int) %\n",expression.value.int_value);
				}
				else if(expression.value.type == Ast_Value_Type::Float_Literal) {
					logo::print("(Float) %\n",expression.value.float_value);
				}
				else if(expression.value.type == Ast_Value_Type::Bool_Literal) {
					logo::print("(Bool) %\n",expression.value.bool_value);
				}
				break;
			}
			case Ast_Expression_Type::Unary_Prefix_Operator: {
				logo::print("Unary operator: ");
				if(expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Plus) {
					logo::print("+\n");
				}
				else if(expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Minus) {
					logo::print("-\n");
				}
				else if(expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Logical_Not) {
					logo::print("not\n");
				}
				logo::print_ast_expression(*expression.unary_prefix_operator->child,depth + 1);
				break;
			}
			case Ast_Expression_Type::Binary_Operator: {
				logo::print("Binary operator: ");
				if(expression.binary_operator->type == Ast_Binary_Operator_Type::Plus) {
					logo::print("+\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Minus) {
					logo::print("-\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Multiply) {
					logo::print("*\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Divide) {
					logo::print("/\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Remainder) {
					logo::print("%\n","%");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Exponentiate) {
					logo::print("^\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Logical_And) {
					logo::print("and\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Logical_Or) {
					logo::print("or\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Compare_Equal) {
					logo::print("==\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Compare_Unequal) {
					logo::print("!=\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Compare_Less_Than) {
					logo::print("<\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Compare_Less_Than_Or_Equal) {
					logo::print("<=\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Compare_Greater_Than) {
					logo::print(">\n");
				}
				else if(expression.binary_operator->type == Ast_Binary_Operator_Type::Compare_Greater_Than_Or_Equal) {
					logo::print(">=\n");
				}
				logo::print_ast_expression(*expression.binary_operator->left,depth + 1);
				logo::print_ast_expression(*expression.binary_operator->right,depth + 1);
				break;
			}
			case Ast_Expression_Type::Function_Call: {
				auto arg_count = expression.function_call->arguments.length;
				logo::print("Function call % (% %):\n",expression.function_call->name,arg_count,(arg_count == 1) ? "arg" : "args");
				for(const auto* arg_expr : expression.function_call->arguments) {
					logo::print_ast_expression(*arg_expr,depth + 1);
				}
				break;
			}
			default: logo::unreachable();
		}
	}

	static void print_ast_statement(const Ast_Statement& statement,std::size_t depth = 0) {
		logo::print_n_spaces(depth);
		switch(statement.type) {
			case Ast_Statement_Type::Break_Stetement: logo::print("Break statement\n"); break;
			case Ast_Statement_Type::Continue_Statement: logo::print("Continue statement\n"); break;
			case Ast_Statement_Type::Declaration: {
				logo::print("Declaration % =\n",statement.declaration.name);
				logo::print_ast_expression(statement.declaration.initial_value_expr,depth + 1);
				break;
			}
			case Ast_Statement_Type::Assignment: {
				logo::print("Assignment % ",statement.assignment.name);
				switch(statement.assignment.type) {
					case Ast_Assignment_Type::Assignment: logo::print("=\n"); break;
					case Ast_Assignment_Type::Compound_Plus: logo::print("+=\n"); break;
					case Ast_Assignment_Type::Compound_Minus: logo::print("-=\n"); break;
					case Ast_Assignment_Type::Compound_Multiply: logo::print("*=\n"); break;
					case Ast_Assignment_Type::Compound_Divide: logo::print("/=\n"); break;
					case Ast_Assignment_Type::Compound_Remainder: logo::print("%=\n","%"); break;
					case Ast_Assignment_Type::Compound_Exponentiate: logo::print("^=\n"); break;
					default: logo::unreachable();
				}
				logo::print_ast_expression(statement.assignment.value_expr,depth + 1);
				break;
			}
			case Ast_Statement_Type::If_Statement: {
				logo::print("If\n");
				logo::print_ast_expression(statement.if_statement.condition_expr,depth + 1);
				if(statement.if_statement.if_true_statements.length > 0) {
					logo::print_n_spaces(depth);
					logo::print("Then\n");
					for(const auto& inner_statement : statement.if_statement.if_true_statements) {
						logo::print_ast_statement(inner_statement,depth + 1);
					}
				}
				if(statement.if_statement.if_false_statements.length > 0) {
					logo::print_n_spaces(depth);
					logo::print("Else\n");
					for(const auto& inner_statement : statement.if_statement.if_false_statements) {
						logo::print_ast_statement(inner_statement,depth + 1);
					}
				}
				break;
			}
			case Ast_Statement_Type::While_Statement: {
				logo::print("While\n");
				logo::print_ast_expression(statement.while_statement.condition_expr,depth + 1);
				if(statement.while_statement.body_statements.length > 0) {
					logo::print_n_spaces(depth);
					logo::print("Repeat\n");
					for(const auto& inner_statement : statement.while_statement.body_statements) {
						logo::print_ast_statement(inner_statement,depth + 1);
					}
				}
				break;
			}
			case Ast_Statement_Type::Expression: {
				logo::print("Expression\n");
				logo::print_ast_expression(statement.expression,depth + 1);
				break;
			}
			default: logo::unreachable();
		}
	}
}

int main() {
	if(!logo::debug_init()) {
		logo::eprint("%\n",logo::get_reported_error());
		return 1;
	}
	defer[]{logo::debug_term();};

	auto [file_bytes,file_opened] = logo::read_file("./script0.txt");
	if(!file_opened) {
		logo::eprint("%\n",logo::get_reported_error());
		return 1;
	}
	defer[&]{file_bytes.destroy();};

	auto [parsing_result,parsing_successful] = logo::parse_input({file_bytes.data,file_bytes.length});
	if(!parsing_successful) {
		logo::eprint("%\n",logo::get_reported_error());
		return 1;
	}
	defer[&]{parsing_result.destroy();};

	/*for(const auto& statement : parsing_result.statements) {
		logo::print_ast_statement(statement);
	}*/

	if(!logo::interpret_ast({parsing_result.statements.data,parsing_result.statements.length})) {
		logo::eprint("%\n",logo::get_reported_error());
		return 1;
	}
	return 0;
}
