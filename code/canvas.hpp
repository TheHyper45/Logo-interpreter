#ifndef LOGO_CANVAS_HPP
#define LOGO_CANVAS_HPP

#include <cstdint>
#include "string.hpp"
#include "heap_array.hpp"

namespace logo {
	struct Color {
		std::uint8_t r,g,b;
	};
	struct Canvas {
		std::int32_t width,height;
		double pos_x,pos_y,rot;
		Heap_Array<Color> pixels;

		bool init(std::int32_t w,std::int32_t h);
		void destroy();
		[[nodiscard]] Option<Canvas> clone() const;
		bool save_as_bitmap(String_View file_path);
		void move_forward(double steps);
	};
}

#endif
