#include "lexer.hpp"
#include "debug.hpp"
#include "parser.hpp"
#include "heap_array.hpp"

namespace logo {
	static Ast_Binary_Operator_Type token_type_to_ast_binary_operator_type(Token_Type type) {
		switch(type) {
			case Token_Type::Plus: return Ast_Binary_Operator_Type::Plus;
			case Token_Type::Minus: return Ast_Binary_Operator_Type::Minus;
			case Token_Type::Asterisk: return Ast_Binary_Operator_Type::Multiply;
			case Token_Type::Slash: return Ast_Binary_Operator_Type::Divide;
			case Token_Type::Percent: return Ast_Binary_Operator_Type::Remainder;
			case Token_Type::Caret: return Ast_Binary_Operator_Type::Exponentiate;
			case Token_Type::Logical_And: return Ast_Binary_Operator_Type::Logical_And;
			case Token_Type::Logical_Or: return Ast_Binary_Operator_Type::Logical_Or;
			case Token_Type::Compare_Equal: return Ast_Binary_Operator_Type::Compare_Equal;
			case Token_Type::Compare_Unequal: return Ast_Binary_Operator_Type::Compare_Unequal;
			case Token_Type::Compare_Less_Than: return Ast_Binary_Operator_Type::Compare_Less_Than;
			case Token_Type::Compare_Less_Than_Or_Equal: return Ast_Binary_Operator_Type::Compare_Less_Than_Or_Equal;
			case Token_Type::Compare_Greater_Than: return Ast_Binary_Operator_Type::Compare_Greater_Than;
			case Token_Type::Compare_Greater_Than_Or_Equal: return Ast_Binary_Operator_Type::Compare_Greater_Than_Or_Equal;
			default: logo::unreachable();
		}
	}
	static Ast_Unary_Prefix_Operator_Type token_type_to_ast_unary_prefix_operator_type(Token_Type type) {
		switch(type) {
			case Token_Type::Plus: return Ast_Unary_Prefix_Operator_Type::Plus;
			case Token_Type::Minus: return Ast_Unary_Prefix_Operator_Type::Minus;
			case Token_Type::Logical_Not: return Ast_Unary_Prefix_Operator_Type::Logical_Not;
			default: logo::unreachable();
		}
	}
	static std::size_t get_operator_precedence(Ast_Binary_Operator_Type type) {
		switch(type) {
			case Ast_Binary_Operator_Type::Logical_And: return 4;
			case Ast_Binary_Operator_Type::Logical_Or: return 4;
			case Ast_Binary_Operator_Type::Compare_Equal: return 3;
			case Ast_Binary_Operator_Type::Compare_Unequal: return 3;
			case Ast_Binary_Operator_Type::Compare_Less_Than: return 3;
			case Ast_Binary_Operator_Type::Compare_Less_Than_Or_Equal: return 3;
			case Ast_Binary_Operator_Type::Compare_Greater_Than: return 3;
			case Ast_Binary_Operator_Type::Compare_Greater_Than_Or_Equal: return 3;
			case Ast_Binary_Operator_Type::Plus: return 2;
			case Ast_Binary_Operator_Type::Minus: return 2;
			case Ast_Binary_Operator_Type::Multiply: return 1;
			case Ast_Binary_Operator_Type::Divide: return 1;
			case Ast_Binary_Operator_Type::Remainder: return 1;
			case Ast_Binary_Operator_Type::Exponentiate: return 0;
			default: logo::unreachable();
		}
	}

	void Parsing_Result::destroy() {
		memory.destroy();
	}

	template<typename... Args>
	static void report_parser_error(Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::write_char32_t_to_error_message,"[Syntax error] Line %: ",logo::get_token_line_index());
		logo::format_into(logo::write_char32_t_to_error_message,format,std::forward<Args>(args)...);
		logo::write_char32_t_to_error_message('\n');
	}

	template<typename... Args>
	[[nodiscard]] static Lexing_Result require_next_token(Token_Type type,Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		auto token = logo::get_next_token();
		if(token.status == Lexing_Status::Error) return Lexing_Status::Error;
		if(token.status == Lexing_Status::Out_Of_Tokens) {
			logo::report_parser_error(format,args...);
			return Lexing_Status::Error;
		}
		if(token.token->type != type) {
			logo::report_parser_error(format,args...);
			return Lexing_Status::Error;
		}
		return token;
	}

	enum struct Parsing_Status {
		Continue,
		Error,
		Complete
	};
	struct Expression_State {
		bool empty = true;
		bool complete = false;
		Token_Type last_token_type = Token_Type::None;
	};

	[[nodiscard]] Option<Ast_Value> create_ast_value(Parsing_Result* state,const Token& token) {
		Ast_Value value{};
		if(token.type == Token_Type::Int_Literal) {
			value.type = Ast_Value_Type::Int_Literal;
			value.int_value = token.int_value;
		}
		else if(token.type == Token_Type::Float_Literal) {
			value.type = Ast_Value_Type::Float_Literal;
			value.float_value = token.float_value;
		}
		else if(token.type == Token_Type::Bool_Literal) {
			value.type = Ast_Value_Type::Bool_Literal;
			value.bool_value = token.bool_value;
		}
		else {
			char* string_ptr = state->memory.construct_string(token.string.byte_length());
			if(!string_ptr) {
				Report_Error("Couldn't allocate % bytes of memory.",token.string.byte_length() + 1);
				return {};
			}
			std::memcpy(string_ptr,token.string.begin_ptr,token.string.byte_length());

			if(token.type == Token_Type::String_Literal) {
				value.type = Ast_Value_Type::String_Literal;
				value.string = String_View(string_ptr,token.string.byte_length());
			}
			else if(token.type == Token_Type::Identifier) {
				value.type = Ast_Value_Type::Identifier;
				value.identfier_name = String_View(string_ptr,token.string.byte_length());
			}
			else logo::unreachable();
		}
		return value;
	}

	[[nodiscard]] bool insert_value_into_ast(Parsing_Result* state,Ast_Expression* root,const Token& token) {
		if(root->type == Ast_Expression_Type::None) {
			root->type = Ast_Expression_Type::Value;
			auto [value,has_value] = logo::create_ast_value(state,token);
			if(!has_value) return false;
			root->value = value;
			return true;
		}
		if(root->type == Ast_Expression_Type::Binary_Operator) {
			if(!root->binary_operator->right) {
				root->binary_operator->right = state->memory.construct<Ast_Expression>();
				if(!root->binary_operator->right) {
					Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
					return false;
				}
				root->binary_operator->right->type = Ast_Expression_Type::Value;

				auto [value,has_value] = logo::create_ast_value(state,token);
				if(!has_value) return false;
				root->binary_operator->right->value = value;
				return true;
			}
			return logo::insert_value_into_ast(state,root->binary_operator->right,token);
		}
		if(root->type == Ast_Expression_Type::Unary_Prefix_Operator) {
			if(!root->unary_prefix_operator->child) {
				root->unary_prefix_operator->child = state->memory.construct<Ast_Expression>();
				if(!root->unary_prefix_operator->child) {
					Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
					return false;
				}
				root->unary_prefix_operator->child->type = Ast_Expression_Type::Value;

				auto [value,has_value] = logo::create_ast_value(state,token);
				if(!has_value) return false;
				root->unary_prefix_operator->child->value = value;
				return true;
			}
			return logo::insert_value_into_ast(state,root->unary_prefix_operator->child,token);
		}
		logo::report_parser_error("Unexpected token '%'.",token.string);
		return false;
	}

	[[nodiscard]] bool insert_operator_into_ast(Parsing_Result* state,Ast_Expression* root,const Token& token,Expression_State* expr_state) {
		if(root->type == Ast_Expression_Type::None) {
			root->type = Ast_Expression_Type::Unary_Prefix_Operator;
			if(token.type != Token_Type::Plus && token.type != Token_Type::Minus && token.type != Token_Type::Logical_Not) {
				logo::report_parser_error("Token '%' is not an unary prefix operator.",token.string);
				return false;
			}
			root->unary_prefix_operator = state->memory.construct<Ast_Unary_Prefix_Operator>();
			if(!root->unary_prefix_operator) {
				Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Unary_Prefix_Operator));
				return false;
			}
			root->unary_prefix_operator->type = logo::token_type_to_ast_unary_prefix_operator_type(token.type);
			return true;
		}
		if(logo::is_token_type_literal(expr_state->last_token_type) || expr_state->last_token_type == Token_Type::Identifier || expr_state->last_token_type == Token_Type::Right_Paren) {
			Ast_Binary_Operator binary_operator{};
			binary_operator.type = logo::token_type_to_ast_binary_operator_type(token.type);
			binary_operator.left = state->memory.construct<Ast_Expression>();
			if(!binary_operator.left) {
				Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
				return false;
			}

			while(true) {
				if(root->type == Ast_Expression_Type::Value || root->type == Ast_Expression_Type::Unary_Prefix_Operator) {
					*binary_operator.left = *root;
					root->type = Ast_Expression_Type::Binary_Operator;
					root->binary_operator = state->memory.construct<Ast_Binary_Operator>();
					if(!root->binary_operator) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Binary_Operator));
						return false;
					}
					*root->binary_operator = binary_operator;
					return true;
				}
				if(logo::get_operator_precedence(root->binary_operator->type) <= logo::get_operator_precedence(binary_operator.type)) {
					*binary_operator.left = *root;
					root->type = Ast_Expression_Type::Binary_Operator;
					root->binary_operator = state->memory.construct<Ast_Binary_Operator>();
					if(!root->binary_operator) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Binary_Operator));
						return false;
					}
					*root->binary_operator = binary_operator;
					return true;
				}
				root = root->binary_operator->right;
			}
		}
		if(logo::is_token_type_binary_operator(expr_state->last_token_type)) {
			if(token.type != Token_Type::Plus && token.type != Token_Type::Minus && token.type != Token_Type::Logical_Not) {
				logo::report_parser_error("Token '%' is not an unary prefix operator.",token.string);
				return false;
			}

			Ast_Unary_Prefix_Operator unary_prefix_operator{};
			unary_prefix_operator.type = logo::token_type_to_ast_unary_prefix_operator_type(token.type);
			while(true) {
				if(!root->binary_operator->right) {
					root->binary_operator->right = state->memory.construct<Ast_Expression>();
					if(!root->binary_operator->right) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
						return false;
					}
					root->binary_operator->right->type = Ast_Expression_Type::Unary_Prefix_Operator;
					root->binary_operator->right->unary_prefix_operator = state->memory.construct<Ast_Unary_Prefix_Operator>();
					if(!root->binary_operator->right->unary_prefix_operator) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Unary_Prefix_Operator));
						return false;
					}
					*root->binary_operator->right->unary_prefix_operator = unary_prefix_operator;
					break;
				}
				else root = root->binary_operator->right;
			}
			return true;
		}
		logo::report_parser_error("Unexpected token '%'.",token.string);
		return false;
	}

	[[nodiscard]] bool insert_ast_into_ast(Parsing_Result* state,Ast_Expression* root,const Ast_Expression& new_expr) {
		while(true) {
			if(root->type == Ast_Expression_Type::None) {
				*root = new_expr;
				return true;
			}
			if(root->type == Ast_Expression_Type::Unary_Prefix_Operator) {
				if(!root->unary_prefix_operator->child) {
					root->unary_prefix_operator->child = state->memory.construct<Ast_Expression>();
					if(!root->unary_prefix_operator->child) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
						return false;
					}
					*root->unary_prefix_operator->child = new_expr;
					return true;
				}
			}
			if(root->type == Ast_Expression_Type::Binary_Operator) {
				if(!root->binary_operator->right) {
					root->binary_operator->right = state->memory.construct<Ast_Expression>();
					if(!root->binary_operator->right) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
						return false;
					}
					*root->binary_operator->right = new_expr;
					return true;
				}
				root = root->binary_operator->right;
				continue;
			}
			switch(root->value.type) {
				case Ast_Value_Type::Int_Literal: logo::report_parser_error("Missing a binary operator between '%' and '('.",root->value.int_value); return false;
				case Ast_Value_Type::Float_Literal: logo::report_parser_error("Missing a binary operator between '%' and '('.",root->value.float_value); return false;
				case Ast_Value_Type::Bool_Literal: logo::report_parser_error("Missing a binary operator between '%' and '('.",root->value.bool_value); return false;
				case Ast_Value_Type::String_Literal: logo::report_parser_error("Missing a binary operator between '%' and '('.",root->value.string); return false;
				case Ast_Value_Type::Identifier: logo::report_parser_error("Missing a binary operator between '%' and '('.",root->value.identfier_name); return false;
				default: logo::unreachable();
			}
		}
	}

	[[nodiscard]] Option<Ast_Expression> parse_expression(Parsing_Result* state,bool inside_parenthesis = false) {
		Ast_Expression root_expr{};
		Expression_State expr_state{};
		while(true) {
			auto first_token = logo::peek_next_token(1);
			if(first_token.status == Lexing_Status::Out_Of_Tokens) {
				if(expr_state.empty) logo::report_parser_error("Empty expressions are not allowed.");
				else logo::report_parser_error("Incomplete expression (error message not complete yet).");
				return {};
			}
			if(first_token.token->type == Token_Type::Semicolon || first_token.token->type == Token_Type::Comma || first_token.token->type == Token_Type::Right_Paren) {
				if(expr_state.complete) {
					if(!inside_parenthesis && first_token.token->type == Token_Type::Right_Paren) {
						logo::report_parser_error("Closed parenthesis that was never opened.");
						return {};
					}
					return root_expr;
				}
				char32_t character = '\0';
				switch(first_token.token->type) {
					case Token_Type::Semicolon: character = U';'; break;
					case Token_Type::Comma: character = U','; break;
					case Token_Type::Right_Paren: character = U')'; break;
					default: logo::unreachable();
				}
				logo::report_parser_error("Unexpected token '%'.",character);
				return {};
			}

			expr_state.empty = false;
			first_token = logo::get_next_token();
			switch(first_token.token->type) {
				case Token_Type::Identifier:
				case Token_Type::Int_Literal:
				case Token_Type::Float_Literal:
				case Token_Type::String_Literal:
				case Token_Type::Bool_Literal: {
					if(!logo::insert_value_into_ast(state,&root_expr,*first_token.token)) return {};
					expr_state.complete = true;
					break;
				}
				case Token_Type::Plus:
				case Token_Type::Minus:
				case Token_Type::Asterisk:
				case Token_Type::Slash:
				case Token_Type::Percent:
				case Token_Type::Caret:
				case Token_Type::Logical_And:
				case Token_Type::Logical_Or:
				case Token_Type::Logical_Not:
				case Token_Type::Compare_Equal:
				case Token_Type::Compare_Unequal:
				case Token_Type::Compare_Less_Than:
				case Token_Type::Compare_Less_Than_Or_Equal:
				case Token_Type::Compare_Greater_Than:
				case Token_Type::Compare_Greater_Than_Or_Equal: {
					if(!logo::insert_operator_into_ast(state,&root_expr,*first_token.token,&expr_state)) return {};
					expr_state.complete = false;
					break;
				}
				case Token_Type::Left_Paren: {
					auto [expr_ast,success] = logo::parse_expression(state,true);
					if(!success) return {};
					if(logo::require_next_token(Token_Type::Right_Paren,"Unmatched parenthesis.").status == Lexing_Status::Error) return {};
					if(!logo::insert_ast_into_ast(state,&root_expr,expr_ast)) return {};
					expr_state.complete = true;
					expr_state.last_token_type = Token_Type::Right_Paren;
					continue;
				}
				default: {
					logo::report_parser_error("Invalid token '%'.\n",first_token.token->string);
					return {};
				}
			}
			expr_state.last_token_type = first_token.token->type;
		}
		logo::unreachable();
	}

	static void print_ast_expression(const Ast_Expression& expr,std::size_t depth = 0) {
		for(std::size_t i = 0;i < (depth * 4);i += 1) logo::print(" ");
		switch(expr.type) {
			case Ast_Expression_Type::Value: {
				logo::print("Value: ");
				if(expr.value.type == Ast_Value_Type::Identifier) {
					logo::print("(Identifier) %\n",expr.value.identfier_name);
				}
				else if(expr.value.type == Ast_Value_Type::String_Literal) {
					logo::print("(String) \"%\"\n",expr.value.string);
				}
				else if(expr.value.type == Ast_Value_Type::Int_Literal) {
					logo::print("(Int) %\n",expr.value.int_value);
				}
				else if(expr.value.type == Ast_Value_Type::Float_Literal) {
					logo::print("(Float) %\n",expr.value.float_value);
				}
				else if(expr.value.type == Ast_Value_Type::Bool_Literal) {
					logo::print("(Bool) %\n",expr.value.bool_value);
				}
				break;
			}
			case Ast_Expression_Type::Unary_Prefix_Operator: {
				logo::print("Unary operator: ");
				if(expr.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Plus) {
					logo::print("+\n");
				}
				else if(expr.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Minus) {
					logo::print("-\n");
				}
				else if(expr.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Logical_Not) {
					logo::print("not\n");
				}
				logo::print_ast_expression(*expr.unary_prefix_operator->child,depth + 1);
				break;
			}
			case Ast_Expression_Type::Binary_Operator: {
				logo::print("Binary operator: ");
				if(expr.binary_operator->type == Ast_Binary_Operator_Type::Plus) {
					logo::print("+\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Minus) {
					logo::print("-\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Multiply) {
					logo::print("*\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Divide) {
					logo::print("/\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Remainder) {
					logo::print("%\n","%");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Exponentiate) {
					logo::print("^\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Logical_And) {
					logo::print("and\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Logical_Or) {
					logo::print("or\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Compare_Equal) {
					logo::print("==\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Compare_Unequal) {
					logo::print("!=\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Compare_Less_Than) {
					logo::print("<\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Compare_Less_Than_Or_Equal) {
					logo::print("<=\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Compare_Greater_Than) {
					logo::print(">\n");
				}
				else if(expr.binary_operator->type == Ast_Binary_Operator_Type::Compare_Greater_Than_Or_Equal) {
					logo::print(">=\n");
				}
				logo::print_ast_expression(*expr.binary_operator->left,depth + 1);
				logo::print_ast_expression(*expr.binary_operator->right,depth + 1);
				break;
			}
		}
	}

	[[nodiscard]] static Parsing_Status parse_statement(Parsing_Result* state) {
		auto first_token = logo::peek_next_token(1);
		if(first_token.status == Lexing_Status::Out_Of_Tokens) return Parsing_Status::Complete;

		switch(first_token.token->type) {
			case Token_Type::Semicolon: {
				logo::discard_next_token();
				break;
			}
			case Token_Type::Identifier:
			case Token_Type::String_Literal:
			case Token_Type::Int_Literal:
			case Token_Type::Float_Literal:
			case Token_Type::Bool_Literal:
			case Token_Type::Left_Paren:
			case Token_Type::Plus:
			case Token_Type::Minus:
			case Token_Type::Logical_Not: {
				auto [expr_ast,success] = logo::parse_expression(state);
				if(!success) return Parsing_Status::Error;
				//@TODO: Return this to the caller instead of discarding it after printing it.
				logo::print_ast_expression(expr_ast);
				if(logo::require_next_token(Token_Type::Semicolon,"Expected a semicolon at the end of a statement.").status == Lexing_Status::Error) return Parsing_Status::Error;
				break;
			}
			default: {
				logo::report_parser_error("Invalid token '%'.\n",first_token.token->string);
				return Parsing_Status::Error;
			}
		}
		return Parsing_Status::Continue;
	}

	[[nodiscard]] Option<Parsing_Result> parse_input(Array_View<char> input) {
		if(input.length == 0) {
			logo::report_parser_error("Empty input file.");
			return {};
		}
		if(!logo::init_lexer(input)) {
			return {};
		}
		defer[]{logo::term_lexer();};

		Parsing_Result result{};
		bool successful_return = false;
		defer[&]{if(!successful_return) result.destroy();};

		while(true) {
			auto status = logo::parse_statement(&result);
			if(status == Parsing_Status::Error) return {};
			if(status == Parsing_Status::Complete) break;
		}

		successful_return = true;
		return result;
	}
}
