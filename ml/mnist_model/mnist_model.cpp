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


// TODOs:
// - Enable normalization layer
// - Calculate loss accuracy for train and validation sets
// - How to move to compile-time as many things as possible? Change vectors to array, represent layers in model as a tuple?
// - Change design to take `batch` as input for forward_pass() and backward_pass() layer fucnctions and apply changes right inside backward()

constexpr std::size_t image_height = 28;
constexpr std::size_t image_width  = 28;
constexpr std::size_t pixels_count = image_height * image_width;

namespace ml {
    using input = std::vector<double>;

class layer {
public:
    using output = input;

    virtual output forward_pass(const input& input_data) const = 0;

    virtual output backward_pass(const output& activations, output&& grad_output) = 0;

    virtual void apply_changes() {};
};

class normalize_layer : public layer {
    virtual output forward_pass(const input& input_data) const override {
        output result = input_data;
        auto max_el   = std::max_element(std::begin(input_data), std::end(input_data));
        for (auto& el: result) {
            el /= *max_el;
        }

        return result;
    }

    virtual output backward_pass(const output& activations, output&& grad_output) override {
        output grad = std::move(grad_output);
        return grad;
    }
};

class relu_layer : public layer {
public:
    virtual output forward_pass(const input& input_data) const override {
        output result = input_data;
        for (auto& el: result) {
            el = std::max(0., el);
        }

        return result;
    }

    virtual output backward_pass(const output& activations, output&& grad_output) override {
        output grad = std::move(grad_output);
        for (std::size_t idx = 0; idx < activations.size(); ++idx) {
            if (activations[idx] <= 0) {
                grad[idx] = 0;
            }
        }
        return grad;
    }
};

class dense_layer : public layer {
    using node = std::vector<double>;
    std::vector<node> nodes, weights_change;
    std::size_t current_batch_size;
    double learning_rate;

    std::size_t weights_num() const{
        return nodes.front().size() - 1;
    }

public:
    dense_layer(std::size_t input_size, std::size_t output_size, double in_learning_rate = 0.1)
        : nodes(output_size, node(input_size + 1)) // + 1 for bias
        , weights_change(nodes)
        , current_batch_size(0)
        , learning_rate(in_learning_rate)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0, 0.001);

        for(auto& node: nodes) {
            for (auto& weight: node) weight = dist(gen);
            node.back() = 0; // nullify bias
        }
    }

    virtual output forward_pass(const input& input_data) const override {
        output result(nodes.size());

        assertm(input_data.size() == weights_num(), "Wrong input size");
        for (std::size_t node_idx = 0; node_idx < nodes.size(); ++node_idx) {
            const auto& node = nodes[node_idx];
            for (std::size_t weight_idx = 0; weight_idx < weights_num(); ++weight_idx) {
                result[node_idx] += node[weight_idx] * input_data[weight_idx];
            }
            result[node_idx] += node.back(); // add bias
        }

        return result;
    }

    virtual output backward_pass(const output& activations, output&& grad_output) override {
        // Calculate weights change
        ++current_batch_size;
        for (std::size_t node_idx = 0; node_idx < nodes.size(); ++node_idx) {
            for (std::size_t weight_idx = 0; weight_idx < weights_num(); ++weight_idx) {
                weights_change[node_idx][weight_idx] += grad_output[node_idx] * activations[weight_idx];
            }
            weights_change[node_idx].back() += grad_output[node_idx];
        }

        // Calculate grad for prev layer
        output grad(weights_num());
        for (std::size_t weight_idx = 0; weight_idx < weights_num(); ++weight_idx) {
            for (std::size_t node_idx = 0; node_idx < nodes.size(); ++node_idx) {
                grad[weight_idx] += grad_output[node_idx] * nodes[node_idx][weight_idx];
            }
        }

        return grad;
    }

    virtual void apply_changes() {
        for (std::size_t node_idx = 0; node_idx < nodes.size(); ++node_idx) {
            for (std::size_t weight_idx = 0; weight_idx < nodes[node_idx].size(); ++weight_idx) {
                nodes[node_idx][weight_idx] -= learning_rate * weights_change[node_idx][weight_idx] / current_batch_size;
                weights_change[node_idx][weight_idx] = 0;
            }
        }
        current_batch_size = 0;
    }
};

struct softmax_crossentropy_with_logits {
    static double loss(const layer::output& logits, std::size_t expected) {
        return -logits[expected] + std::log(std::accumulate(std::begin(logits), std::end(logits), 0,
            [](auto acc, auto elem) {
                return acc + std::exp(elem);
            }));
    }

    static layer::output loss_grad(const layer::output& logits, std::size_t expected) {
        layer::output result(logits.size());
        auto exp_logits_sum = std::accumulate(std::begin(logits), std::end(logits), 0.,
            [](auto acc, auto elem) {
                return acc + std::exp(elem);
            });

        for (std::size_t idx = 0; idx < logits.size(); ++idx) {
            double expected_probability = (idx == expected);
            double softmax = std::exp(logits[idx]) / exp_logits_sum;

            result[idx] = -expected_probability + softmax;
        }

        return result;
    }
};

template<typename LossFunc>
class model {
    std::vector<std::unique_ptr<layer>> layers;
public:
    using activations = std::vector<layer::output>;
    using train_data  = std::vector<std::pair<int, input>>; // batch

    void add_next_layer(std::unique_ptr<layer>&& layer_ptr) {
        layers.emplace_back(std::move(layer_ptr));
    }

    activations forward_pass(const input& input_data) const {
        activations result;
        auto *input_ptr = &input_data;

        for (const auto& layer: layers) {
            result.emplace_back(layer->forward_pass(*input_ptr));
            input_ptr = &result.back();
        }

        return result;
    }

    layer::output predict(const input& input_data) const {
        // REPORT ONLY PREDICTED RESULT
        // activations model_result    = forward_pass(input_data);
        // const layer::output& logits = model_result.back();
        // auto max_logit_it           = std::max_element(std::begin(logits), std::end(logits));
        // return std::distance(std::begin(logits), max_logit_it);

        return forward_pass(input_data).back();
    }

    void train(train_data& input_train_data) {
        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(std::begin(input_train_data), std::end(input_train_data), g);
 
        for (auto& [label, input_data]: input_train_data) {
            activations layer_inputs;
            layer_inputs.emplace_back(input_data);

            activations model_result = forward_pass(input_data);
            layer_inputs.insert(
                layer_inputs.end(),
                std::make_move_iterator(std::begin(model_result)),
                std::make_move_iterator(std::end(model_result))
            );

            const layer::output& logits = layer_inputs.back();

            double loss = LossFunc::loss(logits, label);
            layer::output grad = LossFunc::loss_grad(logits, label);

            for (int layer_idx = layers.size() - 1; layer_idx >= 0; --layer_idx) {
                grad = layers[layer_idx]->backward_pass(layer_inputs[layer_idx], std::move(grad));
            }
        }

        for (const auto& layer: layers) {
            layer->apply_changes();
        }
    }
};

}

constexpr std::size_t batch_size = 32;
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

    ml::model<ml::softmax_crossentropy_with_logits> mnist_model;
    // mnist_model.add_next_layer(std::make_unique<ml::normalize_layer>()); // enable later
    mnist_model.add_next_layer(std::make_unique<ml::dense_layer>(pixels_count, 100));
    mnist_model.add_next_layer(std::make_unique<ml::relu_layer>());
    mnist_model.add_next_layer(std::make_unique<ml::dense_layer>(100, 200));
    mnist_model.add_next_layer(std::make_unique<ml::relu_layer>());
    mnist_model.add_next_layer(std::make_unique<ml::dense_layer>(200, 10));

    std::cout << "First predict: " << std::endl;
    std::cout << "label is " << label << std::endl;
    ml::input normalized;
    for (std::size_t pixel_idx = 1; pixel_idx < first_image.size(); ++pixel_idx) {
        normalized.emplace_back(first_image[pixel_idx] / 255.);
    }
    auto result = mnist_model.predict(normalized);
    // auto result = mnist_model.predict(ml::input(std::next(std::begin(first_image)), std::end(first_image)));
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
            std::cout << "Batch #" << batch << " training " << std::endl;
            std::vector<std::pair<int, ml::input>> train_data_instance;
            // Why the following string causes error
            // ml::model::train_data train_data_instance;
            for (std::size_t sample_idx = 0; sample_idx < batch_size; ++sample_idx) {
                normalized.clear();
                std::size_t sample_base = batch * batch_size;
                std::vector<int> img = doc.GetRow<int>(sample_base + sample_idx);
                for (std::size_t pixel_idx = 1; pixel_idx < img.size(); ++pixel_idx) {
                    normalized.emplace_back(img[pixel_idx] / 255.);
                }
                train_data_instance.emplace_back(img.front(), normalized);

                // Can be enabled if normalization layer exists
                // train_data_instance.emplace_back(img.front(), ml::input(std::next(std::begin(img)), std::end(img)));
            }
            mnist_model.train(train_data_instance);

        }
        std::cout << "Epoch #" << epoch << " prediction " << std::endl;
        result = mnist_model.predict(ml::input(std::next(std::begin(first_image)), std::end(first_image)));
        for (std::size_t i = 0; i < result.size(); ++i) {
            std::cout << " i = " << i << " probability = " << result[i] << std::endl;
        }
    }
    std::cout << "End training" << std::endl;
}