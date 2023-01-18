#include <cmath>
#include "debug.hpp"
#include "parser.hpp"
#include "interpreter.hpp"

namespace logo {
	enum struct Interpreter_Value_Type {
		Void,
		Int,
		Float,
		Bool,
		String
	};
	struct Interpreter_Value {
		Interpreter_Value_Type type;
		union {
			std::int64_t int_v;
			double float_v;
			bool bool_v;
			String_View string_v;
		};
		Interpreter_Value() : type(),int_v() {}
	};
	struct Interpreter_Variable {
		String_View name;
		Interpreter_Value value;
	};
	struct Interpreter_Context {
		Heap_Array<Interpreter_Value> tmp_values;
		Heap_Array<Interpreter_Variable> variables;
	};

	template<typename... Args>
	static void report_interpreter_error(std::size_t line_index,Format_String<std::type_identity_t<Args>...> format,Args&&... args) {
		logo::format_into(logo::write_char32_t_to_error_message,"[Runtime error] Line %: ",line_index);
		logo::format_into(logo::write_char32_t_to_error_message,format,std::forward<Args>(args)...);
		logo::write_char32_t_to_error_message('\n');
	}

	[[nodiscard]] static Option<Interpreter_Value> make_interpreter_value_from_ast_value(const Ast_Value& value) {
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

	[[nodiscard]] static Option<Interpreter_Value> compute_expression(Interpreter_Context* context,const Ast_Expression& expression) {
		switch(expression.type) {
			case Ast_Expression_Type::Value: {
				return logo::make_interpreter_value_from_ast_value(expression.value);
			}
			case Ast_Expression_Type::Unary_Prefix_Operator: {
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
						return value;
					}
					case Ast_Unary_Prefix_Operator_Type::Logical_Not: {
						if(value.type == Interpreter_Value_Type::Bool) value.bool_v = !value.bool_v;
						else {
							//@TODO: Print value that couldn't be negated.
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot logically negate a nonboolean value.");
							return {};
						}
						return value;
					}
					default: logo::unreachable();
				}
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
							result.bool_v = logo::compute_compare_operation<double>(expression.binary_operator->type,value0.int_v,value1.float_v);
						}
						else if(value0.type == Interpreter_Value_Type::Float && value1.type == Interpreter_Value_Type::Int) {
							result.bool_v = logo::compute_compare_operation<double>(expression.binary_operator->type,value0.float_v,value1.int_v);
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
				context->tmp_values.length = 0;

				for(const auto& arg_expr : expression.function_call->arguments) {
					auto [arg_value,success] = logo::compute_expression(context,*arg_expr);
					if(!success) return {};
					if(!context->tmp_values.push_back(arg_value)) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(arg_value));
						return {};
					}
				}

				Interpreter_Value result{};
				if(std::strcmp(expression.function_call->name.begin_ptr,"sin") == 0) {
					if(expression.function_call->arguments.length != 1) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'sin' takes exactly 1 argument.");
						return {};
					}

					const auto& arg0 = context->tmp_values[0];
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
				/*else if(std::strcmp(expression.function_call->name.begin_ptr,"print")) {
					result.type = Interpreter_Value_Type::Void;
					if(expression.function_call->arguments.length == 0) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'print' takes a format string as a 1 argument and an unspecified number of arguments after that.");
						return {};
					}

					const auto& format_string_arg = context->tmp_values[0];
					if(format_string_arg.type != Interpreter_Value_Type::String) {
						logo::report_interpreter_error(expression.function_call->line_index,"First argument to a function 'print' must be a string literal.");
						return {};
					}
					//if(expression.function_call->arguments.length == 1)
				}*/
				else {
					logo::report_interpreter_error(expression.function_call->line_index,"Function '%' has not been defined.",expression.function_call->name);
					return {};
				}
				return result;
			}
			default: logo::unreachable();
		}
	}

	bool interpret_ast(Array_View<Ast_Statement> statements) {
		Interpreter_Context context{};
		if(!context.tmp_values.reserve(16)) {
			Report_Error("Couldn't allocate % bytes of memory.",16 * sizeof(Interpreter_Value));
			return false;
		}
		defer[&]{context.tmp_values.destroy();};

		for(const auto& statement : statements) {
			switch(statement.type) {
				case Ast_Statement_Type::Expression: {
					auto [value,success] = logo::compute_expression(&context,statement.expression);
					if(!success) return false;

					switch(value.type) {
						case Interpreter_Value_Type::Int: {
							logo::print("Return value: (Int) %\n",value.int_v);
							break;
						}
						case Interpreter_Value_Type::Float: {
							logo::print("Return value: (Float) %\n",value.float_v);
							break;
						}
						case Interpreter_Value_Type::Bool: {
							logo::print("Return value: (Bool) %\n",value.bool_v);
							break;
						}
						case Interpreter_Value_Type::String: {
							logo::print("Return value: (String) %\n",value.string_v);
							break;
						}
						case Interpreter_Value_Type::Void: {
							logo::print("Return value: (Void)\n");
							break;
						}
					}
					break;
				}
				case Ast_Statement_Type::Declaration: {
					
					break;
				}
			}
		}
		return true;
	}
}
