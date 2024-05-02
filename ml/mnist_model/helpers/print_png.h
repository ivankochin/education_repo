#include <vector>
#include <memory>
#include <cstdio>

#include <png.h>

#define IMAGE_HEIGHT 250
#define IMAGE_WIDTH 300
#define IMAGE_GRAY 50

#define PNG_SETJMP_NOT_SUPPORTED 1

using line_type = std::vector<png_byte>;
using image_type = std::vector<line_type>;

image_type make_image(std::size_t height, std::size_t width) {
    return image_type(height, line_type(width));
}

void close_file(std::FILE* fp)
{
    std::fclose(fp);
}

void write_png_image(image_type& image, const char* filename)
{
    // std::vector<line_type> image(IMAGE_HEIGHT, line_type(IMAGE_WIDTH));
    std::vector<png_byte*> write_image_view(image.size());
    std::unique_ptr<std::FILE, decltype(&close_file)> fp(std::fopen(filename, "wb"), &close_file);

    for (int i = 0; i < write_image_view.size(); ++i) {
        write_image_view[i] = image[i].data();
    }
    // for (line_type& line: image) {
    //     for (png_byte& point: line) {
    //         point = IMAGE_GRAY;
    //     }
    // }

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
    png_set_IHDR(png, info, image.size(), image.front().size(), 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    // Write png image information to file
    png_write_info(png, info);
    // Write image data
    png_write_image(png, write_image_view.data());
    png_write_end(png, NULL);
}
