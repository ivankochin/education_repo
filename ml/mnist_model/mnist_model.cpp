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

    namespace sigmoid {
        void activate(result& input) {
            for (auto& el: input) {
                el = expf(el) / (1 + expf(el));
            }
        }

        // Passing node activation function to derivative insted of node_output since it works with both relu and softmax
        double derivative(double node_activation_result) {
            return node_activation_result * (1 - node_activation_result);
        }
    } // namespace relu

    /*
    // USIGN SOFTMAX AS A DERIVATIVE IS MORE COMPLICATED BECAUSE IT DEPENDS ON ALL INPUT VECTOR FOR EACH ELEMENT
    namespace softmax {
        void activate(result& input) {
            double sum = 0;
            for (auto& el: input) {
                sum += expf(el);
                // std::cout << "sum = " << sum << " el = " << el << " std::expf(el) = " << expf(el) << std::endl;
            }

            for (auto& el: input) {
                el = std::exp(el) / sum;
            }
        }

        // Passing node activation function to derivative insted of node_output since it works with both relu and softmax
        double derivative(double node_activation_result) {
            return 1;
        }
    } // namespace softmax
    */



    } // namespace details

    activation_func relu = {details::relu::activate, details::relu::derivative};
    // activation_func softmax = {details::softmax::activate, details::softmax::derivative};
    activation_func sigmoid = {details::sigmoid::activate, details::sigmoid::derivative};
} // namespace act

namespace init {
    void random(node& input_node) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0, 0.01);

        for(auto& weight: input_node) {
            weight = dist(gen);
        }
    }

    // Symmetry training issue, can be used only for debugging
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

        result res(nodes_count());
        for (int i = 0; i < nodes_count(); ++i) {
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

    void backprop(train_data& input_train_data, double learning_step = 0.01) {
        std::vector<std::vector<node>> change_matrix{}, gradient;
        for (const auto& layer: layers) {
            change_matrix.emplace_back(layer.nodes_count(), node(layer.weights_count() + 1));
        }
        gradient = change_matrix;

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

            // Calculate gradient
            for (int layer_idx = layers.size() - 1; layer_idx >= 0; --layer_idx) {
                auto& layer = layers[layer_idx];
                for (std::size_t node_idx = 0; node_idx < layer.nodes_count(); ++node_idx) {
                    auto& gradient_vector = gradient[layer_idx][node_idx];
                    double& gradient_bias = gradient_vector.back();

                    if (layer_idx == layers.size() - 1) {
                        // Handle last output layer
                        // dC/dW[L,k,j] = (node_activation_results[L,j] - expected) * act_func.derivative(node_activation_results[L,j]) * node_activation_results[L - 1, j])
                        double result   = node_activation_results[layer_idx][node_idx];
                        double expected = (label == node_idx) ? 1.0 : 0.0;
                        gradient_bias   = (result - expected) * layer.act_func.derivative(result);
                        for (std::size_t weight_idx = 0; weight_idx < layer.weights_count(); ++weight_idx) {
                            gradient_vector[weight_idx] = gradient_bias * node_activation_results[layer_idx - 1][weight_idx];
                        }
                    } else {
                        // All other layers have save formula for bias
                        // dC/db[D,c,d] = act_func.derivative(node_results[D, d]) * sum(gradient_bias[D + 1] * weight[D + 1, d])
                        for (std::size_t next_layer_node_idx = 0; next_layer_node_idx < layers[layer_idx + 1].nodes_count(); ++next_layer_node_idx) {
                            gradient_bias += gradient[layer_idx + 1][next_layer_node_idx].back() * layers[layer_idx + 1].data[next_layer_node_idx][node_idx];
                        }
                        gradient_bias *= layer.act_func.derivative(node_activation_results[layer_idx][node_idx]);

                        if (layer_idx == 0) {
                            for (std::size_t weight_idx = 0; weight_idx < layer.weights_count(); ++weight_idx) {
                                gradient_vector[weight_idx] = gradient_bias * input[weight_idx];
                            }
                        } else {
                            for (std::size_t weight_idx = 0; weight_idx < layer.weights_count(); ++weight_idx) {
                                gradient_vector[weight_idx] = gradient_bias * node_activation_results[layer_idx - 1][weight_idx];
                            }
                        }
                    }
                }
            }

            for (std::size_t layer_idx = 0; layer_idx < change_matrix.size(); ++layer_idx) {
                for (std::size_t node_idx = 0; node_idx < change_matrix[layer_idx].size(); ++node_idx) {
                    for (std::size_t weight_idx = 0; weight_idx < change_matrix[layer_idx][node_idx].size(); ++weight_idx) {
                        change_matrix[layer_idx][node_idx][weight_idx] += gradient[layer_idx][node_idx][weight_idx];
                    }
                }
            }
        }

        // Apply changes for whole batch
        for (size_t layer_idx = 0; layer_idx < change_matrix.size(); ++layer_idx) {
            auto& change_layer = change_matrix[layer_idx];
            for (size_t node_idx = 0; node_idx < change_layer.size(); ++node_idx) {
                auto& change_node = change_layer[node_idx];
                for (size_t weight_idx = 0; weight_idx < change_node.size(); ++weight_idx) {
                    layers[layer_idx].data[node_idx][weight_idx] -= change_node[weight_idx] * learning_step;
                }
            }
        }
    }
};
}

constexpr std::size_t batch_size = 128;
constexpr std::size_t epochs_num = 10;

int main() {
    rapidcsv::Document doc("data/mnist_train.csv");

    // Display and predict first image on untrained network
    std::vector<int> first_image = doc.GetRow<int>(0);
    auto image_it = std::begin(first_image);
    int label = *image_it++;

    png::image image = png::make_image(image_height, image_width);
    for (int y = 0; y < image_height; ++y) {
        for (int x = 0; x < image_width; ++x) {
            image.set_pixel(x, y, *image_it++);
        }
    }
    image.write_to_file("image.png");

    ml::model mnist_model(pixels_count, 16, ml::act::relu, ml::init::random);
    mnist_model.add_next_layer(16, ml::act::relu);
    mnist_model.add_next_layer(10, ml::act::sigmoid);

    std::cout << "First predict: " << std::endl;
    std::cout << "label is " << label << std::endl;
    auto result = mnist_model.predict(image.get_data());
    for (std::size_t i = 0; i < result.size(); ++i) {
        std::cout << " i = " << i << " probability = " << result[i] << std::endl;
    }

    // Train model
    std::size_t img_count  = doc.GetRowCount();
    std::size_t batchs_num = img_count / batch_size;
    std::cout << "Start training, epochs number: " << epochs_num << std::endl;
    for (std::size_t epoch = 0; epoch < epochs_num; ++epoch) {
        std::cout << "Epoch #" << epoch << " training " << std::endl;
        for (std::size_t batch = 0; batch < batchs_num; ++batch) {
            ml::model::train_data train_data_instance;
            for (std::size_t sample_idx = 0; sample_idx < batch_size; ++sample_idx) {
                std::size_t sample_base = batch * batch_size;
                std::vector<int> img = doc.GetRow<int>(sample_base + sample_idx);
                train_data_instance.emplace_back(img.front(), std::vector<int>(std::next(std::begin(img)), std::end(img)));
            }
            mnist_model.backprop(train_data_instance);

        }
        std::cout << "Epoch #" << epoch << " prediction " << std::endl;
        result = mnist_model.predict(image.get_data());
        for (std::size_t i = 0; i < result.size(); ++i) {
            std::cout << " i = " << i << " probability = " << result[i] << std::endl;
        }
    }
    std::cout << "End training" << std::endl;
}