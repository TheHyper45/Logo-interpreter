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
	struct Interpreter_Context;
	enum struct Interpreter_Value_Type {
		Void,
		Int,
		Float,
		Bool,
		String,
		Reference,
		Lvalue,
		Int_Or_Float, //This is only used in 'Interpreter_Builtin_Function' to denote argumnets that can be both ints or floats.
		Any //This is only used in 'Interpreter_Builtin_Function' to denote arguments of unspecified type.
	};
	struct Interpreter_Value_Reference {
		std::size_t var_index;
		std::size_t generation;
	};
	struct Interpreter_Value_Lvalue {
		std::size_t var_index;
	};
	struct Interpreter_Value {
		Interpreter_Value_Type type;
		union {
			std::int64_t int_v;
			double float_v;
			bool bool_v;
			String_View string_v;
			Interpreter_Value_Reference reference_v;
			Interpreter_Value_Lvalue lvalue_v;
		};
		Interpreter_Value() : type(),int_v() {}
	};
	struct Interpreter_Variable {
		String_View name;
		Interpreter_Value value;
		std::size_t generation;
	};
	struct Interpreter_Function {
		String_View name;
		Array_View<String_View> arguments;
		Array_View<Ast_Statement> body_statements;
	};
	struct Interpreter_Builtin_Function {
		String_View name;
		Static_Array<Interpreter_Value_Type,16> argument_types;
		Option<Interpreter_Value>(*func_ptr)(Interpreter_Context*,std::size_t,Array_View<Interpreter_Value>);
	};
	struct Interpreter_Context {
		std::mt19937_64 random_engine;
		std::uniform_real_distribution<double> random_dist_0_1;
		Heap_Array<Interpreter_Variable> variables;
		std::size_t current_function_scope_first_var_index;
		std::size_t generation_counter;
		Heap_Array<Interpreter_Function> functions;
		Canvas canvas;
		Heap_Array<Interpreter_Builtin_Function> builtin_functions;
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
				result.type = Interpreter_Value_Type::Lvalue;
				result.lvalue_v = {};
				for(std::size_t i = context->current_function_scope_first_var_index;i < context->variables.length;i += 1) {
					const auto& var = context->variables[i];
					if(std::strcmp(var.name.begin_ptr,value.identfier_name.begin_ptr) == 0) {
						result.lvalue_v.var_index = i;
						return result;
					}
				}
				logo::report_interpreter_error(value.line_index,"Identifier '%' does not exist.",value.identfier_name);
				return {};
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

	enum struct Interpreter_Status {
		Success,
		Error,
		Function_Return,
		Break,
		Continue
	};
	struct Interpreter_Result {
		Interpreter_Status status;
		Interpreter_Value value;
		Interpreter_Result(Interpreter_Status _status) : status(_status),value() {}
		Interpreter_Result(const Interpreter_Value& _value) : status(Interpreter_Status::Success),value(_value) {}
	};

	[[nodiscard]] static Interpreter_Result interpret_ast(Interpreter_Context* context,Array_View<Ast_Statement> statements,bool is_function_scope,bool inside_loop);

	[[nodiscard]] static Option<Interpreter_Value> compute_expression(Interpreter_Context* context,const Ast_Expression& expression) {
		switch(expression.type) {
			case Ast_Expression_Type::Value: {
				return logo::make_interpreter_value_from_ast_value(context,expression.value);
			}
			case Ast_Expression_Type::Unary_Prefix_Operator: {
				if(expression.unary_prefix_operator->type == Ast_Unary_Prefix_Operator_Type::Parent_Scope_Access) {
					const auto* child = expression.unary_prefix_operator->child;
					if(child->type != Ast_Expression_Type::Value) {
						logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"'parent scope access' operator can only be applied on an identifier.");
						return {};
					}
					if(child->value.type != Ast_Value_Type::Identifier) {
						logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"'parent scope access' operator can only be applied on an identifier.");
						return {};
					}
					
					for(std::size_t i = context->variables.length;i > 0;i -= 1) {
						const auto& var = context->variables[i - 1];
						if(std::strcmp(child->value.identfier_name.begin_ptr,var.name.begin_ptr) == 0) {
							Interpreter_Value result{};
							result.type = Interpreter_Value_Type::Lvalue;
							result.lvalue_v = {};
							result.lvalue_v.var_index = i - 1;
							return result;
						}
					}
					logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Variable '%' does not exist.",child->value.identfier_name);
					return {};
				}

				auto [value,success] = logo::compute_expression(context,*expression.unary_prefix_operator->child);
				if(!success) return {};
				switch(expression.unary_prefix_operator->type) {
					case Ast_Unary_Prefix_Operator_Type::Plus:
					case Ast_Unary_Prefix_Operator_Type::Minus: {
						if(value.type == Interpreter_Value_Type::Lvalue) {
							Interpreter_Value copy = value;
							value = context->variables[copy.lvalue_v.var_index].value;
						}

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
						if(value.type == Interpreter_Value_Type::Lvalue) {
							Interpreter_Value copy = value;
							value = context->variables[copy.lvalue_v.var_index].value;
						}

						if(value.type == Interpreter_Value_Type::Bool) value.bool_v = !value.bool_v;
						else {
							//@TODO: Print value that couldn't be negated.
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot logically negate a nonboolean value.");
							return {};
						}
						break;
					}
					case Ast_Unary_Prefix_Operator_Type::Reference: {
						if(value.type != Interpreter_Value_Type::Lvalue) {
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot take a reference to an object that is not an lvalue.");
							return {};
						}

						auto var_index = value.lvalue_v.var_index;
						value.type = Interpreter_Value_Type::Reference;
						value.reference_v = {};
						value.reference_v.var_index = var_index;
						logo::assert(value.reference_v.var_index < context->variables.length);
						value.reference_v.generation = context->variables[value.reference_v.var_index].generation;
						break;
					}
					case Ast_Unary_Prefix_Operator_Type::Dereference: {
						if(value.type == Interpreter_Value_Type::Lvalue) {
							Interpreter_Value copy = value;
							value = context->variables[copy.lvalue_v.var_index].value;
						}
						if(value.type != Interpreter_Value_Type::Reference) {
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Cannot dereference an object that is not a reference.");
							return {};
						}

						if(value.reference_v.var_index >= context->variables.length) {
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Dangling reference.");
							return {};
						}
						const auto& referenced_var = context->variables[value.reference_v.var_index];
						if(value.reference_v.generation != referenced_var.generation) {
							logo::report_interpreter_error(expression.unary_prefix_operator->line_index,"Dangling reference.");
							return {};
						}

						auto var_index = value.reference_v.var_index;
						value.type = Interpreter_Value_Type::Lvalue;
						value.lvalue_v = {};
						value.lvalue_v.var_index = var_index;
						break;
					}
					default: logo::unreachable();
				}
				return value;
			}
			case Ast_Expression_Type::Binary_Operator: {
				auto [value0,success0] = logo::compute_expression(context,*expression.binary_operator->left);
				if(!success0) return {};
				if(value0.type == Interpreter_Value_Type::Lvalue) {
					Interpreter_Value copy = value0;
					value0 = context->variables[copy.lvalue_v.var_index].value;
				}

				auto [value1,success1] = logo::compute_expression(context,*expression.binary_operator->right);
				if(!success1) return {};
				if(value1.type == Interpreter_Value_Type::Lvalue) {
					Interpreter_Value copy = value1;
					value1 = context->variables[copy.lvalue_v.var_index].value;
				}

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
							result.float_v = logo::compute_arithmetic_operation(expression.binary_operator->type,static_cast<double>(value0.int_v),value1.float_v);
						}
						else if(value0.type == Interpreter_Value_Type::Float && value1.type == Interpreter_Value_Type::Int) {
							result.type = Interpreter_Value_Type::Float;
							result.float_v = logo::compute_arithmetic_operation(expression.binary_operator->type,value0.float_v,static_cast<double>(value1.int_v));
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
				for(const auto& arg_expr : expression.function_call->arguments) {
					auto [arg_value,success] = logo::compute_expression(context,*arg_expr);
					if(!success) return {};

					if(arg_value.type == Interpreter_Value_Type::Void) {
						logo::report_interpreter_error(expression.function_call->line_index,"Cannot assign value of type 'Void' to a function parameter.");
						return {};
					}
					if(arg_value.type == Interpreter_Value_Type::Lvalue) {
						auto& referenced_value = context->variables[arg_value.lvalue_v.var_index].value;
						arg_value = referenced_value;
					}
					if(!arg_values.push_back(arg_value)) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function '%' cannot take more than 16 arguments.",expression.function_call->name);
						return {};
					}
				}

				//This function is hardcoded because the interpreter doesn't support variadic functions.
				if(std::strcmp(expression.function_call->name.begin_ptr,"print") == 0) {
					if(expression.function_call->arguments.length == 0) {
						logo::report_interpreter_error(expression.function_call->line_index,"Function 'print' takes at least 1 argument.");
						return {};
					}
					const auto& format_arg = arg_values[0];
					if(format_arg.type != Interpreter_Value_Type::String) {
						logo::report_interpreter_error(expression.function_call->line_index,"Argument 0 to function 'print' must be a string.");
						return {};
					}

					Static_Array<String_Format_Arg,15> format_args{};
					for(std::size_t i = 1;i < arg_values.length;i += 1) {
						const auto& arg_value = arg_values[i];

						String_Format_Arg format_arg{};
						switch(arg_value.type) {
							case Interpreter_Value_Type::Int: format_arg = logo::make_string_format_arg(arg_value.int_v); break;
							case Interpreter_Value_Type::Float: format_arg = logo::make_string_format_arg(arg_value.float_v); break;
							case Interpreter_Value_Type::Bool: format_arg = logo::make_string_format_arg(arg_value.bool_v); break;
							case Interpreter_Value_Type::String: format_arg = logo::make_string_format_arg(arg_value.string_v); break;
							case Interpreter_Value_Type::Reference: format_arg = logo::make_string_format_arg("(Reference)"); break; //@TODO: Print what that reference points to.
							default: logo::unreachable();
						}

						if(!format_args.push_back(format_arg)) {
							Report_Error("Couldn't create a formatting argument.");
							return {};
						}
					}
					auto format_result = logo::format_args_into(logo::print_stdout_char32_t,format_arg.string_v,{format_args.data,format_args.length});
					if(format_result.external_failure) {
						Report_Error("Couldn't execute a print statement.");
						return {};
					}
					if(format_result.count_of_args != format_result.count_of_arguments_processed) {
						logo::report_interpreter_error(expression.function_call->line_index,"The number of argumnets given to 'print' does nor match the numnber of markers in the format string.");
						return {};
					}

					Interpreter_Value result{};
					result.type = Interpreter_Value_Type::Void;
					return result;
				}

				bool function_overload_exist = false;
				for(const auto& builtin_function : context->builtin_functions) {
					if(std::strcmp(expression.function_call->name.begin_ptr,builtin_function.name.begin_ptr) != 0) continue;
					function_overload_exist = true;
					if(expression.function_call->arguments.length != builtin_function.argument_types.length) continue;

					for(std::size_t i = 0;i < builtin_function.argument_types.length;i += 1) {
						Interpreter_Value_Type arg_type = arg_values[i].type;
						Interpreter_Value_Type required_type = builtin_function.argument_types[i];
						if(required_type != Interpreter_Value_Type::Any && required_type != arg_type) {
							if(required_type != Interpreter_Value_Type::Int_Or_Float || (arg_type != Interpreter_Value_Type::Int && arg_type != Interpreter_Value_Type::Float)) {
								Array_String<64> type_name{};
								switch(required_type) {
									case Interpreter_Value_Type::Int: type_name.append("'Int'"); break;
									case Interpreter_Value_Type::Float: type_name.append("'Float'"); break;
									case Interpreter_Value_Type::Bool: type_name.append("'Bool'"); break;
									case Interpreter_Value_Type::String: type_name.append("'String'"); break;
									case Interpreter_Value_Type::Reference: type_name.append("'Reference'"); break;
									case Interpreter_Value_Type::Int_Or_Float: type_name.append("'Int' or 'Float'"); break;
									default: logo::unreachable();
								}
								logo::report_interpreter_error(expression.function_call->line_index,"Argument % to function '%' must be of type %.",i,expression.function_call->name,String_View(type_name.buffer,type_name.byte_length));
								return {};
							}
						}
					}
					return builtin_function.func_ptr(context,expression.function_call->line_index,{arg_values.data,arg_values.length});
				}

				for(auto& function : context->functions) {
					if(std::strcmp(function.name.begin_ptr,expression.function_call->name.begin_ptr) != 0) continue;
					function_overload_exist = true;
					if(expression.function_call->arguments.length != function.arguments.length) continue;

					auto var_count = context->variables.length;
					auto func_count = context->functions.length;
					auto copy_var_index = context->current_function_scope_first_var_index;
					context->current_function_scope_first_var_index = context->variables.length;

					for(std::size_t i = 0;i < arg_values.length;i += 1) {
						Interpreter_Variable variable{};
						variable.name = function.arguments[i];
						context->generation_counter += 1;
						variable.generation = context->generation_counter;
						variable.value = arg_values[i];
						if(!context->variables.push_back(variable)) {
							Report_Error("Couldn't allocate % bytes of memory.",sizeof(variable));
							return {};
						}
					}

					auto func_result = logo::interpret_ast(context,function.body_statements,true,false);

					context->current_function_scope_first_var_index = copy_var_index;
					context->functions.length = func_count;
					context->variables.length = var_count;

					if(func_result.status == Interpreter_Status::Error) {};
					if(func_result.status == Interpreter_Status::Function_Return) {
						return func_result.value;
					}
					logo::assert(func_result.status != Interpreter_Status::Success);
				}
				if(function_overload_exist) logo::report_interpreter_error(expression.function_call->line_index,"Function '%' does not take % arguments.",expression.function_call->name,arg_values.length);
				else logo::report_interpreter_error(expression.function_call->line_index,"Function '%' does not exist.",expression.function_call->name);
				return {};
			}
			case Ast_Expression_Type::Array_Access: {
				logo::report_interpreter_error(expression.array_access->line_index,"Arrays are not yet implemented.");
				return {};
			}
			default: logo::unreachable();
		}
	}

	[[nodiscard]] static Interpreter_Result interpret_ast(Interpreter_Context* context,Array_View<Ast_Statement> statements,bool is_function_scope,bool inside_loop) {
		for(const auto& statement : statements) {
			switch(statement.type) {
				case Ast_Statement_Type::Expression: {
					auto [value,success] = logo::compute_expression(context,statement.expression);
					if(!success) return Interpreter_Status::Error;
					break;
				}
				case Ast_Statement_Type::Declaration: {
					for(std::size_t i = context->current_function_scope_first_var_index;i < context->variables.length;i += 1) {
						const auto& var = context->variables[i];
						if(std::strcmp(var.name.begin_ptr,statement.declaration.name.begin_ptr) == 0) {
							logo::report_interpreter_error(statement.line_index,"Variable '%' has already been defined.",statement.declaration.name);
							return Interpreter_Status::Error;
						}
					}

					Interpreter_Variable variable{};
					variable.name = statement.declaration.name;
					context->generation_counter += 1;
					variable.generation = context->generation_counter;
					auto [value,success] = logo::compute_expression(context,statement.declaration.initial_value_expr);
					if(!success) return Interpreter_Status::Error;

					if(value.type == Interpreter_Value_Type::Void) {
						logo::report_interpreter_error(statement.line_index,"Cannot assign value of type 'Void' to '%'.",statement.declaration.name);
						return Interpreter_Status::Error;
					}
					if(value.type == Interpreter_Value_Type::Lvalue) {
						Interpreter_Value copy = value;
						value = context->variables[copy.lvalue_v.var_index].value;
					}
					variable.value = value;

					if(!context->variables.push_back(variable)) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(variable));
						return Interpreter_Status::Error;
					}
					break;
				}
				case Ast_Statement_Type::Function_Definition: {
					Interpreter_Function function{};
					function.name = statement.function_definition.name;
					function.arguments = Array_View<String_View>(statement.function_definition.function_arguments.data,statement.function_definition.function_arguments.length);
					function.body_statements = Array_View<Ast_Statement>(statement.function_definition.body_statements.data,statement.function_definition.body_statements.length);

					if(!context->functions.push_back(function)) {
						Report_Error("Couldn't allocate % bytes of memory.",sizeof(function));
						return Interpreter_Status::Error;
					}
					break;
				}
				case Ast_Statement_Type::Assignment: {
					auto [lvalue_value,success0] = logo::compute_expression(context,statement.assignment.lvalue_expr);
					if(!success0) return Interpreter_Status::Error;

					if(lvalue_value.type != Interpreter_Value_Type::Lvalue) {
						logo::report_interpreter_error(statement.assignment.line_index,"Cannot assign a value to a non-lvalue.");
						return Interpreter_Status::Error;
					}

					auto [rvalue_value,success1] = logo::compute_expression(context,statement.assignment.rvalue_expr);
					if(!success1) return Interpreter_Status::Error;

					if(rvalue_value.type == Interpreter_Value_Type::Void) {
						logo::report_interpreter_error(statement.assignment.line_index,"Cannot assign value of type 'Void'.");
						return Interpreter_Status::Error;
					}
					if(rvalue_value.type == Interpreter_Value_Type::Lvalue) {
						Interpreter_Value copy = rvalue_value;
						rvalue_value = context->variables[copy.lvalue_v.var_index].value;
					}

					auto& target_lvalue = context->variables[lvalue_value.lvalue_v.var_index].value;
					if(statement.assignment.type == Ast_Assignment_Type::Assignment) {
						target_lvalue = rvalue_value;
					}
					else if(target_lvalue.type == Interpreter_Value_Type::Int && rvalue_value.type == Interpreter_Value_Type::Int) {
						target_lvalue.type = Interpreter_Value_Type::Int;
						target_lvalue.int_v = logo::compute_compound_assignment_operation(statement.assignment.type,target_lvalue.int_v,rvalue_value.int_v);
					}
					else if(target_lvalue.type == Interpreter_Value_Type::Float && rvalue_value.type == Interpreter_Value_Type::Float) {
						target_lvalue.type = Interpreter_Value_Type::Float;
						target_lvalue.float_v = logo::compute_compound_assignment_operation(statement.assignment.type,target_lvalue.float_v,rvalue_value.float_v);
					}
					else if(target_lvalue.type == Interpreter_Value_Type::Int && rvalue_value.type == Interpreter_Value_Type::Float) {
						target_lvalue.type = Interpreter_Value_Type::Float;
						target_lvalue.float_v = logo::compute_compound_assignment_operation(statement.assignment.type,static_cast<double>(target_lvalue.int_v),rvalue_value.float_v);
					}
					else if(target_lvalue.type == Interpreter_Value_Type::Float && rvalue_value.type == Interpreter_Value_Type::Int) {
						target_lvalue.type = Interpreter_Value_Type::Float;
						target_lvalue.float_v = logo::compute_compound_assignment_operation(statement.assignment.type,target_lvalue.float_v,static_cast<double>(rvalue_value.int_v));
					}
					else {
						logo::report_interpreter_error(statement.assignment.line_index,"Cannot perform compound assignment if the type of the variable being assigned to and the type of the expression on the right are not 'Int' or 'Float'.");
						return Interpreter_Status::Error;
					}
					break;
				}
				case Ast_Statement_Type::If_Statement: {
					auto [condition_expr,success] = logo::compute_expression(context,statement.if_statement.condition_expr);
					if(!success) return Interpreter_Status::Error;

					if(condition_expr.type != Interpreter_Value_Type::Bool) {
						logo::report_interpreter_error(statement.line_index,"Condition in a 'if' statement must be of type 'Bool'.");
						return Interpreter_Status::Error;
					}

					std::size_t var_count = context->variables.length;
					std::size_t func_count = context->functions.length;

					Interpreter_Result result{Interpreter_Status::Error};
					if(condition_expr.bool_v && statement.if_statement.if_true_statements.length > 0) {
						result = logo::interpret_ast(context,{statement.if_statement.if_true_statements.data,statement.if_statement.if_true_statements.length},false,inside_loop);
						if(result.status == Interpreter_Status::Error) return Interpreter_Status::Error;
					}
					else if(!condition_expr.bool_v && statement.if_statement.if_false_statements.length > 0) {
						result = logo::interpret_ast(context,{statement.if_statement.if_false_statements.data,statement.if_statement.if_false_statements.length},false,inside_loop);
						if(result.status == Interpreter_Status::Error) return Interpreter_Status::Error;
					}

					context->functions.length = func_count;
					context->variables.length = var_count;

					if(result.status == Interpreter_Status::Function_Return) return result;
					if(result.status == Interpreter_Status::Break) return Interpreter_Status::Break;
					if(result.status == Interpreter_Status::Continue) return Interpreter_Status::Continue;
					break;
				}
				case Ast_Statement_Type::While_Statement: {
					auto condition_expr_option = logo::compute_expression(context,statement.while_statement.condition_expr);
					if(!condition_expr_option.has_value) return Interpreter_Status::Error;

					if(condition_expr_option.value.type != Interpreter_Value_Type::Bool) {
						logo::report_interpreter_error(statement.line_index,"Condition in a 'while' statement must be of type 'Bool'.");
						return Interpreter_Status::Error;
					}

					while(condition_expr_option.value.bool_v) {
						std::size_t var_count = context->variables.length;
						std::size_t func_count = context->functions.length;

						auto result = logo::interpret_ast(context,{statement.while_statement.body_statements.data,statement.while_statement.body_statements.length},false,true);
						if(result.status == Interpreter_Status::Error) return Interpreter_Status::Error;

						context->functions.length = func_count;
						context->variables.length = var_count;

						if(result.status == Interpreter_Status::Function_Return) return result;
						if(result.status == Interpreter_Status::Break) break;

						condition_expr_option = logo::compute_expression(context,statement.while_statement.condition_expr);
						if(!condition_expr_option.has_value) return Interpreter_Status::Error;
					}
					break;
				}
				case Ast_Statement_Type::For_Statement: {
					auto [lower_bound_value,success0] = logo::compute_expression(context,statement.for_statement.start_expr);
					if(!success0) return Interpreter_Status::Error;
					if(lower_bound_value.type == Interpreter_Value_Type::Lvalue) {
						Interpreter_Value copy = lower_bound_value;
						lower_bound_value = context->variables[copy.lvalue_v.var_index].value;
					}
					if(lower_bound_value.type != Interpreter_Value_Type::Int) {
						logo::report_interpreter_error(statement.line_index,"Starting index in a 'for' loop must be an integer.");
						return Interpreter_Status::Error;
					}

					auto [upper_bound_value,success1] = logo::compute_expression(context,statement.for_statement.end_expr);
					if(!success1) return Interpreter_Status::Error;
					if(upper_bound_value.type == Interpreter_Value_Type::Lvalue) {
						Interpreter_Value copy = upper_bound_value;
						upper_bound_value = context->variables[copy.lvalue_v.var_index].value;
					}
					if(upper_bound_value.type != Interpreter_Value_Type::Int) {
						logo::report_interpreter_error(statement.line_index,"Ending index in a 'for' loop must be an integer.");
						return Interpreter_Status::Error;
					}

					if(lower_bound_value.int_v < upper_bound_value.int_v) {
						Interpreter_Variable iterator_variable{};
						iterator_variable.name = statement.for_statement.iterator_identifier;
						context->generation_counter += 1;
						iterator_variable.generation = context->generation_counter;
						iterator_variable.value = lower_bound_value;

						if(!context->variables.push_back(iterator_variable)) {
							Report_Error("Couldn't allocate % bytes of memory.",sizeof(iterator_variable));
							return Interpreter_Status::Error;
						}
						std::size_t iterator_var_index = context->variables.length - 1;

						for(std::int64_t i = lower_bound_value.int_v;i < upper_bound_value.int_v;i += 1) {
							std::size_t var_count = context->variables.length;
							std::size_t func_count = context->functions.length;

							auto result = logo::interpret_ast(context,{statement.for_statement.body_statements.data,statement.for_statement.body_statements.length},false,true);
							if(result.status == Interpreter_Status::Error) return Interpreter_Status::Error;

							context->functions.length = func_count;
							context->variables.length = var_count;

							if(result.status == Interpreter_Status::Function_Return) return result;
							if(result.status == Interpreter_Status::Break) break;

							context->variables[iterator_var_index].value.int_v += 1;
						}
						context->variables.pop_back();
					}
					break;
				}
				case Ast_Statement_Type::Break_Statement: {
					return Interpreter_Status::Break;
				}
				case Ast_Statement_Type::Continue_Statement: {
					return Interpreter_Status::Continue;
				}
				case Ast_Statement_Type::Return_Statement: {
					Interpreter_Result return_value = Interpreter_Status::Function_Return;
					if(statement.return_statement.return_value) {
						auto [value,success] = logo::compute_expression(context,*statement.return_statement.return_value);
						if(!success) return Interpreter_Status::Error;
						return_value.value = value;
					}
					else return_value.value.type = Interpreter_Value_Type::Void;
					return return_value;
				}
				default: logo::unreachable();
			}
		}
		if(is_function_scope) {
			Interpreter_Result return_value = Interpreter_Status::Function_Return;
			return_value.value.type = Interpreter_Value_Type::Void;
			return return_value;
		}
		return Interpreter_Status::Success;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_typename(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value> values) {
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::String;
		const auto& arg = values[0];
		switch(arg.type) {
			case Interpreter_Value_Type::Int: result.string_v = "Int"; return result;
			case Interpreter_Value_Type::Float: result.string_v = "Float"; return result;
			case Interpreter_Value_Type::Bool: result.string_v = "Bool"; return result;
			case Interpreter_Value_Type::String: result.string_v = "String"; return result;
			case Interpreter_Value_Type::Reference: result.string_v = "Reference"; return result;
			default: logo::unreachable();
		}
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_int(Interpreter_Context* context,std::size_t line_index,Array_View<Interpreter_Value> values) {
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Int;
		const auto& arg = values[0];
		switch(arg.type) {
			case Interpreter_Value_Type::Int: result.int_v = arg.int_v; break;
			case Interpreter_Value_Type::Float: result.int_v = static_cast<std::int64_t>(arg.float_v); break;
			case Interpreter_Value_Type::Bool: result.int_v = static_cast<std::int64_t>(arg.bool_v); break;
			case Interpreter_Value_Type::String: {
				logo::report_interpreter_error(line_index,"Cannot convert values of type 'String' to int.");
				return {};
			}
			case Interpreter_Value_Type::Reference: {
				logo::report_interpreter_error(line_index,"Cannot convert values of type 'Reference' to int.");
				return {};
			}
			default: logo::unreachable();
		}
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_float(Interpreter_Context* context,std::size_t line_index,Array_View<Interpreter_Value> values) {
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Float;
		const auto& arg = values[0];
		switch(arg.type) {
			case Interpreter_Value_Type::Int: result.float_v = static_cast<double>(arg.int_v); break;
			case Interpreter_Value_Type::Float: result.float_v = arg.float_v; break;
			case Interpreter_Value_Type::Bool: result.float_v = static_cast<double>(arg.bool_v); break;
			case Interpreter_Value_Type::String: {
				logo::report_interpreter_error(line_index,"Cannot convert values of type 'String' to int.");
				return {};
			}
			case Interpreter_Value_Type::Reference: {
				logo::report_interpreter_error(line_index,"Cannot convert values of type 'Reference' to int.");
				return {};
			}
			default: logo::unreachable();
		}
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_pi(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value>) {
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Float;
		result.float_v = PI;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_random(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value>) {
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Float;
		result.float_v = context->random_dist_0_1(context->random_engine);
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_init(Interpreter_Context* context,std::size_t line_index,Array_View<Interpreter_Value> values) {
		const auto& arg0 = values[0];
		if(arg0.int_v > std::numeric_limits<std::int32_t>::max()) {
			logo::report_interpreter_error(line_index,"Argument 0 to function 'init' must be an intger from interval (0,%].",std::numeric_limits<std::int32_t>::max());
			return {};
		}
		const auto& arg1 = values[1];
		if(arg1.int_v > std::numeric_limits<std::int32_t>::max()) {
			logo::report_interpreter_error(line_index,"Argument 1 to function 'init' must be an intger from interval (0,%].",std::numeric_limits<std::int32_t>::max());
			return {};
		}

		if(!context->canvas.init(arg0.int_v,arg1.int_v)) return {};
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_init_with_colors(Interpreter_Context* context,std::size_t line_index,Array_View<Interpreter_Value> values) {
		const auto& arg0 = values[0];
		if(arg0.int_v > std::numeric_limits<std::int32_t>::max()) {
			logo::report_interpreter_error(line_index,"Argument 0 to function 'init' must be an intger from interval (0,%].",std::numeric_limits<std::int32_t>::max());
			return {};
		}
		const auto& arg1 = values[1];
		if(arg1.int_v > std::numeric_limits<std::int32_t>::max()) {
			logo::report_interpreter_error(line_index,"Argument 1 to function 'init' must be an intger from interval (0,%].",std::numeric_limits<std::int32_t>::max());
			return {};
		}

		if(values[2].int_v < 0 || values[2].int_v > 255) {
			logo::report_interpreter_error(line_index,"Argument 2 to function 'init' must be from range [0,255].");
			return {};
		}
		if(values[3].int_v < 0 || values[3].int_v > 255) {
			logo::report_interpreter_error(line_index,"Argument 3 to function 'init' must be from range [0,255].");
			return {};
		}
		if(values[4].int_v < 0 || values[4].int_v > 255) {
			logo::report_interpreter_error(line_index,"Argument 4 to function 'init' must be from range [0,255].");
			return {};
		}

		Color background_color = {static_cast<std::uint8_t>(values[2].int_v),static_cast<std::uint8_t>(values[3].int_v),static_cast<std::uint8_t>(values[4].int_v)};
		if(!context->canvas.init(arg0.int_v,arg1.int_v,background_color)) return {};
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_forward(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value> values) {
		const auto& arg = values[0];
		if(arg.type == Interpreter_Value_Type::Int) context->canvas.move_forward(static_cast<double>(arg.int_v));
		else context->canvas.move_forward(arg.float_v);
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_backwards(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value> values) {
		const auto& arg = values[0];
		if(arg.type == Interpreter_Value_Type::Int) context->canvas.move_forward(-static_cast<double>(arg.int_v));
		else context->canvas.move_forward(-arg.float_v);
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_right(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value> values) {
		const auto& arg = values[0];
		if(arg.type == Interpreter_Value_Type::Int) context->canvas.rot -= static_cast<double>(arg.int_v);
		else context->canvas.rot -= arg.float_v;
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_left(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value> values) {
		const auto& arg = values[0];
		if(arg.type == Interpreter_Value_Type::Int) context->canvas.rot += static_cast<double>(arg.int_v);
		else context->canvas.rot += arg.float_v;
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_setpos(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value> values) {
		const auto& arg0 = values[0];
		if(arg0.type == Interpreter_Value_Type::Int) context->canvas.pos_x = static_cast<double>(arg0.int_v);
		else context->canvas.pos_x = arg0.float_v;

		const auto& arg1 = values[1];
		if(arg1.type == Interpreter_Value_Type::Int) context->canvas.pos_y = static_cast<double>(arg1.int_v);
		else context->canvas.pos_y = arg1.float_v;

		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_setrot(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value> values) {
		const auto& arg = values[0];
		if(arg.type == Interpreter_Value_Type::Int) context->canvas.rot = static_cast<double>(arg.int_v);
		else context->canvas.rot = arg.float_v;

		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_getposx(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value>) {
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Float;
		result.float_v = context->canvas.pos_x;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_getposy(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value>) {
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Float;
		result.float_v = context->canvas.pos_y;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_getrot(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value>) {
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Float;
		result.float_v = context->canvas.rot;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_penup(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value>) {
		context->canvas.is_pen_down = false;
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_pendown(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value>) {
		context->canvas.is_pen_down = true;
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_pencolor(Interpreter_Context* context,std::size_t line_index,Array_View<Interpreter_Value> values) {
		if(values[0].int_v < 0 || values[0].int_v > 255) {
			logo::report_interpreter_error(line_index,"Argument 0 to function 'pencolor' must be from range [0,255].");
			return {};
		}
		if(values[1].int_v < 0 || values[1].int_v > 255) {
			logo::report_interpreter_error(line_index,"Argument 1 to function 'pencolor' must be from range [0,255].");
			return {};
		}
		if(values[2].int_v < 0 || values[2].int_v > 255) {
			logo::report_interpreter_error(line_index,"Argument 2 to function 'pencolor' must be from range [0,255].");
			return {};
		}
		context->canvas.pen_color = {static_cast<std::uint8_t>(values[0].int_v),static_cast<std::uint8_t>(values[1].int_v),static_cast<std::uint8_t>(values[2].int_v)};
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	[[nodiscard]] static Option<Interpreter_Value> interpreter_builtin_function_save(Interpreter_Context* context,std::size_t,Array_View<Interpreter_Value> values) {
		const auto& arg = values[0];
		if(!context->canvas.save_as_bitmap(arg.string_v)) return {};
		Interpreter_Value result{};
		result.type = Interpreter_Value_Type::Void;
		return result;
	}

	bool interpret_ast(Array_View<Ast_Statement> statements) {
		Interpreter_Context context{};
		context.random_engine = std::mt19937_64(std::chrono::steady_clock::now().time_since_epoch().count());
		context.random_dist_0_1 = std::uniform_real_distribution(0.0,1.0);
		context.current_function_scope_first_var_index = 0;
		defer[&]{
			context.builtin_functions.destroy();
			context.canvas.destroy();
			context.functions.destroy();
			context.variables.destroy();
		};

#define LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(CONTEXT,NAME,FUNC)\
	if(!(CONTEXT).builtin_functions.push_back(Interpreter_Builtin_Function{(NAME),{Interpreter_Value_Type::Int_Or_Float},\
	[](Interpreter_Context*,std::size_t,Array_View<Interpreter_Value> values) -> Option<Interpreter_Value> {\
		Interpreter_Value result{};\
		result.type = Interpreter_Value_Type::Float;\
		const auto& arg = values[0];\
		if(arg.type == Interpreter_Value_Type::Int) result.float_v = static_cast<double>((FUNC)(arg.int_v));\
		else result.float_v = (FUNC)(arg.float_v);\
		return result;\
	}})) {\
		Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));\
		return false;\
	}

#define LOGO_DEFINE_DOUBLE_ARG_MATH_BUILTIN_FUNCTION(CONTEXT,NAME,FUNC)\
	if(!(CONTEXT).builtin_functions.push_back(Interpreter_Builtin_Function{(NAME),{Interpreter_Value_Type::Int_Or_Float,Interpreter_Value_Type::Int_Or_Float},\
	[](Interpreter_Context*,std::size_t,Array_View<Interpreter_Value> values) -> Option<Interpreter_Value> {\
		Interpreter_Value result{};\
		result.type = Interpreter_Value_Type::Float;\
		double arg0_v = 0.0,arg1_v = 0.0;\
		const auto& arg0 = values[0];\
		if(arg0.type == Interpreter_Value_Type::Int) arg0_v = static_cast<double>(arg0.int_v);\
		else arg0_v = arg0.float_v;\
		const auto& arg1 = values[1];\
		if(arg1.type == Interpreter_Value_Type::Int) arg1_v = static_cast<double>(arg1.int_v);\
		else arg1_v = arg1.float_v;\
		result.float_v = (FUNC)(arg0_v,arg1_v);\
		return result;\
	}})) {\
		Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));\
		return false;\
	}

		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"sin",std::sin);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"cos",std::cos);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"tan",std::tan);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"asin",std::asin);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"acos",std::acos);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"atan",std::atan);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"sinh",std::sinh);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"cosh",std::cosh);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"tanh",std::tanh);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"asinh",std::asinh);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"acosh",std::acosh);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"atanh",std::atanh);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"abs",std::abs);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"sqrt",std::sqrt);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"cbrt",std::cbrt);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"gamma",std::tgamma);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"radians",logo::radians);
		LOGO_DEFINE_SINGLE_ARG_MATH_BUILTIN_FUNCTION(context,"degrees",logo::degrees);
		LOGO_DEFINE_DOUBLE_ARG_MATH_BUILTIN_FUNCTION(context,"min",logo::float_min);
		LOGO_DEFINE_DOUBLE_ARG_MATH_BUILTIN_FUNCTION(context,"max",logo::float_max);

		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"typename",{Interpreter_Value_Type::Any},logo::interpreter_builtin_function_typename})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"int",{Interpreter_Value_Type::Any},logo::interpreter_builtin_function_int})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"float",{Interpreter_Value_Type::Any},logo::interpreter_builtin_function_float})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"pi",{},logo::interpreter_builtin_function_pi})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"random",{},logo::interpreter_builtin_function_random})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"init",{Interpreter_Value_Type::Int,Interpreter_Value_Type::Int},logo::interpreter_builtin_function_init})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"init",{Interpreter_Value_Type::Int,Interpreter_Value_Type::Int,
												Interpreter_Value_Type::Int,Interpreter_Value_Type::Int,Interpreter_Value_Type::Int},logo::interpreter_builtin_function_init_with_colors})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"forward",{Interpreter_Value_Type::Int_Or_Float},logo::interpreter_builtin_function_forward})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"backwards",{Interpreter_Value_Type::Int_Or_Float},logo::interpreter_builtin_function_backwards})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"backward",{Interpreter_Value_Type::Int_Or_Float},logo::interpreter_builtin_function_backwards})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"right",{Interpreter_Value_Type::Int_Or_Float},logo::interpreter_builtin_function_right})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"left",{Interpreter_Value_Type::Int_Or_Float},logo::interpreter_builtin_function_left})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"setpos",{Interpreter_Value_Type::Int_Or_Float,Interpreter_Value_Type::Int_Or_Float},logo::interpreter_builtin_function_setpos})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"setrot",{Interpreter_Value_Type::Int_Or_Float},logo::interpreter_builtin_function_setrot})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"getposx",{},logo::interpreter_builtin_function_getposx})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"getposy",{},logo::interpreter_builtin_function_getposy})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"getrot",{},logo::interpreter_builtin_function_getrot})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"penup",{},logo::interpreter_builtin_function_penup})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"pendown",{},logo::interpreter_builtin_function_pendown})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"pencolor",{Interpreter_Value_Type::Int,Interpreter_Value_Type::Int,Interpreter_Value_Type::Int},logo::interpreter_builtin_function_pencolor})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		if(!context.builtin_functions.push_back(Interpreter_Builtin_Function{"save",{Interpreter_Value_Type::String},logo::interpreter_builtin_function_save})) {
			Report_Error("Couldn't allocate % bytes of memory.",sizeof(Interpreter_Builtin_Function));
			return false;
		}
		return logo::interpret_ast(&context,statements,false,false).status == Interpreter_Status::Success;
	}
}
