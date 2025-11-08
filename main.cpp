// Compiler: MSVC for C++20
// Release mode, Optimization /Ox

#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <random>
#include <execution> // For parallel policies
#include <thread>    // For hardware_concurrency and custom parallel algorithm
#include <future>    // For std::async or futures in custom parallel algorithm
#include <iomanip>   // For std::setw, std::fixed, std::setprecision

bool is_even(int n) {
    return n % 2 == 0;
}

std::vector<int> generate_random_vector(size_t size) {
    std::vector<int> vec(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1, 10000); // Random numbers between 1 and 10000

    for (size_t i = 0; i < size; ++i) {
        vec[i] = distrib(gen);
    }
    return vec;
}

template<typename Func>
double measure_time(Func func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    return duration.count();
}

long long custom_parallel_count_if(const std::vector<int>& data, int num_threads) {
    if (data.empty()) {
        return 0;
    }

    long long total_count = 0;
    size_t data_size = data.size();
    size_t chunk_size = (data_size + num_threads - 1) / num_threads; 

    std::vector<std::future<long long>> futures;
    futures.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        size_t start_idx = i * chunk_size;
        size_t end_idx = std::min(start_idx + chunk_size, data_size);

        if (start_idx >= end_idx) { 
            continue;
        }

        futures.push_back(std::async(std::launch::async, [&data, start_idx, end_idx]() {
            long long local_count = 0;
            for (size_t j = start_idx; j < end_idx; ++j) {
                if (is_even(data[j])) {
                    local_count++;
                }
            }
            return local_count;
            }));
    }

    for (auto& f : futures) {
        total_count += f.get();
    }

    return total_count;
}


int main() {
    std::cout << "Starting count_if performance research..." << std::endl;
    std::cout << "Using C++20, MSVC Compiler in RELEASE mode (Optimization Maximum /Ox)" << std::endl;
    std::cout << "Predicate: is_even (checks if number is even)" << std::endl << std::endl;

    std::vector<size_t> data_sizes = { 100000, 1000000, 10000000, 100000000 }; // 10^5, 10^6, 10^7, 10^8

    unsigned int hardware_threads = std::thread::hardware_concurrency();
    std::cout << "Hardware concurrency (logical cores): " << hardware_threads << std::endl << std::endl;

    std::cout << std::fixed << std::setprecision(6);

    std::cout << "--- 1. Library std::count_if (sequential) ---" << std::endl;
    int nonlocal{};
    for (size_t size : data_sizes) {
        std::vector<int> data = generate_random_vector(size);
        double duration = measure_time([&]() {
            nonlocal = std::count_if(data.begin(), data.end(), is_even);
            });
        std::cout << "Data size: " << std::setw(9) << size << " | Time: " << duration << " seconds" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "--- 2. Library std::count_if with execution policies ---" << std::endl;
    size_t large_data_size = data_sizes.back(); // Use 10^8 elements
    std::vector<int> large_data = generate_random_vector(large_data_size);
    std::cout << "Using data size: " << large_data_size << std::endl;
    
    long long count_result{}; // Declare a variable to store the result
    
    double time_seq_policy = measure_time([&]() {
        count_result = std::count_if(std::execution::seq, large_data.begin(), large_data.end(), is_even);
        });
    std::cout << "  Policy std::execution::seq      | Time: " << time_seq_policy << " seconds" << std::endl;

    double time_par_policy = measure_time([&]() {
        count_result =   std::count_if(std::execution::par, large_data.begin(), large_data.end(), is_even);
        });
    std::cout << "  Policy std::execution::par      | Time: " << time_par_policy << " seconds" << std::endl;

    double time_unseq_policy = measure_time([&]() {
        count_result =  std::count_if(std::execution::unseq, large_data.begin(), large_data.end(), is_even);
        });
    std::cout << "  Policy std::execution::unseq    | Time: " << time_unseq_policy << " seconds" << std::endl;

    double time_par_unseq_policy = measure_time([&]() {
        count_result =   std::count_if(std::execution::par_unseq, large_data.begin(), large_data.end(), is_even);
        });
    std::cout << "  Policy std::execution::par_unseq| Time: " << time_par_unseq_policy << " seconds" << std::endl;
    std::cout << std::endl;

    std::cout << "--- 3. Custom Parallel count_if Algorithm ---" << std::endl;
    std::cout << "Using data size: " << large_data_size << std::endl;
    std::cout << "Varying K (number of threads/chunks):" << std::endl;

    std::vector<int> k_values;
    k_values.push_back(1);
    if (hardware_threads > 1) {
        k_values.push_back(hardware_threads / 2);
    }
    k_values.push_back(hardware_threads);
    k_values.push_back(hardware_threads + (hardware_threads > 1 ? hardware_threads / 2 : 1));
    k_values.push_back(hardware_threads * 2);
    if (hardware_threads > 2) {
        k_values.push_back(hardware_threads * 3);
    }
    k_values.push_back(32); // Max up to 32 to see effect of too many threads

    std::sort(k_values.begin(), k_values.end());
    k_values.erase(std::unique(k_values.begin(), k_values.end()), k_values.end());

    std::cout << std::left << std::setw(10) << "K" << std::setw(15) << "Time (s)" << std::setw(15) << "Count" << std::endl;
    std::cout << "---------------------------------------" << std::endl;

    double best_time = std::numeric_limits<double>::max();
    int best_k = -1;
    long long last_count = -1;

    for (int k : k_values) {
        if (k == 0) continue; // Ensure K is at least 1
        double duration = measure_time([&]() {
            last_count = custom_parallel_count_if(large_data, k);
            });
        std::cout << std::setw(10) << k << std::setw(15) << duration << std::setw(15) << last_count << std::endl;

        if (duration < best_time) {
            best_time = duration;
            best_k = k;
        }
    }
    std::cout << std::endl;

    std::cout << "Best K found for custom algorithm: " << best_k << " (Time: " << best_time << " seconds)" << std::endl;
    std::cout << "Ratio Best K / Hardware threads: " << static_cast<double>(best_k) / hardware_threads << std::endl;
    std::cout << std::endl;

     return 0;
}
