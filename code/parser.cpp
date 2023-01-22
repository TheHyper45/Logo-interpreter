#include "lexer.hpp"
#include "debug.hpp"
#include "parser.hpp"
#include "heap_array.hpp"

namespace logo {
	[[nodiscard]] static Ast_Binary_Operator_Type token_type_to_ast_binary_operator_type(Token_Type type) {
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
	[[nodiscard]] static Ast_Unary_Prefix_Operator_Type token_type_to_ast_unary_prefix_operator_type(Token_Type type) {
		switch(type) {
			case Token_Type::Plus: return Ast_Unary_Prefix_Operator_Type::Plus;
			case Token_Type::Minus: return Ast_Unary_Prefix_Operator_Type::Minus;
			case Token_Type::Logical_Not: return Ast_Unary_Prefix_Operator_Type::Logical_Not;
			case Token_Type::Ampersand: return Ast_Unary_Prefix_Operator_Type::Reference;
			case Token_Type::Caret: return Ast_Unary_Prefix_Operator_Type::Dereference;
			case Token_Type::Apostrophe: return Ast_Unary_Prefix_Operator_Type::Parent_Scope_Access;
			default: logo::unreachable();
		}
	}
	[[nodiscard]] static std::size_t get_operator_precedence(Ast_Binary_Operator_Type type) {
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
	[[nodiscard]] static Ast_Assignment_Type token_type_to_ast_assignment_type(Token_Type type) {
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

	static void destroy_expression(Ast_Expression* expression) {
		switch(expression->type) {
			case Ast_Expression_Type::Unary_Prefix_Operator: {
				if(expression->unary_prefix_operator->child) logo::destroy_expression(expression->unary_prefix_operator->child);
				break;
			}
			case Ast_Expression_Type::Binary_Operator: {
				if(expression->binary_operator->left) logo::destroy_expression(expression->binary_operator->left);
				if(expression->binary_operator->right) logo::destroy_expression(expression->binary_operator->right);
				break;
			}
			case Ast_Expression_Type::Function_Call: {
				for(auto& argument : expression->function_call->arguments) {
					if(argument) logo::destroy_expression(argument);
				}
				expression->function_call->arguments.destroy();
				break;
			}
			case Ast_Expression_Type::Array_Access: {
				if(expression->array_access->left) logo::destroy_expression(expression->array_access->left);
				if(expression->array_access->right) logo::destroy_expression(expression->array_access->right);
				break;
			}
		}
	}

	static void destroy_statement(Ast_Statement* statement) {
		switch(statement->type) {
			case Ast_Statement_Type::Assignment: {
				logo::destroy_expression(&statement->assignment.lvalue_expr);
				logo::destroy_expression(&statement->assignment.rvalue_expr);
				break;
			}
			case Ast_Statement_Type::Declaration: {
				logo::destroy_expression(&statement->declaration.initial_value_expr);
				break;
			}
			case Ast_Statement_Type::If_Statement: {
				for(auto& statement : statement->if_statement.if_false_statements) logo::destroy_statement(&statement);
				statement->if_statement.if_false_statements.destroy();
				for(auto& statement : statement->if_statement.if_true_statements) logo::destroy_statement(&statement);
				statement->if_statement.if_true_statements.destroy();
				logo::destroy_expression(&statement->if_statement.condition_expr);
				break;
			}
			case Ast_Statement_Type::While_Statement: {
				for(auto& statement : statement->while_statement.body_statements) logo::destroy_statement(&statement);
				statement->while_statement.body_statements.destroy();
				logo::destroy_expression(&statement->while_statement.condition_expr);
				break;
			}
			case Ast_Statement_Type::For_Statement: {
				for(auto& statement : statement->for_statement.body_statements) logo::destroy_statement(&statement);
				statement->for_statement.body_statements.destroy();
				logo::destroy_expression(&statement->for_statement.end_expr);
				logo::destroy_expression(&statement->for_statement.start_expr);
				break;
			}
			case Ast_Statement_Type::Function_Definition: {
				for(auto& statement : statement->function_definition.body_statements) logo::destroy_statement(&statement);
				statement->function_definition.body_statements.destroy();
				statement->function_definition.function_arguments.destroy();
				break;
			}
			case Ast_Statement_Type::Return_Statement: {
				if(statement->return_statement.return_value) logo::destroy_expression(statement->return_statement.return_value);
				break;
			}
		}
	}

	void Parsing_Result::destroy() {
		for(auto& statement : statements) logo::destroy_statement(&statement);
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

	[[nodiscard]] static Option<Ast_Value> create_ast_value(Parsing_Result* state,const Token& token) {
		Ast_Value value{};
		value.line_index = token.line_index;
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
				value.string_value = String_View(string_ptr,token.string.byte_length());
			}
			else if(token.type == Token_Type::Identifier) {
				value.type = Ast_Value_Type::Identifier;
				value.identfier_name = String_View(string_ptr,token.string.byte_length());
			}
			else logo::unreachable();
		}
		return value;
	}

	[[nodiscard]] static bool insert_value_into_ast(Parsing_Result* state,Ast_Expression* root,const Ast_Value& value) {
		if(root->type == Ast_Expression_Type::None) {
			root->type = Ast_Expression_Type::Value;
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
				root->binary_operator->right->value = value;
				return true;
			}
			return logo::insert_value_into_ast(state,root->binary_operator->right,value);
		}
		if(root->type == Ast_Expression_Type::Unary_Prefix_Operator) {
			if(!root->unary_prefix_operator->child) {
				root->unary_prefix_operator->child = state->memory.construct<Ast_Expression>();
				if(!root->unary_prefix_operator->child) {
					Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
					return false;
				}
				root->unary_prefix_operator->child->type = Ast_Expression_Type::Value;
				root->unary_prefix_operator->child->value = value;
				return true;
			}
			return logo::insert_value_into_ast(state,root->unary_prefix_operator->child,value);
		}
		logo::report_parser_error("Unexpected token.");//@TODO: Say which token.
		return false;
	}

	[[nodiscard]] static bool insert_operator_into_ast(Parsing_Result* state,Ast_Expression* root,const Token& token,Expression_State* expr_state) {
		if(root->type == Ast_Expression_Type::None) {
			root->type = Ast_Expression_Type::Unary_Prefix_Operator;
			if(!logo::is_token_type_unary_prefix_operator(token.type)) {
				logo::report_parser_error("Token '%' is not an unary prefix operator.",token.string);
				return false;
			}
			root->unary_prefix_operator = state->memory.construct<Ast_Unary_Prefix_Operator>();
			if(!root->unary_prefix_operator) {
				Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Unary_Prefix_Operator));
				return false;
			}
			root->unary_prefix_operator->type = logo::token_type_to_ast_unary_prefix_operator_type(token.type);
			root->unary_prefix_operator->line_index = token.line_index;
			return true;
		}
		if(logo::is_token_type_literal(expr_state->last_token_type) || expr_state->last_token_type == Token_Type::Identifier ||
		   expr_state->last_token_type == Token_Type::Right_Paren || expr_state->last_token_type == Token_Type::Right_Bracket) {
			Ast_Binary_Operator binary_operator{};
			binary_operator.type = logo::token_type_to_ast_binary_operator_type(token.type);
			binary_operator.line_index = token.line_index;
			binary_operator.left = state->memory.construct<Ast_Expression>();
			if(!binary_operator.left) {
				Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
				return false;
			}

			while(true) {
				if(root->is_parenthesised || root->type == Ast_Expression_Type::Value || root->type == Ast_Expression_Type::Unary_Prefix_Operator ||
				   root->type == Ast_Expression_Type::Function_Call || root->type == Ast_Expression_Type::Array_Access) {
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
			if(!logo::is_token_type_unary_prefix_operator(token.type)) {
				logo::report_parser_error("Token '%' is not an unary prefix operator.",token.string);
				return false;
			}

			Ast_Unary_Prefix_Operator unary_prefix_operator{};
			unary_prefix_operator.line_index = token.line_index;
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

	[[nodiscard]] static bool insert_ast_into_ast(Parsing_Result* state,Ast_Expression* root,const Ast_Expression& new_expr) {
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
				case Ast_Value_Type::String_Literal: logo::report_parser_error("Missing a binary operator between '%' and '('.",root->value.string_value); return false;
				case Ast_Value_Type::Identifier: logo::report_parser_error("Missing a binary operator between '%' and '('.",root->value.identfier_name); return false;
				default: logo::unreachable();
			}
		}
	}

	[[nodiscard]] static Option<Ast_Expression> parse_expression(Parsing_Result* state,bool inside_parenthesis,bool is_assignment_lvalue,bool is_for_lower_bound,bool inside_array_subscript);

	[[nodiscard]] static Option<Ast_Expression> parse_array_subscript(Parsing_Result* state,const Ast_Expression& left_expr,std::size_t line_index) {
		if(logo::require_next_token(Token_Type::Left_Bracket,"Expected a '['.").status == Lexing_Status::Error) return {};

		auto [subscript_expr,success] = logo::parse_expression(state,false,false,false,true);
		if(!success) return {};

		if(logo::require_next_token(Token_Type::Right_Bracket,"Expected a ']'.").status == Lexing_Status::Error) return {};

		Ast_Expression array_subscript_ast{};
		array_subscript_ast.type = Ast_Expression_Type::Array_Access;
		array_subscript_ast.array_access = state->memory.construct<Ast_Array_Access>();
		if(!array_subscript_ast.array_access) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Array_Access));
			return {};
		}
		array_subscript_ast.array_access->line_index = line_index;

		array_subscript_ast.array_access->left = state->memory.construct<Ast_Expression>();
		if(!array_subscript_ast.array_access->left) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
			return {};
		}
		*array_subscript_ast.array_access->left = left_expr;

		array_subscript_ast.array_access->right = state->memory.construct<Ast_Expression>();
		if(!array_subscript_ast.array_access->right) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
			return {};
		}
		*array_subscript_ast.array_access->right = subscript_expr;
		return array_subscript_ast;
	}

	[[nodiscard]] static Option<Ast_Expression> parse_expression(Parsing_Result* state,bool inside_parenthesis,bool is_assignment_lvalue,bool is_for_lower_bound,bool inside_array_subscript) {
		Ast_Expression root_expr{};
		Expression_State expr_state{};
		while(true) {
			auto first_token = logo::peek_next_token(1);
			if(first_token.status == Lexing_Status::Out_Of_Tokens) {
				if(expr_state.empty) logo::report_parser_error("Empty expressions are not allowed.");
				else logo::report_parser_error("Incomplete expression (error message not complete yet).");
				return {};
			}
			if(first_token.token->type == Token_Type::Semicolon || first_token.token->type == Token_Type::Comma || first_token.token->type == Token_Type::Right_Paren ||
			   first_token.token->type == Token_Type::Left_Brace || logo::is_token_type_assignment(first_token.token->type) || first_token.token->type == Token_Type::Arrow ||
			   first_token.token->type == Token_Type::Right_Bracket) {
				if(expr_state.complete) {
					if(!inside_parenthesis && first_token.token->type == Token_Type::Right_Paren) {
						logo::report_parser_error("Closed parenthesis that was never opened.");
						return {};
					}
					if(!is_assignment_lvalue && logo::is_token_type_assignment(first_token.token->type)) {
						logo::report_parser_error("A token '%' cannot appear in an expression.",first_token.token->string);
						return {};
					}
					if(!is_for_lower_bound && first_token.token->type == Token_Type::Arrow) {
						logo::report_parser_error("Unexpected token '->'.");
						return {};
					}
					if(!inside_array_subscript && first_token.token->type == Token_Type::Right_Bracket) {
						logo::report_parser_error("Unexpected token ']'.");
						return {};
					}
					return root_expr;
				}
				String_View character = "";
				switch(first_token.token->type) {
					case Token_Type::Semicolon: character = ";"; break;
					case Token_Type::Comma: character = ","; break;
					case Token_Type::Right_Paren: character = ")"; break;
					case Token_Type::Right_Bracket: character = "]"; break;
					case Token_Type::Left_Brace: character = "{"; break;
					case Token_Type::Arrow: character = "->"; break;
					default: {
						if(logo::is_token_type_assignment(first_token.token->type)) character = "=";
						else logo::unreachable();
					}
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
						new_expr.function_call->line_index = first_token.token->line_index;
						{
							auto third_token = logo::peek_next_token(1);
							if(third_token.status == Lexing_Status::Out_Of_Tokens) {
								logo::report_parser_error("Expected a token after '%'.",second_token.token->string);
								return {};
							}
							if(third_token.token->type == Token_Type::Right_Paren) {
								logo::discard_next_token();

								auto potential_left_bracket_token = logo::peek_next_token(1);
								if(potential_left_bracket_token.status == Lexing_Status::Out_Of_Tokens) {
									logo::report_parser_error("Expected a token after ')'.");
									return {};
								}

								if(potential_left_bracket_token.token->type == Token_Type::Left_Bracket) {
									auto [array_subscript_ast,success1] = logo::parse_array_subscript(state,new_expr,first_token.token->line_index);
									if(!success1) return {};
									if(!logo::insert_ast_into_ast(state,&root_expr,array_subscript_ast)) return {};
									expr_state.last_token_type = Token_Type::Right_Bracket;
								}
								else {
									if(!logo::insert_ast_into_ast(state,&root_expr,new_expr)) return {};
									expr_state.last_token_type = Token_Type::Right_Paren;
								}
								expr_state.complete = true;
								continue;
							}
						}

						while(true) {
							auto [arg_ast,success] = logo::parse_expression(state,true,false,false,false);
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

						auto potential_left_bracket_token = logo::peek_next_token(1);
						if(potential_left_bracket_token.status == Lexing_Status::Out_Of_Tokens) {
							logo::report_parser_error("Expected a token after ')'.");
							return {};
						}
						if(potential_left_bracket_token.token->type == Token_Type::Left_Bracket) {
							auto [array_subscript_ast,success1] = logo::parse_array_subscript(state,new_expr,first_token.token->line_index);
							if(!success1) return {};
							if(!logo::insert_ast_into_ast(state,&root_expr,array_subscript_ast)) return {};
							expr_state.last_token_type = Token_Type::Right_Bracket;
						}
						else {
							if(!logo::insert_ast_into_ast(state,&root_expr,new_expr)) return {};
							expr_state.last_token_type = Token_Type::Right_Paren;
						}
						expr_state.complete = true;
						continue;
					}
					[[fallthrough]];
				}
				case Token_Type::Int_Literal:
				case Token_Type::Float_Literal:
				case Token_Type::String_Literal:
				case Token_Type::Bool_Literal: {
					auto next_token = logo::peek_next_token(1);
					if(next_token.status == Lexing_Status::Out_Of_Tokens) {
						logo::report_parser_error("Expected a token after '%'.",first_token.token->string);
						return {};
					}
					if(next_token.token->type == Token_Type::Left_Bracket) {
						auto [subscripted_value,success0] = logo::create_ast_value(state,*first_token.token);
						if(!success0) return {};
						Ast_Expression left_expr{};
						left_expr.type = Ast_Expression_Type::Value;
						left_expr.value = subscripted_value;

						auto [array_subscript_ast,success1] = logo::parse_array_subscript(state,left_expr,first_token.token->line_index);
						if(!success1) return {};
						if(!logo::insert_ast_into_ast(state,&root_expr,array_subscript_ast)) return {};

						expr_state.complete = true;
						expr_state.last_token_type = Token_Type::Right_Bracket;
						continue;
					}
					else {
						auto [value,has_value] = logo::create_ast_value(state,*first_token.token);
						if(!has_value) return {};
						if(!logo::insert_value_into_ast(state,&root_expr,value)) return {};
						expr_state.complete = true;
						break;
					}
				}
				case Token_Type::Plus:
				case Token_Type::Minus:
				case Token_Type::Asterisk:
				case Token_Type::Slash:
				case Token_Type::Percent:
				case Token_Type::Ampersand:
				case Token_Type::Caret:
				case Token_Type::Apostrophe:
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
					auto [expr_ast,success] = logo::parse_expression(state,true,false,false,false);
					if(!success) return {};
					if(logo::require_next_token(Token_Type::Right_Paren,"Unmatched parenthesis.").status == Lexing_Status::Error) return {};
					expr_ast.is_parenthesised = true;

					auto next_token = logo::peek_next_token(1);
					if(next_token.status == Lexing_Status::Out_Of_Tokens) {
						logo::report_parser_error("Expected a token after ')'.");
						return {};
					}
					if(next_token.token->type == Token_Type::Left_Bracket) {
						auto [array_subscript_ast,success1] = logo::parse_array_subscript(state,expr_ast,first_token.token->line_index);
						if(!success1) return {};
						if(!logo::insert_ast_into_ast(state,&root_expr,array_subscript_ast)) return {};
						expr_state.last_token_type = Token_Type::Right_Bracket;
					}
					else {
						if(!logo::insert_ast_into_ast(state,&root_expr,expr_ast)) return {};
						expr_state.last_token_type = Token_Type::Right_Paren;
					}
					expr_state.complete = true;
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

	[[nodiscard]] static Option<Ast_Assignment> parse_assignment(Parsing_Result* state) {
		Ast_Assignment assignment_ast{};

		auto [lvalue_expr,success0] = logo::parse_expression(state,false,true,false,false);
		if(!success0) return {};

		auto assignment_token = logo::get_next_token();
		if(assignment_token.status == Lexing_Status::Out_Of_Tokens) {
			logo::report_parser_error("Missing(?) a semicolon at the end of the statement.");
			return {};
		}

		if(!logo::is_token_type_assignment(assignment_token.token->type)) {
			logo::report_parser_error("Expected an assignment token");
			return {};
		}
		assignment_ast.type = logo::token_type_to_ast_assignment_type(assignment_token.token->type);
		assignment_ast.line_index = assignment_token.token->line_index;
		assignment_ast.lvalue_expr = lvalue_expr;

		auto [value_expr,success1] = logo::parse_expression(state,false,false,false,false);
		if(!success1) return {};
		assignment_ast.rvalue_expr = value_expr;
		return assignment_ast;
	}
	
	[[nodiscard]] static Parsing_Status_Info parse_statement(Parsing_Result* state,bool inside_compound_statement,bool inside_loop,bool inside_function) {
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

				statement_ast.line_index = first_token.token->line_index;
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

				auto [init_ast,success] = logo::parse_expression(state,false,false,false,false);
				if(!success) return Parsing_Status::Error;
				statement_ast.declaration.initial_value_expr = init_ast;
				break;
			}
			case Token_Type::Keyword_If: {
				first_token = logo::get_next_token();

				statement_ast.line_index = first_token.token->line_index;
				statement_ast.type = Ast_Statement_Type::If_Statement;
				statement_ast.if_statement = {};

				auto [init_ast,success] = logo::parse_expression(state,false,false,false,false);
				if(!success) return Parsing_Status::Error;
				statement_ast.if_statement.condition_expr = init_ast;

				if(logo::require_next_token(Token_Type::Left_Brace,"After a condition, a '{' is required.").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}

				while(true) {
					auto status = logo::parse_statement(state,true,inside_loop,inside_function);
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
								auto status = logo::parse_statement(state,true,inside_loop,inside_function);
								if(status.status == Parsing_Status::Complete) break;
								if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
								statement_ast.if_statement.if_false_statements.push_back(status.statement);
							}

							if(logo::require_next_token(Token_Type::Right_Brace,"Expected a '}'.").status == Lexing_Status::Error) {
								return Parsing_Status::Error;
							}
						}
						else {
							auto status = logo::parse_statement(state,false,inside_loop,inside_function);
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

				statement_ast.line_index = first_token.token->line_index;
				statement_ast.type = Ast_Statement_Type::While_Statement;
				statement_ast.while_statement = {};

				auto [init_ast,success] = logo::parse_expression(state,false,false,false,false);
				if(!success) return Parsing_Status::Error;
				statement_ast.while_statement.condition_expr = init_ast;

				if(logo::require_next_token(Token_Type::Left_Brace,"After a condition, a '{' is required.").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}

				while(true) {
					auto status = logo::parse_statement(state,true,true,inside_function);
					if(status.status == Parsing_Status::Complete) break;
					if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
					statement_ast.while_statement.body_statements.push_back(status.statement);
				}

				if(logo::require_next_token(Token_Type::Right_Brace,"Expected a '}'.").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}
				return statement_ast;
			}
			case Token_Type::Keyword_For: {
				first_token = logo::get_next_token();

				statement_ast.line_index = first_token.token->line_index;
				statement_ast.type = Ast_Statement_Type::For_Statement;
				statement_ast.for_statement = {};

				auto iternator_name_token = logo::require_next_token(Token_Type::Identifier,"Expected an identifier after 'for'.");
				if(iternator_name_token.status == Lexing_Status::Out_Of_Tokens) return Parsing_Status::Error;
				{
					auto function_name_length = iternator_name_token.token->string.byte_length();
					char* function_name_ptr = state->memory.construct_string(function_name_length);
					if(!function_name_ptr) {
						Report_Error("Couldn't allocate % bytes of memory.",function_name_length + 1);
						return Parsing_Status::Error;
					}
					std::memcpy(function_name_ptr,iternator_name_token.token->string.begin_ptr,function_name_length);
					statement_ast.for_statement.iterator_identifier = String_View(function_name_ptr,function_name_length);
				}

				if(logo::require_next_token(Token_Type::Colon,"Expected a colon after '%'.",iternator_name_token.token->string).status == Lexing_Status::Out_Of_Tokens) {
					return Parsing_Status::Error;
				}

				auto [lower_bound_expr,success0] = logo::parse_expression(state,false,false,true,false);
				if(!success0) return Parsing_Status::Error;
				statement_ast.for_statement.start_expr = lower_bound_expr;

				if(logo::require_next_token(Token_Type::Arrow,"Expected an arrow after the starting index in a 'for' loop.").status == Lexing_Status::Out_Of_Tokens) {
					return Parsing_Status::Error;
				}

				auto [upper_bound_expr,success1] = logo::parse_expression(state,false,false,false,false);
				if(!success1) return Parsing_Status::Error;
				statement_ast.for_statement.end_expr = upper_bound_expr;

				if(logo::require_next_token(Token_Type::Left_Brace,"After a condition, a '{' is required.").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}

				while(true) {
					auto status = logo::parse_statement(state,true,true,inside_function);
					if(status.status == Parsing_Status::Complete) break;
					if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
					statement_ast.for_statement.body_statements.push_back(status.statement);
				}

				if(logo::require_next_token(Token_Type::Right_Brace,"Expected a '}'.").status == Lexing_Status::Error) {
					return Parsing_Status::Error;
				}
				return statement_ast;
			}
			case Token_Type::Keyword_Func: {
				first_token = logo::get_next_token();

				statement_ast.line_index = first_token.token->line_index;
				statement_ast.type = Ast_Statement_Type::Function_Definition;
				statement_ast.function_definition = {};

				auto identifier_token = logo::require_next_token(Token_Type::Identifier,"Expected an identifier after 'func'.");
				if(identifier_token.status == Lexing_Status::Error) return Parsing_Status::Error;
				{
					auto function_name_length = identifier_token.token->string.byte_length();
					char* function_name_ptr = state->memory.construct_string(function_name_length);
					if(!function_name_ptr) {
						Report_Error("Couldn't allocate % bytes of memory.",function_name_length + 1);
						return Parsing_Status::Error;
					}
					std::memcpy(function_name_ptr,identifier_token.token->string.begin_ptr,function_name_length);
					statement_ast.function_definition.name = String_View(function_name_ptr,function_name_length);
				}

				if(logo::require_next_token(Token_Type::Left_Paren,"Expected a token '('.").status == Lexing_Status::Error) return Parsing_Status::Error;

				bool allow_comma = false;
				bool allow_identifier = true;
				bool allow_right_paren = true;
				while(true) {
					auto next_token = logo::get_next_token();
					if(next_token.status == Lexing_Status::Out_Of_Tokens) {
						logo::report_parser_error("Expected a token after '('.");
						return Parsing_Status::Error;
					}

					if(next_token.token->type == Token_Type::Right_Paren) {
						if(!allow_right_paren) {
							logo::report_parser_error("Expected an identifier.");
							return Parsing_Status::Error;
						}
						break;
					}
					else if(next_token.token->type == Token_Type::Identifier) {
						if(!allow_identifier) {
							logo::report_parser_error("Expected a ',' or ')'.");
							return Parsing_Status::Error;
						}

						String_View argument{};
						{
							auto function_name_length = next_token.token->string.byte_length();
							char* function_name_ptr = state->memory.construct_string(function_name_length);
							if(!function_name_ptr) {
								Report_Error("Couldn't allocate % bytes of memory.",function_name_length + 1);
								return Parsing_Status::Error;
							}
							std::memcpy(function_name_ptr,next_token.token->string.begin_ptr,function_name_length);
							argument = String_View(function_name_ptr,function_name_length);
						}
						if(!statement_ast.function_definition.function_arguments.push_back(argument)) {
							Report_Error("Couldn't allocate % bytes of memory.",sizeof(argument));
							return Parsing_Status::Error;
						}

						allow_comma = true;
						allow_identifier = false;
						allow_right_paren = true;
					}
					else if(next_token.token->type == Token_Type::Comma) {
						if(!allow_comma) {
							logo::report_parser_error("Expected an identifier or ')'.");
							return Parsing_Status::Error;
						}
						allow_comma = false;
						allow_identifier = true;
						allow_right_paren = false;
					}
					else {
						logo::report_parser_error("Invalid token '%'.",next_token.token->string);
						return Parsing_Status::Error;
					}
				}
				
				auto next_token = logo::peek_next_token(1);
				if(next_token.status == Lexing_Status::Out_Of_Tokens) {
					logo::report_parser_error("Expected a token after ')'.");
					return Parsing_Status::Error;
				}

				if(next_token.token->type == Token_Type::Left_Brace) {
					next_token = logo::get_next_token();

					while(true) {
						auto status = logo::parse_statement(state,true,false,true);
						if(status.status == Parsing_Status::Complete) break;
						if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
						statement_ast.function_definition.body_statements.push_back(status.statement);
					}

					if(logo::require_next_token(Token_Type::Right_Brace,"Expected a '}'.").status == Lexing_Status::Error) {
						return Parsing_Status::Error;
					}
				}
				else {
					auto status = logo::parse_statement(state,false,false,true);
					if(status.status == Parsing_Status::Complete) {
						logo::report_parser_error("A function body must comprise of at least one statement.");
						return Parsing_Status::Error;
					}
					if(status.status == Parsing_Status::Error) return Parsing_Status::Error;
					statement_ast.function_definition.body_statements.push_back(status.statement);
				}
				return statement_ast;
			}
			case Token_Type::Keyword_Break: {
				first_token = logo::get_next_token();
				statement_ast.line_index = first_token.token->line_index;
				statement_ast.type = Ast_Statement_Type::Break_Statement;
				if(!inside_loop) {
					logo::report_parser_error("Keyword 'break' can only be used inside a loop.");
					return Parsing_Status::Error;
				}
				break;
			}
			case Token_Type::Keyword_Continue: {
				first_token = logo::get_next_token();
				statement_ast.line_index = first_token.token->line_index;
				statement_ast.type = Ast_Statement_Type::Continue_Statement;
				if(!inside_loop) {
					logo::report_parser_error("Keyword 'continue' can only be used inside a loop.");
					return Parsing_Status::Error;
				}
				break;
			}
			case Token_Type::Keyword_Return: {
				first_token = logo::get_next_token();
				statement_ast.line_index = first_token.token->line_index;
				statement_ast.type = Ast_Statement_Type::Return_Statement;
				statement_ast.return_statement = {};
				if(!inside_function) {
					logo::report_parser_error("Keyword 'return' can only be used inside a function.");
					return Parsing_Status::Error;
				}

				auto next_token = logo::peek_next_token(1);
				if(next_token.status == Lexing_Status::Out_Of_Tokens) {
					logo::report_parser_error("Expected a token after 'return'.");
					return Parsing_Status::Error;
				}

				if(next_token.token->type != Token_Type::Semicolon) {
					auto [return_expr,success] = logo::parse_expression(state,false,false,false,false);
					if(!success) return Parsing_Status::Error;

					statement_ast.return_statement.return_value = state->memory.construct<Ast_Expression>();
					if(!statement_ast.return_statement.return_value) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(Ast_Expression));
						return Parsing_Status::Error;
					}
					*statement_ast.return_statement.return_value = return_expr;
				}
				break;
			}
			case Token_Type::String_Literal:
			case Token_Type::Int_Literal:
			case Token_Type::Float_Literal:
			case Token_Type::Bool_Literal:
			case Token_Type::Identifier:
			case Token_Type::Left_Paren:
			case Token_Type::Plus:
			case Token_Type::Minus:
			case Token_Type::Logical_Not:
			case Token_Type::Ampersand:
			case Token_Type::Caret:
			case Token_Type::Apostrophe: {
				for(std::size_t i = 1;;i += 1) {
					auto next_token = logo::peek_next_token(i);
					if(next_token.status == Lexing_Status::Out_Of_Tokens) {
						logo::report_parser_error("Missing a token.");//@TODO: Improve error message.
						return Parsing_Status::Error;
					}
					if(next_token.token->type == Token_Type::Semicolon) {
						statement_ast.type = Ast_Statement_Type::Expression;
						auto [expr_ast,success] = logo::parse_expression(state,false,false,false,false);
						if(!success) return Parsing_Status::Error;
						statement_ast.expression = expr_ast;
						break;
					}
					if(logo::is_token_type_assignment(next_token.token->type)) {
						statement_ast.type = Ast_Statement_Type::Assignment;
						auto [assignment_ast,success] = logo::parse_assignment(state);
						if(!success) return Parsing_Status::Error;
						statement_ast.assignment = assignment_ast;
						break;
					}
				}
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
			auto status = logo::parse_statement(&result,false,false,false);
			if(status.status == Parsing_Status::Error) return {};
			if(status.status == Parsing_Status::Complete) break;
			result.statements.push_back(status.statement);
		}

		successful_return = true;
		return result;
	}
}
