#ifndef UTILS_HPP
#define UTILS_HPP

namespace logo {
	template<typename...>
	struct Tuple;

	template<typename T0,typename T1>
	struct Tuple<T0,T1> {
		T0 first;
		T1 second;
	};

	struct Invalid_Utf8_Sequence {

	};
}

#endif
