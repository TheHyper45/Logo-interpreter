#include <cmath>
#include <random>
#include <chrono>
#include "debug.hpp"
#include "parser.hpp"
#include "canvas.hpp"
#include "interpreter.hpp"
#include "static_array.hpp"

namespace logo {
	struct Interpreter_Variable;
	enum struct Interpreter_Value_Type {
		Void,
		Int,
		Float,
		Bool,
		String,
		Canvas,
		Reference
	};
	struct Interpreter_Value_Reference {
		std::size_t var_index;
		std::size_t generation;
	};
	struct Interpreter_Value {
		Interpreter_Value_Type type;
		union {
			std::int64_t int_v;
			double float_v;
			bool bool_v;
			String_View string_v;
			Canvas canvas_v;
			Interpreter_Value_Reference reference_v;
		};
		Interpreter_Value() : type(),int_v() {}
	};
	struct Interpreter_Variable {
		String_View name;
		Interpreter_Value value;
		std::size_t generation;
	};
	struct Interpreter_Context {
		std::mt19937_64 random_engine;
		std::uniform_real_distribution<double> random_dist_0_1;
		Heap_Array<Interpreter_Variable> variables;
		std::size_t generation_counter;
	};

	template<typename... Args>
	static void report_interpreter_error(std::size_t line_index,Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::write_char32_t_to_error_message,"[Runtime error] Line %: ",line_index);
		logo::format_into(logo::write_char32_t_to_error_message,format,std::forward<Args>(args)...);
		logo::write_char32_t_to_error_message('\n');
	}

	[[nodiscard]] static Option<Interpreter_Value> make_interpreter_value_from_ast_value(Interpreter_Context* context,const Ast_Value& value) {
		Interpreter_Value result{};
		switch(value.type) {
			case Ast_Value_Type::Int_Literal: {
				result.type = Interpreter_Value_Type::Int;
				result.int_v = value.int_value;
				return result;
			}
			case Ast_Value_Type::Float_Literal: {
				result.type = Interpreter_Value_Type::Float;
				result.float_v = value.float_value;
				return result;
			}
			case Ast_Value_Type::Bool_Literal: {
				result.type = Interpreter_Value_Type::Bool;
				result.bool_v = value.bool_value;
				return result;
			}
			case Ast_Value_Type::String_Literal: {
				result.type = Interpreter_Value_Type::String;
				result.string_v = value.string_value;
				return result;
			}
			case Ast_Value_Type::Identifier: {
				result.type = Interpreter_Value_Type::Void;
				for(const auto& var : context->variables) {
					if(std::strcmp(var.name.begin_ptr,value.identfier_name.begin_ptr) == 0) {
						if(var.value.type == Interpreter_Value_Type::Canvas) {
							auto [copy,success] = var.value.canvas_v.clone();
							if(!success) return {};
							result.type = Interpreter_Value_Type::Canvas;
							result.canvas_v = copy;
						}
						else result = var.value;
						break;
					}
				}
				if(result.type == Interpreter_Value_Type::Void) {
					logo::report_interpreter_error(value.line_index,"Identifier '%' does not exist.",value.identfier_name);
					return {};
				}
				return result;
			}
			default: logo::unreachable();
		}
	}

	template<typename T>
	[[nodiscard]] static bool compute_compare_operation(Ast_Binary_Operator_Type type,const T& left,const T& right) {
		switch(type) {
			case Ast_Binary_Operator_Type::Compare_Equal: return left == right;
			case Ast_Binary_Operator_Type::Compare_Unequal: return left != right;
			case Ast_Binary_Operator_Type::Compare_Less_Than: return left < right;
			case Ast_Binary_Operator_Type::Compare_Less_Than_Or_Equal: return left <= right;
			case Ast_Binary_Operator_Type::Compare_Greater_Than: return left > right;
			case Ast_Binary_Operator_Type::Compare_Greater_Than_Or_Equal: return left >= right;
			default: logo::unreachable();
		}
	}
	template<typename T>
	[[nodiscard]] static T compute_arithmetic_operation(Ast_Binary_Operator_Type type,const T& left,const T& right) {
		switch(type) {
			case Ast_Binary_Operator_Type::Plus: return left + right;
			case Ast_Binary_Operator_Type::Minus: return left - right;
			case Ast_Binary_Operator_Type::Multiply: return left * right;
			case Ast_Binary_Operator_Type::Divide: return left / right;
			case Ast_Binary_Operator_Type::Remainder: {
				if constexpr(std::is_integral_v<T>) return left % right;
				else return std::fmod(left,right);
			}
			case Ast_Binary_Operator_Type::Exponentiate: return std::pow(left,right);
			default: logo::unreachable();
		}
	}
	template<typename T>
	[[nodiscard]] static T compute_compound_assignment_operation(Ast_Assignment_Type type,const T& left,const T& right) {
		switch(type) {
			case Ast_Assignment_Type::Compound_Plus: return left + right;
			case Ast_Assignment_Type::Compound_Minus: return left - right;
			case Ast_Assignment_Type::Compound_Multiply: return left * right;
			case Ast_Assignment_Type::Compound_Divide: return left / right;
			case Ast_Assignment_Type::Compound_Remainder:
			{
				if constexpr(std::is_integral_v<T>) return left % right;
				else return std::fmod(left,right);
			}
			case Ast_Assignment_Type::Compound_Exponentiate: return std::pow(left,right);
			default: logo::unreachable();
		}
	}

	[[nodiscard]] static Option<Interpreter_Value> compute_expression(Interpreter_Context* context,const Ast_Expression& expression) {
		switch(expression.type) {
			case Ast_Expression_Type::Value: {
				return logo::make_interpreter_value_from_ast_value(context,expression.value);
			}
			case Ast_Expression_Type::Unary_Prefix_Operator: {
				if(expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Reference || expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Dereference) {
					if(expression.unary_prefix_operator->child->type != Ast_Expression_Type::Value) {
						if(expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Reference) {
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot take a reference of an expression.");
						}
						else logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot dereference an expression.");
						return {};
					}

					const auto& identfier_value = expression.unary_prefix_operator->child->value;
					if(identfier_value.type != Ast_Value_Type::Identifier) {
						if(expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Reference) {
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot take a reference of an expression.");
						}
						else logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot dereference an expression.");
						return {};
					}

					for(std::size_t i = 0;i < context->variables.length;i += 1) {
						const auto& var = context->variables[i];
						if(std::strcmp(var.name.begin_ptr,identfier_value.identfier_name.begin_ptr) == 0) {
							if(expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Reference) {
								Interpreter_Value value{};
								value.type = Interpreter_Value_Type::Reference;
								value.reference_v = {};
								value.reference_v.var_index = i;
								value.reference_v.generation = var.generation;
								return value;
							}

							if(var.value.type != Interpreter_Value_Type::Reference) {
								logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot dereference '%' which is a non-reference.",identfier_value.identfier_name);
								return {};
							}
							const auto& ref_info = var.value.reference_v;
							if(ref_info.var_index >= context->variables.length) {
								logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Dangling reference '%'.",identfier_value.identfier_name);
								return {};
							}
							const Interpreter_Variable& referenced_var = context->variables[ref_info.var_index];
							if(ref_info.generation != referenced_var.generation) {
								logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Dangling reference '%'.",identfier_value.identfier_name);
								return {};
							}

							if(referenced_var.value.type == Interpreter_Value_Type::Canvas) {
								auto [result,success] = referenced_var.value.canvas_v.clone();
								if(!success) return {};
								Interpreter_Value value{};
								value.type = Interpreter_Value_Type::Canvas;
								value.canvas_v = result;
								return value;
							}
							else return referenced_var.value;
						}
					}
					logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Identifier '%' does not exist.",identfier_value.identfier_name);
					return {};
				}

				auto [value,success] = logo::compute_expression(context,*expression.unary_prefix_operator->child);
				if(!success) return {};
				switch(expression.unary_prefix_operator->type) {
					case Ast_Unary_Prefix_Operator_Type::Plus:
					case Ast_Unary_Prefix_Operator_Type::Minus: {
						if(value.type == Interpreter_Value_Type::Bool) {
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot negate '%'.",value.bool_v);
							return {};
						}
						if(value.type == Interpreter_Value_Type::String) {
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot negate \"%\".",value.string_v);
							return {};
						}
						if(value.type == Interpreter_Value_Type::Int) value.int_v *= -1;
						else if(value.type == Interpreter_Value_Type::Float) value.float_v *= -1.0;
						break;
					}
					case Ast_Unary_Prefix_Operator_Type::Logical_Not: {
						if(value.type == Interpreter_Value_Type::Bool) value.bool_v = !value.bool_v;
						else {
							//@TODO: Print value that couldn't be negated.
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot logically negate a nonboolean value.");
							return {};
						}
						break;
					}
					default: logo::unreachable();
				}
				return value;
			}
			case Ast_Expression_Type::Binary_Operator: {
				auto [value0,success0] = logo::compute_expression(context,*expression.binary_operator->left);
				if(!success0) return {};
				auto [value1,success1] = logo::compute_expression(context,*expression.binary_operator->right);
				if(!success1) return {};

				switch(expression.binary_operator->type) {
					case Ast_Binary_Operator_Type::Plus:
					case Ast_Binary_Operator_Type::Minus:
					case Ast_Binary_Operator_Type::Multiply:
					case Ast_Binary_Operator_Type::Divide:
					case Ast_Binary_Operator_Type::Remainder:
					case Ast_Binary_Operator_Type::Exponentiate: {
						Interpreter_Value result{};
						if(value0.type == Interpreter_Value_Type::Int && value1.type == Interpreter_Value_Type::Int) {
							result.type = Interpreter_Value_Type::Int;
							result.int_v = logo::compute_arithmetic_operation(expression.binary_operator->type,value0.int_v,value1.int_v);
						}
						else if(value0.type == Interpreter_Value_Type::Float && value1.type == Interpreter_Value_Type::Float) {
							result.type = Interpreter_Value_Type::Float;
							result.float_v = logo::compute_arithmetic_operation(expression.binary_operator->type,value0.float_v,value1.float_v);
						}
						else if(value0.type == Interpreter_Value_Type::Int && value1.type == Interpreter_Value_Type::Float) {
							result.type = Interpreter_Value_Type::Float;
							result.float_v = logo::compute_arithmetic_operation<double>(expression.binary_operator->type,value0.int_v,value1.float_v);
						}
						else if(value0.type == Interpreter_Value_Type::Float && value1.type == Interpreter_Value_Type::Int) {
							result.type = Interpreter_Value_Type::Float;
							result.float_v = logo::compute_arithmetic_operation<double>(expression.binary_operator->type,value0.float_v,value1.int_v);
						}
						else {
							//@TODO: Print more info.
							logo::report_interpreter_error(expression.binary_operator->line_index,"Invalid arithmetic operation (error message in progress).");
							return {};
						}
						return result;
					}
					case Ast_Binary_Operator_Type::Logical_And:
					case Ast_Binary_Operator_Type::Logical_Or: {
						if(value0.type != Interpreter_Value_Type::Bool || value1.type != Interpreter_Value_Type::Bool) {
							String_View operator_string = (expression.binary_operator->type == Ast_Binary_Operator_Type::Logical_And) ? "and" : "or";
							logo::report_interpreter_error(expression.binary_operator->line_index,"Operator '%' needs both operands of type 'Bool'.",operator_string);
							return {};
						}
						Interpreter_Value result{};
						result.type = Interpreter_Value_Type::Bool;
						if(expression.binary_operator->type == Ast_Binary_Operator_Type::Logical_And) result.bool_v = (value0.bool_v && value1.bool_v);
						else result.bool_v = (value0.bool_v || value1.bool_v);
						return result;
					}
					case Ast_Binary_Operator_Type::Compare_Equal:
					case Ast_Binary_Operator_Type::Compare_Unequal:
					case Ast_Binary_Operator_Type::Compare_Less_Than:
					case Ast_Binary_Operator_Type::Compare_Less_Than_Or_Equal:
					case Ast_Binary_Operator_Type::Compare_Greater_Than:
					case Ast_Binary_Operator_Type::Compare_Greater_Than_Or_Equal: {
						Interpreter_Value result{};
						result.type = Interpreter_Value_Type::Bool;
						if(value0.type == Interpreter_Value_Type::Int && value1.type == Interpreter_Value_Type::Int) {
							result.bool_v = logo::compute_compare_operation(expression.binary_operator->type,value0.int_v,value1.int_v);
						}
						else if(value0.type == Interpreter_Value_Type::Float && value1.type == Interpreter_Value_Type::Float) {
							result.bool_v = logo::compute_compare_operation(expression.binary_operator->type,value0.float_v,value1.float_v);
						}
						else if(value0.type == Interpreter_Value_Type::Int && value1.type == Interpreter_Value_Type::Float) {
							result.bool_v = logo::compute_compare_operation(expression.binary_operator->type,static_cast<double>(value0.int_v),value1.float_v);
						}
						else if(value0.type == Interpreter_Value_Type::Float && value1.type == Interpreter_Value_Type::Int) {
							result.bool_v = logo::compute_compare_operation(expression.binary_operator->type,value0.float_v,static_cast<double>(value1.int_v));
						}
						else if(value0.type == Interpreter_Value_Type::Bool && value1.type == Interpreter_Value_Type::Bool) {
							if(expression.binary_operator->type != Ast_Binary_Operator_Type::Compare_Equal && expression.binary_operator->type != Ast_Binary_Operator_Type::Compare_Unequal) {
								//@TODO: Print the wrong operator.
								logo::report_interpreter_error(expression.binary_operator->line_index,"Cannot apply that comparison operator on bools.");
								return {};
							}
							if(expression.binary_operator->type == Ast_Binary_Operator_Type::Compare_Equal) result.bool_v = (value0.bool_v == value1.bool_v);
							else result.bool_v = (value0.bool_v != value1.bool_v);
						}
						else if(value0.type == Interpreter_Value_Type::String && value1.type == Interpreter_Value_Type::String) {
							if(expression.binary_operator->type != Ast_Binary_Operator_Type::Compare_Equal && expression.binary_operator->type != Ast_Binary_Operator_Type::Compare_Unequal) {
								//@TODO: Print the wrong operator.
								logo::report_interpreter_error(expression.binary_operator->line_index,"Cannot apply that comparison operator on strings.");
								return {};
							}
							if(expression.binary_operator->type == Ast_Binary_Operator_Type::Compare_Equal) result.bool_v = (std::strcmp(value0.string_v.begin_ptr,value1.string_v.begin_ptr) == 0);
							else result.bool_v = (std::strcmp(value0.string_v.begin_ptr,value1.string_v.begin_ptr) != 0);
						}
						else {
							//@TODO: Print more info.
							logo::report_interpreter_error(expression.binary_operator->line_index,"Invalid comparison operation (error message in progress).");
							return {};
						}
						return result;
					}
					default: logo::unreachable();
				}
			}
			case Ast_Expression_Type::Function_Call: {
				Static_Array<Interpreter_Value,16> arg_values{};
				defer[&]{
					for(auto& arg : arg_values) {
						if(arg.type == Interpreter_Value_Type::Canvas) arg.canvas_v.destroy();
					}
				};

				for(const auto& arg_expr : expression.function_call->arguments) {
					auto [arg_value,success] = logo::compute_expression(context,*arg_expr);
					if(!success) return {};
					if(!arg_values.push_back(arg_value)) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function '%' cannot take more than 16 arguments.",expression.function_call->name);
						return {};
					}
				}

				Interpreter_Value result{};
				if(std::strcmp(expression.function_call->name.begin_ptr,"sin") == 0) {
					if(expression.function_call->arguments.length != 1) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'sin' takes exactly 1 argument.");
						return {};
					}
					
					const auto& arg0 = arg_values[0];
					if(arg0.type == Interpreter_Value_Type::Int) {
						result.type = Interpreter_Value_Type::Float;
						result.float_v = std::sin(arg0.int_v);
					}
					else if(arg0.type == Interpreter_Value_Type::Float) {
						result.type = Interpreter_Value_Type::Float;
						result.float_v = std::sin(arg0.float_v);
					}
					else {
						logo::report_interpreter_error(expression.function_call->line_index,"Argument 1 to a function 'sin' must be a number.");
						return {};
					}
				}
				else if(std::strcmp(expression.function_call->name.begin_ptr,"typename") == 0) {
					if(expression.function_call->arguments.length != 1) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'typename' takes exactly 1 argument.");
						return {};
					}

					result.type = Interpreter_Value_Type::String;
					const auto& arg0 = arg_values[0];
					switch(arg0.type) {
						case Interpreter_Value_Type::Int:		result.string_v = String_View("Int");		break;
						case Interpreter_Value_Type::Float:		result.string_v = String_View("Float");		break;
						case Interpreter_Value_Type::Bool:		result.string_v = String_View("Bool");		break;
						case Interpreter_Value_Type::String:	result.string_v = String_View("String");	break;
						case Interpreter_Value_Type::Void:		result.string_v = String_View("Void");		break;
						case Interpreter_Value_Type::Canvas:	result.string_v = String_View("Canvas");	break;
						case Interpreter_Value_Type::Reference:	result.string_v = String_View("Reference");	break;
						default: logo::unreachable();
					}
				}
				else if(std::strcmp(expression.function_call->name.begin_ptr,"random") == 0) {
					if(expression.function_call->arguments.length != 0) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'random' takes no argumnets");
						return {};
					}

					result.type = Interpreter_Value_Type::Float;
					result.float_v = context->random_dist_0_1(context->random_engine);
				}
				else if(std::strcmp(expression.function_call->name.begin_ptr,"strlen") == 0) {
					if(expression.function_call->arguments.length != 1) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'strlen' takes exactly 1 argument.");
						return {};
					}

					const auto& arg0 = arg_values[0];
					if(arg0.type != Interpreter_Value_Type::String) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'strlen' requires a string as a first argument.");
						return {};
					}

					result.type = Interpreter_Value_Type::Int;
					result.int_v = arg0.string_v.byte_length();
				}
				else if(std::strcmp(expression.function_call->name.begin_ptr,"logo_init") == 0) {
					if(expression.function_call->arguments.length != 2) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_init' takes exactly 2 arguments.");
						return {};
					}

					const auto& arg0 = arg_values[0];
					if(arg0.type != Interpreter_Value_Type::Int) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_init' requires an integer as a first argument.");
						return {};
					}

					const auto& arg1 = arg_values[1];
					if(arg1.type != Interpreter_Value_Type::Int) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_init' requires an integer as a second argument.");
						return {};
					}

					result.type = Interpreter_Value_Type::Canvas;
					result.canvas_v = {};
					if(!result.canvas_v.init(arg0.int_v,arg1.int_v)) {
						return {};
					}
				}
				else if(std::strcmp(expression.function_call->name.begin_ptr,"logo_move_forward") == 0) {
					if(expression.function_call->arguments.length != 2) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_move_forward' takes exactly 2 arguments.");
						return {};
					}

					auto& arg0 = arg_values[0];
					if(arg0.type != Interpreter_Value_Type::Canvas) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_move_forward' requires a canvas as a first argument.");
						return {};
					}

					const auto& arg1 = arg_values[1];
					if(arg1.type != Interpreter_Value_Type::Int && arg1.type != Interpreter_Value_Type::Float) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_save' requires an integer/float as a second argument.");
						return {};
					}

					if(arg1.type == Interpreter_Value_Type::Int) {
						arg0.canvas_v.move_forward(static_cast<double>(arg1.int_v));
					}
					else arg0.canvas_v.move_forward(arg1.float_v);
					result.type = Interpreter_Value_Type::Void;
				}
				else if(std::strcmp(expression.function_call->name.begin_ptr,"logo_rotate_right") == 0) {
					if(expression.function_call->arguments.length != 2) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_rotate_right' takes exactly 2 arguments.");
						return {};
					}

					auto& arg0 = arg_values[0];
					if(arg0.type != Interpreter_Value_Type::Canvas) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_rotate_right' requires a canvas as a first argument.");
						return {};
					}

					const auto& arg1 = arg_values[1];
					if(arg1.type != Interpreter_Value_Type::Int && arg1.type != Interpreter_Value_Type::Float) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_save' requires an integer/float as a second argument.");
						return {};
					}

					if(arg1.type == Interpreter_Value_Type::Int) {
						arg0.canvas_v.rot += static_cast<double>(arg1.int_v);
					}
					else arg0.canvas_v.rot += arg1.float_v;
					result.type = Interpreter_Value_Type::Void;
				}
				else if(std::strcmp(expression.function_call->name.begin_ptr,"logo_save") == 0) {
					if(expression.function_call->arguments.length != 2) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_save' takes exactly 2 arguments.");
						return {};
					}

					auto& arg0 = arg_values[0];
					if(arg0.type != Interpreter_Value_Type::Canvas) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_save' requires a canvas as a first argument.");
						return {};
					}

					const auto& arg1 = arg_values[1];
					if(arg1.type != Interpreter_Value_Type::String) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'logo_save' requires a string as a second argument.");
						return {};
					}

					if(!arg0.canvas_v.save_as_bitmap(arg1.string_v)) return {};
					result.type = Interpreter_Value_Type::Void;
				}
				else if(std::strcmp(expression.function_call->name.begin_ptr,"dump") == 0) {
					if(expression.function_call->arguments.length != 1) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'dump' takes exactly 1 argument.");
						return {};
					}

					logo::print("[Line: %] ",expression.function_call->line_index);
					const auto& arg0 = arg_values[0];
					switch(arg0.type) {
						case Interpreter_Value_Type::Int:		logo::print("%\n",arg0.int_v);			break;
						case Interpreter_Value_Type::Float:		logo::print("%\n",arg0.float_v);		break;
						case Interpreter_Value_Type::Bool:		logo::print("%\n",arg0.bool_v);			break;
						case Interpreter_Value_Type::String:	logo::print("\"%\"\n",arg0.string_v);	break;
						case Interpreter_Value_Type::Void:		logo::print("(Void)\n");				break;
						case Interpreter_Value_Type::Canvas:
							logo::print("(Canvas){width = %,height = %,pos_x = %,pos_y = %,rot = %}\n",
										arg0.canvas_v.width,arg0.canvas_v.height,arg0.canvas_v.pos_x,arg0.canvas_v.pos_y,arg0.canvas_v.rot); break;
						case Interpreter_Value_Type::Reference: logo::print("(Reference)\n"); break; //@TODO: Print what that reference points to.
					}
					result.type = Interpreter_Value_Type::Void;
				}
				else {
					logo::report_interpreter_error(expression.function_call->line_index,"Function '%' has not been defined.",expression.function_call->name);
					return {};
				}
				return result;
			}
			default: logo::unreachable();
		}
	}

	static void destroy_scope_variables(Interpreter_Context* context,std::size_t start_index = 0) {
		for(std::size_t i = start_index;i < context->variables.length;i += 1) {
			auto& var = context->variables[i];
			if(var.value.type == Interpreter_Value_Type::Canvas) {
				var.value.canvas_v.destroy();
			}
		}
	}

	[[nodiscard]] static bool interpret_ast(Interpreter_Context* context,Array_View<Ast_Statement> statements) {
		for(const auto& statement : statements) {
			switch(statement.type) {
				case Ast_Statement_Type::Expression: {
					auto [value,success] = logo::compute_expression(context,statement.expression);
					if(!success) return false;
					if(value.type == Interpreter_Value_Type::Canvas) value.canvas_v.destroy();
					break;
				}
				case Ast_Statement_Type::Declaration: {
					for(const auto& var : context->variables) {
						if(std::strcmp(var.name.begin_ptr,statement.declaration.name.begin_ptr) == 0) {
							logo::report_interpreter_error(statement.line_index,"Variable '%' has already been defined.",statement.declaration.name);
							return false;
						}
					}

					Interpreter_Variable variable{};
					variable.name = statement.declaration.name;
					context->generation_counter += 1;
					variable.generation = context->generation_counter;
					auto [value,success] = logo::compute_expression(context,statement.declaration.initial_value_expr);
					if(!success) return false;

					if(value.type == Interpreter_Value_Type::Void) {
						logo::report_interpreter_error(statement.line_index,"Cannot assign value of type 'Void' to '%'.",statement.declaration.name);
						return false;
					}
					variable.value = value;

					if(!context->variables.push_back(variable)) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(variable));
						return false;
					}
					break;
				}
				case Ast_Statement_Type::Assignment: {
					bool found = false;
					for(auto& var : context->variables) {
						if(std::strcmp(var.name.begin_ptr,statement.assignment.name.begin_ptr) == 0) {
							auto [new_value,success] = logo::compute_expression(context,statement.assignment.value_expr);
							if(!success) return false;

							if(new_value.type == Interpreter_Value_Type::Void) {
								logo::report_interpreter_error(statement.assignment.line_index,"Cannot assign value of type 'Void' to '%'.",statement.assignment.name);
								return false;
							}

							//@TODO: Implement all compound assignment operators for reference accesses.
							if(statement.assignment.type == Ast_Assignment_Type::Assignment) {
								if(statement.assignment.is_through_reference) {
									if(var.value.type != Interpreter_Value_Type::Reference) {
										logo::report_interpreter_error(statement.assignment.line_index,"Cannot dereference '%' which is a non-reference.",statement.assignment.name);
										return false;
									}
									const auto& ref_info = var.value.reference_v;
									if(ref_info.var_index >= context->variables.length) {
										logo::report_interpreter_error(statement.assignment.line_index,"Dangling reference '%'.",statement.assignment.name);
										return {};
									}
									Interpreter_Variable& referenced_var = context->variables[ref_info.var_index];
									if(ref_info.generation != referenced_var.generation) {
										logo::report_interpreter_error(statement.assignment.line_index,"Dangling reference '%'.",statement.assignment.name);
										return {};
									}
									referenced_var.value = new_value;
								}
								else var.value = new_value;
							}
							else if(var.value.type == Interpreter_Value_Type::Int && new_value.type == Interpreter_Value_Type::Int) {
								var.value.type = Interpreter_Value_Type::Int;
								var.value.int_v = logo::compute_compound_assignment_operation(statement.assignment.type,var.value.int_v,new_value.int_v);
							}
							else if(var.value.type == Interpreter_Value_Type::Float && new_value.type == Interpreter_Value_Type::Float) {
								var.value.type = Interpreter_Value_Type::Float;
								var.value.float_v = logo::compute_compound_assignment_operation(statement.assignment.type,var.value.float_v,new_value.float_v);
							}
							else if(var.value.type == Interpreter_Value_Type::Int && new_value.type == Interpreter_Value_Type::Float) {
								var.value.type = Interpreter_Value_Type::Float;
								var.value.float_v = logo::compute_compound_assignment_operation(statement.assignment.type,static_cast<double>(var.value.int_v),new_value.float_v);
							}
							else if(var.value.type == Interpreter_Value_Type::Float && new_value.type == Interpreter_Value_Type::Int) {
								var.value.type = Interpreter_Value_Type::Float;
								var.value.float_v = logo::compute_compound_assignment_operation(statement.assignment.type,var.value.float_v,static_cast<double>(new_value.int_v));
							}
							else {
								logo::report_interpreter_error(statement.assignment.line_index,"Cannot perform compound assignment if the type of the variable being assigned to and the type of the expression on the right are not 'Int' or 'Float'.");
								return false;
							}

							found = true;
							break;
						}
					}
					if(!found) {
						logo::report_interpreter_error(statement.assignment.line_index,"Variable '%' does not exist.",statement.assignment.name);
						return false;
					}
					break;
				}
				case Ast_Statement_Type::If_Statement: {
					auto [condition_expr,success] = logo::compute_expression(context,statement.if_statement.condition_expr);
					if(!success) return false;

					if(condition_expr.type != Interpreter_Value_Type::Bool) {
						logo::report_interpreter_error(statement.line_index,"Condition in a 'if' statement must be of type 'Bool'.");
						return false;
					}

					std::size_t var_count = context->variables.length;
					if(condition_expr.bool_v && statement.if_statement.if_true_statements.length > 0) {
						if(!logo::interpret_ast(context,{statement.if_statement.if_true_statements.data,statement.if_statement.if_true_statements.length})) {
							return false;
						}
					}
					else if(!condition_expr.bool_v && statement.if_statement.if_false_statements.length > 0) {
						if(!logo::interpret_ast(context,{statement.if_statement.if_false_statements.data,statement.if_statement.if_false_statements.length})) {
							return false;
						}
					}
					logo::destroy_scope_variables(context,var_count);
					context->variables.length = var_count;
					break;
				}
				case Ast_Statement_Type::While_Statement: {
					auto condition_expr_option = logo::compute_expression(context,statement.while_statement.condition_expr);
					if(!condition_expr_option.has_value) return false;

					if(condition_expr_option.value.type != Interpreter_Value_Type::Bool) {
						logo::report_interpreter_error(statement.line_index,"Condition in a 'while' statement must be of type 'Bool'.");
						return false;
					}

					while(condition_expr_option.value.bool_v) {
						std::size_t var_count = context->variables.length;
						if(!logo::interpret_ast(context,{statement.while_statement.body_statements.data,statement.while_statement.body_statements.length})) {
							return false;
						}
						logo::destroy_scope_variables(context,var_count);
						context->variables.length = var_count;
						condition_expr_option = logo::compute_expression(context,statement.while_statement.condition_expr);
						if(!condition_expr_option.has_value) return false;
					}
					break;
				}
				case Ast_Statement_Type::Break_Statement: {
					logo::report_interpreter_error(statement.line_index,"'break' has not yet been implemented.");
					return false;
				}
				case Ast_Statement_Type::Continue_Statement: {
					logo::report_interpreter_error(statement.line_index,"'continue' has not yet been implemented.");
					return false;
				}
				default: logo::unreachable();
			}
		}
		return true;
	}

	bool interpret_ast(Array_View<Ast_Statement> statements) {
		Interpreter_Context context{};
		context.random_engine = std::mt19937_64(std::chrono::steady_clock::now().time_since_epoch().count());
		context.random_dist_0_1 = std::uniform_real_distribution(0.0,1.0);
		defer[&]{
			logo::destroy_scope_variables(&context);
			context.variables.destroy();
		};
		return logo::interpret_ast(&context,statements);
	}
}
