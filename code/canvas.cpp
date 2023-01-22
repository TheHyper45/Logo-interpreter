#include <cmath>
#include <cstdio>
#include "debug.hpp"
#include "canvas.hpp"

namespace logo {
	bool Canvas::init(std::int32_t w,std::int32_t h) {
		width = w;
		height = h;
		pos_x = width / 2.0f;
		pos_y = height / 2.0f;
		rot = 0.0;
		is_pen_down = true;
		pen_color = Color{0,0,0};
		if(!pixels.resize(static_cast<std::size_t>(width) * height,Color{255,255,255})) {
			Report_Error("Couldn't allocate % bytes of memory.",width * height * sizeof(pixels[0]));
			return false;
		}
		return true;
	}

	void Canvas::destroy() {
		pixels.destroy();
	}

	bool Canvas::save_as_bitmap(String_View file_path) {
		logo::print("Saving canvas as \"%\".\n",file_path);

		std::FILE* file = std::fopen(file_path.begin_ptr,"wb");
		if(!file) {
			Report_Error("Couldn't open file \"%\".",file_path);
			return false;
		}
		defer[&]{std::fclose(file);};

		auto file_write = [&]<typename T>(const T& value) {
			if(std::fwrite(&value,sizeof(char),sizeof(T),file) < sizeof(T)) {
				Report_Error("Couldn't write % bytes to file \"%\".",sizeof(T),file_path);
				return false;
			}
			return true;
		};

		static constexpr std::uint8_t Magic_Bytes[] = {'B','M'};
		static constexpr std::uint32_t Bitmap_File_Header_Size = 14;
		static constexpr std::uint32_t Bitmap_Info_Header_Size = 40;

		//Writing bitmap file header.
		if(!file_write(Magic_Bytes)) return false;
		
		std::uint32_t file_size = Bitmap_File_Header_Size + Bitmap_Info_Header_Size + width * height * 4;
		if(!file_write(file_size)) return false;

		static constexpr std::uint16_t Reserved[2] = {};
		if(!file_write(Reserved)) return false;

		std::uint32_t pixel_bytes_offset = Bitmap_File_Header_Size + Bitmap_Info_Header_Size;
		if(!file_write(pixel_bytes_offset)) return false;

		//Writing bitmap info header.
		if(!file_write(Bitmap_Info_Header_Size)) return false;

		if(!file_write(width)) return false;
		if(!file_write(height)) return false;

		static constexpr std::uint16_t Plane_Count = 1;
		if(!file_write(Plane_Count)) return false;

		static constexpr std::uint16_t Bits_Per_Pixel = 32;
		if(!file_write(Bits_Per_Pixel)) return false;

		static constexpr std::uint32_t No_Compression = 0;
		if(!file_write(No_Compression)) return false;

		static constexpr std::uint32_t Image_Size = 0;
		if(!file_write(Image_Size)) return false;

		if(!file_write(width)) return false;
		if(!file_write(height)) return false;

		static constexpr std::uint32_t Color_Palette_Ignored = 0;
		if(!file_write(Color_Palette_Ignored)) return false;
		if(!file_write(Color_Palette_Ignored)) return false;

		for(std::size_t i = 0;i < pixels.length;i += 1) {
			if(!file_write(pixels[i].b)) return false;
			if(!file_write(pixels[i].g)) return false;
			if(!file_write(pixels[i].r)) return false;

			static constexpr std::uint8_t Opaque_Alpha = 255;
			if(!file_write(Opaque_Alpha)) return false;
		}
		return true;
	}

	//https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
	void Canvas::move_forward(double steps) {
		auto plot_line_low = [&](std::int32_t x0,std::int32_t y0,std::int32_t x1,std::int32_t y1) {
			auto dx = x1 - x0;
			auto dy = y1 - y0;
			std::int32_t yi = 1;
			if(dy < 0) {
				yi = -1;
				dy = -dy;
			}
			auto d = 2 * dy - dx;
			auto y = y0;

			for(std::int32_t x = x0;x <= x1;x += 1) {
				if(x >= 0 && x < width && y >= 0 && y < height) {
					if(is_pen_down) pixels[y * width + x] = pen_color;
				}
				if(d > 0) {
					y += yi;
					d += 2 * (dy - dx);
				}
				else d += 2 * dy;
			}
		};
		auto plot_line_high = [&](std::int32_t x0,std::int32_t y0,std::int32_t x1,std::int32_t y1) {
			auto dx = x1 - x0;
			auto dy = y1 - y0;
			std::int32_t xi = 1;
			if(dx < 0) {
				xi = -1;
				dx = -dx;
			}
			auto d = 2 * dx - dy;
			auto x = x0;

			for(std::int32_t y = y0;y <= y1;y += 1) {
				if(x >= 0 && x < width && y >= 0 && y < height) {
					if(is_pen_down) pixels[y * width + x] = pen_color;
				}
				if(d > 0) {
					x += xi;
					d += 2 * (dx - dy);
				}
				else d += 2 * dx;
			}
		};

		auto x0 = static_cast<std::int32_t>(pos_x);
		auto y0 = static_cast<std::int32_t>(pos_y);
		double fx1 = pos_x + std::cos(rot) * steps;
		double fy1 = pos_y + std::sin(rot) * steps;
		pos_x = fx1;
		pos_y = fy1;
		auto x1 = static_cast<std::int32_t>(fx1);
		auto y1 = static_cast<std::int32_t>(fy1);

		if(std::abs(y1 - y0) < std::abs(x1 - x0)) {
			if(x0 > x1) plot_line_low(x1,y1,x0,y0);
			else plot_line_low(x0,y0,x1,y1);
		}
		else {
			if(y0 > y1) plot_line_high(x1,y1,x0,y0);
			else plot_line_high(x0,y0,x1,y1);
		}
	}
}
