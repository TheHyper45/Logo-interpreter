#include "lexer.hpp"
#include "debug.hpp"
#include "parser.hpp"
#include "heap_array.hpp"

namespace logo {
	static [[nodiscard]] Ast_Binary_Operator_Type token_type_to_ast_binary_operator_type(Token_Type type) {
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
	static [[nodiscard]] Ast_Unary_Prefix_Operator_Type token_type_to_ast_unary_prefix_operator_type(Token_Type type) {
		switch(type) {
			case Token_Type::Plus: return Ast_Unary_Prefix_Operator_Type::Plus;
			case Token_Type::Minus: return Ast_Unary_Prefix_Operator_Type::Minus;
			case Token_Type::Logical_Not: return Ast_Unary_Prefix_Operator_Type::Logical_Not;
			default: logo::unreachable();
		}
	}
	static [[nodiscard]] std::size_t get_operator_precedence(Ast_Binary_Operator_Type type) {
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
	static [[nodiscard]] Ast_Assignment_Type token_type_to_ast_assignment_type(Token_Type type) {
		switch(type) {
			case Token_Type::Equals_Sign: return Ast_Assignment_Type::Assignment;
			case Token_Type::Compound_Plus: return Ast_Assignment_Type::Compound_Plus;
			case Token_Type::Compound_Minus: return Ast_Assignment_Type::Compound_Minus;
			case Token_Type::Compound_Multiply: return Ast_Assignment_Type::Compound_Multiply;
			case Token_Type::Compound_Divide: return Ast_Assignment_Type::Compound_Divide;
			case Token_Type::Compound_Remainder: return Ast_Assignment_Type::Compound_Remainder;
			case Token_Type::Compound_Exponentiate: return Ast_Assignment_Type::Compound_Exponentiate;
			default: logo::unreachable();
		}
	}

	void Parsing_Result::destroy() {
		//@TODO: Clean up memory from statements.
		logo::eprint("!!! MEMORY LEAK !!!\n");
		statements.destroy();
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
	struct Parsing_Status_Info {
		Parsing_Status status;
		Ast_Statement statement;
		Parsing_Status_Info(Parsing_Status _status) : status(_status),statement() {}
		Parsing_Status_Info(const Ast_Statement& _statement) : status(Parsing_Status::Continue),statement(_statement) {}
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
				if(root->type == Ast_Expression_Type::Value || root->type == Ast_Expression_Type::Unary_Prefix_Operator || root->type == Ast_Expression_Type::Function_Call) {
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
			if(first_token.token->type == Token_Type::Semicolon || first_token.token->type == Token_Type::Comma || first_token.token->type == Token_Type::Right_Paren || first_token.token->type == Token_Type::Left_Brace) {
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
					case Token_Type::Left_Brace: character = U'{'; break;
					default: logo::unreachable();
				}
				logo::report_parser_error("Unexpected token '%'.",character);
				return {};
			}

			expr_state.empty = false;
			first_token = logo::get_next_token();
			switch(first_token.token->type) {
				case Token_Type::Identifier: {
					auto second_token = logo::peek_next_token(1);
					if(second_token.status == Lexing_Status::Out_Of_Tokens) {
						logo::report_parser_error("Expected a token after '%'.",first_token.token->string);
						return {};
					}

					Ast_Expression new_expr{};
					new_expr.type = Ast_Expression_Type::Function_Call;
					new_expr.function_call = state->memory.construct<Ast_Function_Call>();
					if(!new_expr.function_call) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Function_Call));
						return {};
					}
					{
						auto function_name_length = first_token.token->string.byte_length();
						char* function_name_ptr = state->memory.construct_string(function_name_length);
						if(!function_name_ptr) {
							Report_Error("Couldn't allocate % bytes of memory.",function_name_length + 1);
							return {};
						}
						std::memcpy(function_name_ptr,first_token.token->string.begin_ptr,function_name_length);
						new_expr.function_call->name = String_View(function_name_ptr,function_name_length);
					}

					if(second_token.token->type == Token_Type::Left_Paren) {
						second_token = logo::get_next_token();
						{
							auto third_token = logo::peek_next_token(1);
							if(third_token.status == Lexing_Status::Out_Of_Tokens) {
								logo::report_parser_error("Expected a token after '%'.",second_token.token->string);
								return {};
							}
							if(third_token.token->type == Token_Type::Right_Paren) {
								logo::discard_next_token();
								if(!logo::insert_ast_into_ast(state,&root_expr,new_expr)) return {};
								expr_state.complete = true;
								expr_state.last_token_type = Token_Type::Right_Paren;
								continue;
							}
						}

						while(true) {
							auto [arg_ast,success] = logo::parse_expression(state,true);
							if(!success) return {};

							Ast_Expression* arg_expr = state->memory.construct<Ast_Expression>();
							if(!arg_expr) {
								Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
								return {};
							}
							*arg_expr = arg_ast;

							if(!new_expr.function_call->arguments.push_back(arg_expr)) {
								Report_Error("Couldn't allocate % bytes of memory.",sizeof(*arg_expr));
								return {};
							}

							auto next_token = logo::get_next_token();
							if(next_token.status == Lexing_Status::Out_Of_Tokens) {
								logo::report_parser_error("Expected a token after '%'.",next_token.token->string);
								return {};
							}

							if(next_token.token->type == Token_Type::Right_Paren) break;
							else if(next_token.token->type == Token_Type::Comma) continue;
							else {
								logo::report_parser_error("Unexpected token '%'.",next_token.token->string);
								return {};
							}
						}

						if(!logo::insert_ast_into_ast(state,&root_expr,new_expr)) return {};
						expr_state.complete = true;
						expr_state.last_token_type = Token_Type::Right_Paren;
						continue;
					}
					[[fallthrough]];
				}
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
	
	[[nodiscard]] static Parsing_Status_Info parse_statement(Parsing_Result* state,bool inside_compound_statement = false,bool inside_loop = false) {
		Ast_Statement statement_ast{};

		auto first_token = logo::peek_next_token(1);
		if(first_token.status == Lexing_Status::Out_Of_Tokens) return Parsing_Status::Complete;
		if(first_token.token->type == Token_Type::Right_Brace) {
			if(inside_compound_statement) return Parsing_Status::Complete;
			logo::report_parser_error("Unexpected token '}'.");
			return Parsing_Status::Error;
		}

		switch(first_token.token->type) {
			case Token_Type::Semicolon: break;
			case Token_Type::Keyword_Let: {
				first_token = logo::get_next_token();

				statement_ast.type = Ast_Statement_Type::Declaration;
				statement_ast.declaration = {};

				auto identifier_token = logo::require_next_token(Token_Type::Identifier,"After 'let' keyword an identifier is expected.");
				if(identifier_token.status == Lexing_Status::Error) return Parsing_Status::Error;
				{
					auto function_name_length = identifier_token.token->string.byte_length();
					char* function_name_ptr = state->memory.construct_string(function_name_length);
					if(!function_name_ptr) {
						Report_Error("Couldn't allocate % bytes of memory.",function_name_length + 1);
						return Parsing_Status::Error;
					}
					std::memcpy(function_name_ptr,identifier_token.token->string.begin_ptr,function_name_length);
					statement_ast.declaration.name = String_View(function_name_ptr,function_name_length);
				}

				if(logo::require_next_token(Token_Type::Equals_Sign,"Declaration of '%' without initial value is not allowed.",identifier_token.token->string).status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}

				auto [init_ast,success] = logo::parse_expression(state);
				if(!success) return Parsing_Status::Error;
				statement_ast.declaration.initial_value_expr = init_ast;
				break;
			}
			case Token_Type::Keyword_If: {
				first_token = logo::get_next_token();

				statement_ast.type = Ast_Statement_Type::If_Statement;
				statement_ast.if_statement = {};

				auto [init_ast,success] = logo::parse_expression(state);
				if(!success) return Parsing_Status::Error;
				statement_ast.if_statement.condition_expr = init_ast;

				if(logo::require_next_token(Token_Type::Left_Brace,"After a condition, a '{' is required.").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}

				while(true) {
					auto status = logo::parse_statement(state,true,inside_loop);
					if(status.status == Parsing_Status::Complete) break;
					if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
					statement_ast.if_statement.if_true_statements.push_back(status.statement);
				}

				if(logo::require_next_token(Token_Type::Right_Brace,"Right brace (in progress).").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}

				auto else_keyword_token = logo::peek_next_token(1);
				if(else_keyword_token.status == Lexing_Status::Success) {
					if(else_keyword_token.token->type == Token_Type::Keyword_Else) {
						else_keyword_token = logo::get_next_token();

						auto left_brace_token = logo::peek_next_token(1);
						if(left_brace_token.status == Lexing_Status::Out_Of_Tokens) {
							logo::report_parser_error("Missing(?) a semicolon at the end of the statement.");
							return Parsing_Status::Error;
						}

						if(left_brace_token.token->type == Token_Type::Left_Brace) {
							left_brace_token = logo::get_next_token();

							while(true) {
								auto status = logo::parse_statement(state,true,inside_loop);
								if(status.status == Parsing_Status::Complete) break;
								if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
								statement_ast.if_statement.if_false_statements.push_back(status.statement);
							}

							if(logo::require_next_token(Token_Type::Right_Brace,"Right brace (else) (in progress).").status == Lexing_Status::Error) {
								return Parsing_Status::Error;
							}
						}
						else {
							auto status = logo::parse_statement(state,false,inside_loop);
							if(status.status == Parsing_Status::Complete) {
								logo::report_parser_error("An 'else' clause requires a non empty statement.");
								return Parsing_Status::Error;
							}
							if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
							statement_ast.if_statement.if_false_statements.push_back(status.statement);
						}
					}
				}
				return statement_ast;
			}
			case Token_Type::Keyword_While: {
				first_token = logo::get_next_token();

				statement_ast.type = Ast_Statement_Type::While_Statement;
				statement_ast.while_statement = {};

				auto [init_ast,success] = logo::parse_expression(state);
				if(!success) return Parsing_Status::Error;
				statement_ast.while_statement.condition_expr = init_ast;

				if(logo::require_next_token(Token_Type::Left_Brace,"After a condition, a '{' is required.").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}

				while(true) {
					auto status = logo::parse_statement(state,true,true);
					if(status.status == Parsing_Status::Complete) break;
					if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
					statement_ast.while_statement.body_statements.push_back(status.statement);
				}

				if(logo::require_next_token(Token_Type::Right_Brace,"Right brace (in progress).").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}
				return statement_ast;
			}
			case Token_Type::Keyword_Break: {
				first_token = logo::get_next_token();
				statement_ast.type = Ast_Statement_Type::Break_Stetement;
				if(!inside_loop) {
					logo::report_parser_error("Keyword 'break' can only be used inside a loop.");
					return Parsing_Status::Error;
				}
				break;
			}
			case Token_Type::Keyword_Continue: {
				first_token = logo::get_next_token();
				statement_ast.type = Ast_Statement_Type::Continue_Statement;
				if(!inside_loop) {
					logo::report_parser_error("Keyword 'continue' can only be used inside a loop.");
					return Parsing_Status::Error;
				}
				break;
			}
			case Token_Type::Identifier: {
				auto second_token = logo::peek_next_token(2);
				if(second_token.status == Lexing_Status::Out_Of_Tokens) {
					logo::report_parser_error("Missing(?) a semicolon at the end of the statement.");
					return Parsing_Status::Error;
				}
				if(logo::is_token_type_assignment(second_token.token->type)) {
					first_token = logo::get_next_token();
					second_token = logo::get_next_token();

					statement_ast.type = Ast_Statement_Type::Assignment;
					statement_ast.assignment = {};

					statement_ast.assignment.type = logo::token_type_to_ast_assignment_type(second_token.token->type);
					{
						auto function_name_length = first_token.token->string.byte_length();
						char* function_name_ptr = state->memory.construct_string(function_name_length);
						if(!function_name_ptr) {
							Report_Error("Couldn't allocate % bytes of memory.",function_name_length + 1);
							return Parsing_Status::Error;
						}
						std::memcpy(function_name_ptr,first_token.token->string.begin_ptr,function_name_length);
						statement_ast.assignment.name = String_View(function_name_ptr,function_name_length);
					}

					auto [expr_ast,success] = logo::parse_expression(state);
					if(!success) return Parsing_Status::Error;
					statement_ast.assignment.value_expr = expr_ast;
					break;
				}
				[[fallthrough]];
			}
			case Token_Type::String_Literal:
			case Token_Type::Int_Literal:
			case Token_Type::Float_Literal:
			case Token_Type::Bool_Literal:
			case Token_Type::Left_Paren:
			case Token_Type::Plus:
			case Token_Type::Minus:
			case Token_Type::Logical_Not: {
				statement_ast.type = Ast_Statement_Type::Expression;
				statement_ast.expression = {};

				auto [expr_ast,success] = logo::parse_expression(state);
				if(!success) return Parsing_Status::Error;
				statement_ast.expression = expr_ast;
				break;
			}
			default: {
				logo::report_parser_error("Invalid token '%'.\n",first_token.token->string);
				return Parsing_Status::Error;
			}
		}
		if(logo::require_next_token(Token_Type::Semicolon,"Expected a semicolon at the end of a statement.").status == Lexing_Status::Error) return Parsing_Status::Error;
		return statement_ast;
	}

	[[nodiscard]] Option<Parsing_Result> parse_input(Array_View<char> input) {
		if(input.length == 0) {
			logo::report_parser_error("Empty input file.");
			return {};
		}
		if(!logo::init_lexer(input)) return {};
		defer[]{logo::term_lexer();};

		Parsing_Result result{};
		bool successful_return = false;
		defer[&]{if(!successful_return) result.destroy();};

		while(true) {
			auto status = logo::parse_statement(&result);
			if(status.status == Parsing_Status::Error) return {};
			if(status.status == Parsing_Status::Complete) break;
			result.statements.push_back(status.statement);
		}

		successful_return = true;
		return result;
	}
}
