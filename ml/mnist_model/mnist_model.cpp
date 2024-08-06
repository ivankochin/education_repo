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
#include <cmath>
#include <list>


constexpr std::size_t image_height = 28;
constexpr std::size_t image_width  = 28;
constexpr std::size_t pixels_count = image_height * image_width;


namespace ml {
using node = std::vector<double>;
using result = std::vector<double>;

namespace act {
    struct activation_func {
        using activate_func = void (*)(result&);
        using derivative_func = double (*)(double);

        activate_func activate = nullptr;
        derivative_func derivative = nullptr;
    };

    namespace details {
    namespace relu {
        void activate(result& input) {
            for (auto& el: input) {
                el = std::max(el, 0.0);
            }
        }

        // Passing node activation function to derivative insted of node_output since it works with both relu and softmax
        double derivative(double node_activation_result)  {
            return (node_activation_result > 0) ? 1 : 0;
        }
    } // namespace relu

    namespace softmax {
        void activate(result& input) {
            double sum = std::accumulate(std::begin(input), std::end(input), 0,
                    [](double old_value, double new_value) {
                        return old_value + std::exp(new_value);
                    });
            for (auto& el: input) {
                el = std::exp(el) / sum;
            }
        }

        // Passing node activation function to derivative insted of node_output since it works with both relu and softmax
        double derivative(double node_activation_result) {
            return node_activation_result * (1 - node_activation_result);
        }
    } // namespace softmax
    } // namespace details

    activation_func relu = {details::relu::activate, details::relu::derivative};
    activation_func softmax = {details::softmax::activate, details::softmax::derivative};
} // namespace act

namespace init {
    void random(node& input_node) {
        static std::uniform_int_distribution<uint32_t> unif(0, 32768);
        static std::default_random_engine re;
        uint32_t sum;

        for(auto& weight: input_node) {
            weight = unif(re);
            sum += weight;
        }

        // Substract bias and initialize it as 0
        sum -= input_node.back();
        input_node.back() = 0;

        for(auto& weight: input_node) {
            weight /= sum;
        }
    }

    void zero(node& input_node) {
        for(auto& weight: input_node) {
            weight = 0;
        }
    }
} // namspace init

class layer {
public:
    using initialization_function = void(*)(node&);

    const act::activation_func act_func;
// private:
    std::vector<node> data;
    std::size_t weights_num;
    initialization_function init_func;
public:
    layer(std::size_t nodes_count, std::size_t input_weights_count, act::activation_func input_act_func,
          initialization_function init_func)
        : data(nodes_count, node(input_weights_count + 1))
        , weights_num(input_weights_count)
        , act_func(input_act_func)
    {
        for (auto& n: data) {
            init_func(n);
        }
    }

    std::size_t nodes_count() const {
        return data.size();
    }

    std::size_t weights_count() const {
        return weights_num;
    }

    template <typename Input>
    result process(const Input& input) const {
        assertm(input.size() == weights_num, "Wrong input size");

        result res(data.size());
        for (int i = 0; i < data.size(); ++i) {
            int j = 0;
            for(; j < weights_num; ++j) {
                res[i] += data[i][j] * input[j];
            }
            res[i] += data[i][j]; // add bias
        }

        return res;
    }
};

class model {
    std::vector<layer> layers{};
    layer::initialization_function init_func;

    friend class layer;
public:
    using train_data = std::vector<std::pair<int, std::vector<int>>>;

    model(std::size_t input_size, std::size_t nodes_count, act::activation_func act_func,
          layer::initialization_function input_init_func)
        : layers{}
        , init_func(input_init_func)
    {
        layers.emplace_back(nodes_count, input_size, act_func, init_func);
    }

    void add_next_layer(std::size_t nodes_count, act::activation_func act_func) {
        std::size_t weights_count = layers.back().nodes_count();

        layers.emplace_back(nodes_count, weights_count, act_func, init_func);
    }

    template <typename Input>
    result predict(const Input& input) {
        auto layer_it = std::begin(layers);
        result processing_data = layer_it->process(input);
        layer_it->act_func.activate(processing_data);
        ++layer_it;

        for (; layer_it != std::end(layers); ++layer_it) {
            processing_data = layer_it->process(processing_data);
            layer_it->act_func.activate(processing_data);
        }

        return processing_data;
    }

    void backprop(train_data& input_train_data, double learning_step = 0.001) {
        std::vector<std::vector<node>> change_matrix{};
        for (const auto& layer: layers) {
            change_matrix.emplace_back(layer.nodes_count(), node(layer.weights_count() + 1));
        }

        for (auto& [label, input]: input_train_data) {
            std::vector<result> node_results, node_activation_results;

            // TODO: Think how to unite forward path with predict()
            auto layer_it = std::begin(layers);
            node_results.emplace_back(layer_it->process(input));
            node_activation_results.emplace_back(node_results.back());
            layer_it->act_func.activate(node_activation_results.back());
            ++layer_it;

            for (; layer_it != std::end(layers); ++layer_it) {
                node_results.emplace_back(layer_it->process(node_activation_results.back()));
                node_activation_results.emplace_back(node_results.back());
                layer_it->act_func.activate(node_activation_results.back());
            }

            // Calculate change matrix
            for (int layer_idx = layers.size() - 1; layer_idx >= 0; --layer_idx) {
                auto& layer = layers[layer_idx];
                for (std::size_t node_idx = 0; node_idx < layer.nodes_count(); ++node_idx) {
                    auto& change_vector = change_matrix[layer_idx][node_idx];
                    double expected = (label == node_idx) ? 1.0 : 0.0;
                    double change_bias;

                    if (layer_idx == layers.size() - 1) {
                        // Handle last output layer
                        // dC/dW[L,k,j] = 2 * (node_activation_results[L,j] - expected) * act_func.derivative(node_activation_results[L,j]) * node_activation_results[L - 1, j])
                        change_bias = -1 * 2 * (node_activation_results[layer_idx][node_idx] - expected) * layer.act_func.derivative(node_activation_results[layer_idx][node_idx]);
                        for (std::size_t weight_idx = 0; weight_idx < layer.weights_count(); ++weight_idx) {
                            change_vector[weight_idx] += change_bias * node_activation_results[layer_idx - 1][weight_idx];
                        }
                    } else if (layer_idx != 0) {
                        // Handle middle layer
                        // dC/dW[D,c,d] = act_func.derivative(node_results[D, d]) * node_activation_result[D - 1, c] * sum(change_bias[D + 1] * weight[D + 1, d])
                        for (std::size_t next_layer_node_idx = 0; next_layer_node_idx < layers[layer_idx + 1].nodes_count(); ++next_layer_node_idx) {
                            change_bias += change_matrix[layer_idx + 1][next_layer_node_idx].back() * layers[layer_idx + 1].data[next_layer_node_idx][node_idx];
                        }
                        change_bias *= -1 * layer.act_func.derivative(node_activation_results[layer_idx][node_idx]);

                        for (std::size_t weight_idx = 0; weight_idx < layer.weights_count(); ++weight_idx) {
                            change_vector[weight_idx] += change_bias * node_activation_results[layer_idx - 1][weight_idx];
                        }
                    } else {
                        // Handle first layer
                        for (std::size_t next_layer_node_idx = 0; next_layer_node_idx < layers[layer_idx + 1].nodes_count(); ++next_layer_node_idx) {
                            change_bias += change_matrix[layer_idx + 1][next_layer_node_idx].back() + layers[layer_idx + 1].data[next_layer_node_idx][node_idx];
                        }
                        change_bias *= -1 * layer.act_func.derivative(node_activation_results[layer_idx][node_idx]);

                        for (std::size_t weight_idx = 0; weight_idx < layer.weights_count(); ++weight_idx) {
                            change_vector[weight_idx] += change_bias * input[weight_idx];
                        }
                    }

                    // Sum up bias
                    change_matrix[layer_idx][node_idx].back() += change_bias;
                }
            }

            for (size_t layer_idx = 0; layer_idx < change_matrix.size(); ++layer_idx) {
                auto& change_layer = change_matrix[layer_idx];
                for (size_t node_idx = 0; node_idx < change_layer.size(); ++node_idx) {
                    auto& change_node = change_layer[node_idx];
                    for (size_t weight_idx = 0; weight_idx < change_node.size(); ++weight_idx) {
                        layers[layer_idx].data[node_idx][weight_idx] += change_node[weight_idx] * learning_step;
                    }
                }
            }
        }
    }
};
}

constexpr std::size_t batch_size = 100;

int main() {
    rapidcsv::Document doc("data/mnist_train.csv");

    // Display and predict first image on untrained network
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
    mnist_model.add_next_layer(10, ml::act::softmax);

    std::cout << "First predict: " << std::endl;
    auto result = mnist_model.predict(image.get_data());
    for (std::size_t i = 0; i < result.size(); ++i) {
        std::cout << " i = " << i << " probability = " << result[i] << std::endl;
    }

    // Train model
    std::size_t epochs_num = doc.GetRowCount() / batch_size;
    std::cout << "Epochs number: " << epochs_num << std::endl;
    for (std::size_t epoch = 0; epoch < epochs_num; ++epoch) {
        std::cout << "Epoch #" << epoch << " training " << std::endl;
        ml::model::train_data train_data_instance;
        for (std::size_t sample_idx = epoch * batch_size; sample_idx < batch_size; ++sample_idx) {
            std::vector<int> img = doc.GetRow<int>(sample_idx);
            train_data_instance.emplace_back(img.front(), std::vector<int>(std::next(std::begin(img)), std::end(img)));
        }
        mnist_model.backprop(train_data_instance);
    }

    std::cout << "Second predict: " << std::endl;
    result = mnist_model.predict(image.get_data());
    for (std::size_t i = 0; i < result.size(); ++i) {
        std::cout << " i = " << i << " probability = " << result[i] << std::endl;
    }
}