#ifndef LOGO_UTILS_HPP
#define LOGO_UTILS_HPP

#include <cstddef>
#include <utility>
#include <type_traits>

namespace logo {
	template<typename...>
	struct Tuple;

	template<typename T0,typename T1>
	struct Tuple<T0,T1> {
		T0 first;
		T1 second;
	};

	template<typename T>
	struct Range {
		struct Iterator {
			T value;
			bool operator==(Iterator it) const { return value == it.value; }
			Iterator& operator++() { value += 1; return *this; }
			const T& operator*() const { return value; }
		};
		T min;
		T max;
		Range(T _max) : min(),max(_max) {}
		Range(T _min,T _max) : min(_min),max(_max) {}
		Iterator begin() const { return {min}; }
		Iterator end() const { return {max}; }
	};
	template<typename T>
	Range(T) -> Range<T>;
	template<typename T,typename U>
	Range(T,U) -> Range<std::common_type_t<T,U>>;

	template<typename T>
	struct Option {
		T value;
		bool has_value;
		Option() : value(),has_value() {}
		Option(const Option<T>& option) : value(option.value),has_value(option.has_value) {}
		Option(const T& _value) : value(_value),has_value(true) {}
		Option& operator=(const T& _value) {
			value = _value;
			has_value = true;
			return *this;
		}
		Option& operator=(const Option<T>& option) {
			value = option.value;
			has_value = option.has_value;
			return *this;
		}
	};

	template<typename Lambda>
	struct Deferred_Lambda {
		Lambda lambda;
		Deferred_Lambda(Lambda&& _lambda) : lambda(std::move(_lambda)) {}
		~Deferred_Lambda() { lambda(); }
	};
#define LOGO_CONCAT_(X,Y) X##Y
#define LOGO_CONCAT(X,Y) LOGO_CONCAT_(X,Y)
#define defer logo::Deferred_Lambda LOGO_CONCAT(_lambda,__LINE__) =

	template<typename Type,typename... Types>
	[[nodiscard]] constexpr bool is_one_of(const Type& type,const Types&... types) {
		return ((type == types) || ...);
	}

	[[nodiscard]] constexpr std::size_t megabytes(std::size_t count) {
		return count * 1024 * 1024;
	}

#define LOGO_DEFINE_ENUM_FLAGS(T)\
	T operator~(T v) {\
		using Underlying_Type = std::underlying_type_t<T>;\
		return static_cast<T>(~static_cast<Underlying_Type>(v));\
	}\
	T operator|(T a,T b) {\
		using Underlying_Type = std::underlying_type_t<T>;\
		return static_cast<T>(static_cast<Underlying_Type>(a) | static_cast<Underlying_Type>(b));\
	}\
	T operator&(T a,T b) {\
		using Underlying_Type = std::underlying_type_t<T>;\
		return static_cast<T>(static_cast<Underlying_Type>(a) & static_cast<Underlying_Type>(b));\
	}\
	T& operator|=(T& a,T b) {\
		a = (a | b);\
		return a;\
	}\
	T& operator&=(T& a,T b) {\
		a = (a & b);\
		return a;\
	}
}

#endif
