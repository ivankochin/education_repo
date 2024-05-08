// g++ mnist_model.cpp -lpng -g

#include "helpers/print_png.h"
#include "helpers/rapidcsv.h"


// uncomment to disable assert()
// #define NDEBUG
#include <cassert>
 
// Use (void) to silence unused warnings.
#define assertm(exp, msg) assert(((void)msg, exp))


#include <algorithm>
#include <random>
#include <list>


constexpr std::size_t image_height = 28;
constexpr std::size_t image_width  = 28;
constexpr std::size_t pixels_count = image_height * image_width;


namespace ml {
using node = std::vector<double>;

class layer {
public:
    using result = std::vector<double>;
    using activation_function = void (*)(result&);
    using initialization_function = void(*)(node&);
private:
    std::vector<node> data;
    std::size_t weights_count;
    activation_function act_func;
    initialization_function init_func;
public:
    layer(std::size_t nodes_count, std::size_t input_weights_count, activation_function input_act_func,
          initialization_function input_init_func)
        : data(nodes_count, node(input_weights_count + 1))
        , weights_count(input_weights_count)
        , act_func(input_act_func)
        , init_func(input_init_func)
    {
        for (auto& n: data) {
            init_func(n);
        }
    }

    std::size_t nodes_count() const {
        return data.size();
    }

    template <typename Input>
    result process(const Input& input) const {
        assertm(input.size() == weights_count, "Wrong input size");

        result res(data.size());
        for (int i = 0; i < data.size(); ++i) {
            int j = 0;
            for(; j < weights_count; ++j) {
                res[i] += data[i][j] * input[j];
            }
            res[i] += data[i][j]; // add bias
        }

        act_func(res);

        return res;
    }
};


namespace act {
    void relu(layer::result& input) {
        for (auto& el: input) {
            el = std::max(0.0, el);
        }
    }

    void soft_max(layer::result& input) {
        auto sum = std::accumulate(std::begin(input), std::end(input), 0);
        for (auto& el: input) {
            el = el / sum;
        }
    }
}

namespace init {
    void random(node& input_node) {
        static std::uniform_real_distribution<double> unif(-1, 1);
        static std::default_random_engine re;

        for(auto& weight: input_node) {
            weight = unif(re);
        }
    }

    void zero(node& input_node) {
        for(auto& weight: input_node) {
            weight = 0;
        }
    }
}

class model {
    std::list<layer> layers{};
    layer::initialization_function init_func;
public:

    model(std::size_t input_size, std::size_t nodes_count, layer::activation_function act_func,
          layer::initialization_function input_init_func)
        : layers{}
        , init_func(input_init_func)
    {
        layers.emplace_back(nodes_count, input_size, act_func, init_func);
    }

    void add_next_layer(std::size_t nodes_count, layer::activation_function act_func) {
        std::size_t weights_count = layers.back().nodes_count();

        layers.emplace_back(nodes_count, weights_count, act_func, init_func);
    }

    template <typename Input>
    layer::result predict(const Input& input) {
        auto layer_it = std::begin(layers);
        layer::result processing_data = layer_it->process(input);
        ++layer_it;

        for (; layer_it != std::end(layers); ++layer_it) {
            processing_data = layer_it->process(processing_data);
        }

        return processing_data;
    }
};
}

int main() {
    rapidcsv::Document doc("data/mnist_train.csv");
    std::vector<int> first_image = doc.GetRow<int>(0);
    auto image_it = std::begin(first_image);
    int label = *image_it++;
    std::cout << "label is " << label << std::endl;

    png::image image = png::make_image(image_height, image_width);
    for (int y = 0; y < image_height; ++y) {
        for (int x = 0; x < image_width; ++x) {
            image.set_pixel(x, y, *image_it++);
        }
    }
    image.write_to_file("image.png");

    ml::model mnist_model(pixels_count, 16, ml::act::relu, ml::init::random);
    mnist_model.add_next_layer(16, ml::act::relu);
    mnist_model.add_next_layer(10, ml::act::soft_max);

    auto result = mnist_model.predict(image.get_data());
    for (std::size_t i = 0; i < result.size(); ++i) {
        std::cout << " i = " << i << " probability = " << result[i] << std::endl;
    }
}