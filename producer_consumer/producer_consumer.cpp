#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <numeric>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <cinttypes>
#include <unistd.h>
#include <semaphore.h>


#if defined(__APPLE__) && defined(__MACH__)
#define USE_NAMED_SEMAPHORES
#endif

template <typename T>
struct Mutex {
    T value;
    std::mutex m;

    explicit Mutex(T value) : value(std::move(value)) {}

    Mutex(const Mutex& other) {
        value = other.value;
    }

    template <typename F>
    void acquire(F&& func) {
        std::unique_lock<std::mutex> lock(m);
        func(value);
    }
};

#ifdef USE_NAMED_SEMAPHORES
struct CountingSemaphore {
    std::string semaphore_name;
    sem_t* semaphore;

    explicit CountingSemaphore(size_t count) {
        auto current_nanos = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        semaphore_name = "cs_" + std::to_string(current_nanos) + "_" + std::to_string(getpid());

        sem_unlink(semaphore_name.c_str());
        semaphore = sem_open(semaphore_name.c_str(), O_CREAT | O_EXCL, 0644, count);

        if (semaphore == SEM_FAILED) {
            throw std::runtime_error("Failed to create semaphore");
        }
    }

    ~CountingSemaphore() {
        sem_unlink(semaphore_name.c_str());
        sem_close(semaphore);
    }

    void acquire() {
        if (sem_wait(semaphore) != 0) {
            throw std::runtime_error("Failed to acquire semaphore");
        }
    }

    void release() {
        if (sem_post(semaphore) != 0) {
            throw std::runtime_error("Failed to release semaphore");
        }
    }
};
#else
struct CountingSemaphore {
    sem_t semaphore;

    explicit CountingSemaphore(int count = 0) {
        if (sem_init(&semaphore, 0, count) != 0) {
            throw std::runtime_error("Failed to initialize semaphore");
        }
    }

    ~CountingSemaphore() {
        sem_destroy(&semaphore);
    }

    void acquire() {
        if (sem_wait(&semaphore) != 0) {
            throw std::runtime_error("Failed to acquire semaphore");
        }
    }

    void release() {
        if (sem_post(&semaphore) != 0) {
            throw std::runtime_error("Failed to release semaphore");
        }
    }
};
#endif

template<typename T>
struct RingBuffer {
    Mutex<std::vector<T>> m_buffer;

    // Number of items in the buffer
    // if s_items = 0 then there are no items in the buffer and pop() will wait
    // if s_items = N then there are N items in the buffer and push() will wait for space
    CountingSemaphore s_items;

    // Number of free spaces in the buffer
    // if s_space = 0 then there is no space in the buffer and push() will wait
    // if s_space = N then there are N free spaces in the buffer and pop() will wait for an item
    CountingSemaphore s_space;

    size_t head{0};
    size_t tail{0};
    size_t size;
    std::vector<int8_t> operations{};

    explicit RingBuffer(size_t size):
        m_buffer(std::vector<T>(size)),
        s_items(0),
        s_space(size),
        size(size) {}

    void push(T value) {
        // Wait until there is space in the buffer
        // if (s_space > 0) then continue (s_space = s_space - 1) else wait()
        s_space.acquire();

        m_buffer.acquire([&](std::vector<T>& buffer) {
            buffer[head] = std::move(value);
            head = (head + 1) % size;

            #ifdef DEBUG
                operations.push_back(1);
            #endif
        });

        // "Notify" that there is a new item in the buffer
        // s_items += 1
        s_items.release();
    }

    T pop() {
        // Wait until there is an item in the buffer
        // if (s_items > 0) then continue (s_items = s_items - 1) else wait()
        s_items.acquire();

        T result;

        m_buffer.acquire([&](std::vector<T>& buffer) {
            result = std::move(buffer[tail]);
            tail = (tail + 1) % size;

            #ifdef DEBUG
                operations.push_back(-1);
            #endif
        });

        // "Notify" that there is a new free space in the buffer
        // s_space += 1
        s_space.release();

        return result;
    }

    void dump_logs() {
        // Write the operations into a file
        // Use the current time in millis as a file suffix
        #ifdef DEBUG
            auto current_millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto file = std::ofstream("logs/operations_" + std::to_string(size) + "_" + std::to_string(current_millis) + ".txt");
            
            for (auto operation : operations) {
                file << operation << std::endl;
            }

            file.close();
        #endif
    }
};

struct RandomIntProducer {
    RingBuffer<int>& buffer;
    int num_items;

    RandomIntProducer(RingBuffer<int>& buffer, int num_items) : buffer(buffer), num_items(num_items) {}

    int generate_random_number() {
        std::random_device random_device;
        auto mt = std::mt19937(random_device());
        auto dist = std::uniform_int_distribution<int>(1, 10'000'000);
        return dist(mt);
    }

    void operator()() {
        for (auto i = 0; i < num_items; ++i) {
            buffer.push(generate_random_number());
        }
    }
};

struct PrimeNumberConsumer {
    RingBuffer<int>& buffer;
    int primes_found{0};
    int numbers_consumed{0};

    explicit PrimeNumberConsumer(RingBuffer<int>& buffer): buffer(buffer) {}

    static bool is_prime(int number) {
        if (number == 1) return false;
        else if (number == 2) return true;
        else if (number % 2 == 0) return false;

        auto root = sqrt(number);

        for (auto i = 2; i <= root; ++i) {
            if (number % i == 0) return false;
        }

        return true;
    }

    void operator()() {
        while (true) {
            auto item = buffer.pop();

            if (item == -1) {
                buffer.push(-1);
                break;
            }

            if (is_prime(item)) {
                primes_found++;
                #ifdef DEBUG
                   std::cout << item, "é primo" << std::endl;
                #endif
            }

            numbers_consumed++;
        }
    }
};

double run_benchmark(int buffer_size, int num_producers, int num_consumers, int num_items) {
    auto buffer = RingBuffer<int>(buffer_size);
    auto producer_items = num_items / num_producers;

    auto producers = std::vector<std::thread>();
    auto consumers = std::vector<std::thread>();

    producers.reserve(num_producers);
    consumers.reserve(num_consumers);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (auto i = 0; i < num_producers; ++i) producers.emplace_back(RandomIntProducer(buffer, producer_items));
    for (auto i = 0; i < num_consumers; ++i) consumers.emplace_back(PrimeNumberConsumer(buffer));

    for (auto& producer : producers) producer.join();

    buffer.push(-1);

    for (auto& consumer : consumers) consumer.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    buffer.dump_logs();

    return duration_us / 1000.0;
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " buffer_size num_producers num_consumers num_items samples" << std::endl;
        return 1;
    }

    const auto buffer_size = std::stoi(argv[1]);
    const auto num_producers = std::stoi(argv[2]);
    const auto num_consumers = std::stoi(argv[3]);
    const auto num_items = std::stoi(argv[4]);
    const auto samples = std::stoi(argv[5]);

    auto runtimes = std::vector<double>();

    for (auto i = 0; i <= samples; i++) {
        auto duration_ms = run_benchmark(buffer_size, num_producers, num_consumers, num_items);
        runtimes.push_back(duration_ms);
    }

    auto avg_time = std::accumulate(runtimes.begin(), runtimes.end(), 0.0) / samples;
    std::cout << avg_time << std::endl;

    return 0;
}
