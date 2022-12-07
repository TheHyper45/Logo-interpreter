#include "utils.hpp"
#include "debug.hpp"
#include "string.hpp"
#include "heap_array.hpp"

int main() {
	if(!logo::debug_init()) {
		logo::eprint("[Error] %\n",logo::get_reported_error());
		return 1;
	}
	logo::print("Guys(%)\n",static_cast<std::size_t>(1337));
	logo::eprint("[Error] File doesn't exist.\n");

	logo::Report_Error("Couldn't allocate % bytes.",static_cast<std::size_t>(64));
	logo::eprint("\n%\n",logo::get_reported_error());

	logo::Array_String<1024> string0{};
	auto count0 = logo::format(&string0,"╔%╗","════");
	if(count0 == logo::String_Error) return 1;
	logo::print("Equality: %\n",string0);

	logo::print("----------------------------------------\n");
	for(std::size_t i : logo::Range(0,10)) logo::print("%\n",i);
	logo::print("----------------------------------------\n");

	logo::Heap_Array<std::size_t> array0{};
	defer[&]{array0.destroy();};
	if(!array0.push_back(10)) return 1;
	if(!array0.push_back(11)) return 1;
	if(!array0.push_back(9)) return 1;
	if(!array0.push_back(22)) return 1;
	if(!array0.push_back(18)) return 1;
	for(const auto& x : array0) logo::print("%\n",x);
	return 0;
}
