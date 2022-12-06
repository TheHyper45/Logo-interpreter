#include "utils.hpp"
#include "debug.hpp"
#include "string.hpp"

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
	return 0;
}
