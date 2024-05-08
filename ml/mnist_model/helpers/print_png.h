#include <vector>
#include <memory>
#include <cstdio>

#include <png.h>

#define PNG_SETJMP_NOT_SUPPORTED 1

namespace png {

    namespace {
    void close_file(std::FILE* fp) {
        std::fclose(fp);
    }
    }

class image {
    std::vector<png_byte> data;
    std::size_t height;
    std::size_t width;
public:
    image(std::size_t input_height, std::size_t input_width)
        : data(input_height * input_width)
        , height{input_height}
        , width{input_width}
    {}

    void set_pixel(std::size_t x, std::size_t y, png_byte value) {
        data[x + y * width] = value;
    }

    const std::vector<png_byte>& get_data() {
        return data;
    }

    void write_to_file(const char* filename)
    {
        std::vector<png_byte*> write_image_view(height);
        std::unique_ptr<std::FILE, decltype(&close_file)> fp(std::fopen(filename, "wb"), &close_file);

        for (int i = 0; i < height; ++i) {
            write_image_view[i] = data.data() + i * width;
        }

        // Create struct for write
        png_struct* png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png) {
            printf("Create write struct failed\n");
            return;
        }
        // Create struct for info
        png_infop info = png_create_info_struct(png);
        if (!info) {
            return;
        }

        // Set file output
        png_init_io(png, fp.get());
        // Set image properties
        png_set_IHDR(png, info, height, width, 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        // Write png image information to file
        png_write_info(png, info);
        // Write image data
        png_write_image(png, write_image_view.data());
        png_write_end(png, NULL);
    }
};

image make_image(std::size_t height, std::size_t width) {
    return image(height, width);
}
}
