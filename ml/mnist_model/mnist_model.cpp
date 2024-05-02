#include "helpers/print_png.h"
#include "helpers/rapidcsv.h"

constexpr std::size_t image_height = 28;
constexpr std::size_t image_width  = 28;

int main() {
    rapidcsv::Document doc("data/mnist_train.csv");
    std::vector<int> first_image = doc.GetRow<int>(0);
    auto image_it = std::begin(first_image);
    int label = *image_it++;
    std::cout << "label is " << label << std::endl;

    image_type image = make_image(image_height, image_width);
    for (int i = 0; i < image_height; ++i) {
        for (int j = 0; j < image_width; ++j) {
            image[i][j] = *image_it++;
        }
    }
    write_png_image(image, "test_file.png");
}