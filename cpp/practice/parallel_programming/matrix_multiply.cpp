#include <vector>
#include <iostream>
#include <exception>
#include <thread>

using matrix = std::vector<std::vector<int>>;

void check_multiply_applicability(const matrix& mat1, const matrix& mat2) {
    if (mat1.size() != mat2.front().size() || mat2.size() != mat1.front().size()) {
        std::cout << "Matrixes are incompatible for the multiplication." << std::endl;
        std::terminate();
    }
}

matrix sequential_matrixes_multiplication(const matrix& mat1, const matrix& mat2) {
    check_multiply_applicability(mat1, mat2);

    matrix result_matrix{mat1.size()};
    for (auto& line: result_matrix)
        line.resize(mat2.front().size());

    for (std::size_t i = 0; i < result_matrix.size(); ++i) {
        for (std::size_t j = 0; j < result_matrix.front().size(); ++j) {
            for (std::size_t k = 0; k < mat1.front().size(); ++k) {
                result_matrix[i][j] += mat1[i][k] * mat2[k][j];
            }
        }
    }

    return result_matrix;
}

matrix parallel_matrixes_multiplication(const matrix& mat1, const matrix& mat2) {
    check_multiply_applicability(mat1, mat2);

    matrix result_matrix{mat1.size()};
    for (auto& line: result_matrix)
        line.resize(mat2.front().size());

    auto elements_num = result_matrix.size() * result_matrix.front().size();
    auto threads_num = std::thread::hardware_concurrency();
    std::vector<std::size_t> chunks(threads_num + 1);
    auto chunk_size = elements_num / threads_num;
    auto remainder = elements_num % threads_num;

    chunks[0] = 0;
    for (std::size_t chunk_num = 1; chunk_num < chunks.size(); ++chunk_num) {
        chunks[chunk_num] = chunks[chunk_num - 1] + chunk_size;
        if (remainder > 0) {
            --remainder;
            ++chunks[chunk_num];
        }
    }

    for (std::size_t chunk_id = 0; chunk_id < threads_num; ++chunk_id) {
        for (std::size_t el_num = chunks[chunk_id]; el_num < chunks[chunk_id + 1]; ++el_num) {
            auto row = el_num / result_matrix.size();
            auto col = el_num % result_matrix.size();
        }
    }

    auto thread_body = [&](std::size_t chunk_id) {
        for (std::size_t el_num = chunks[chunk_id]; el_num < chunks[chunk_id + 1]; ++el_num) {
            auto row = el_num / result_matrix.size();
            auto col = el_num % result_matrix.size();
            for (int i = 0; i < mat2.size(); ++i) {
                result_matrix[row][col] += mat1[row][i] * mat2[i][col];
            }
        }
    };

    std::vector<std::thread> workers{threads_num};
    for (std::size_t thread_id = 0; thread_id < threads_num; ++thread_id) {
        workers[thread_id] = std::thread{thread_body, thread_id};
    }

    for (auto& worker: workers) {
        worker.join();
    }

    return result_matrix;
}

void print_matrix(const matrix& mat) {
    for (std::size_t i = 0; i < mat.size(); ++i) {
        for (std::size_t j = 0; j < mat.front().size(); ++j) {
            std::cout << mat[i][j] << " ";
        }
        std::cout << std::endl;
    }
}

int main() {
    matrix mat1 =
    {
        {1, 2, 3, 4},
        {2, 5, 6, 1},
        {3, 1, 7, 9}
    };

    matrix mat2 =
    {
        {1, 5, 2},
        {3, 7, 1},
        {4, 6, 9},
        {1, 2, 3}
    };

    auto mat3 = sequential_matrixes_multiplication(mat1, mat2);
    std::cout << "Sequential:" << std::endl;
    print_matrix(mat3);
    auto mat4 = parallel_matrixes_multiplication(mat1, mat2);
    std::cout << "Parallel:" << std::endl;
    print_matrix(mat4);
}
