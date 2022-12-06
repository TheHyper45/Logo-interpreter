#ifndef LOGO_UTILS_HPP
#define LOGO_UTILS_HPP

#include <cstddef>
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
	consteval std::size_t max_int_char_count() {
		switch(sizeof(T)) {
			case 1: return 4;
			case 2: return 6;
			case 4: return 11;
			case 8: return 20;
		}
	}
}

#endif
