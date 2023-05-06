#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <numeric>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <math.h>
#include <semaphore.h>
#include <unistd.h>

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
        });

        // "Notify" that there is a new free space in the buffer
        // s_space += 1
        s_space.release();

        return result;
    }
};

struct RandomIntProducer {
    RingBuffer<int>& buffer;
    int num_items;

    RandomIntProducer(RingBuffer<int>& buffer, int num_items) : buffer(buffer), num_items(num_items) {}

    void operator()() {
        for (auto i = 0; i < num_items; ++i) {
            buffer.push(rand() % num_items + 1);
        }
    }
};

struct PrimeNumberConsumer {
    RingBuffer<int>& buffer;
    std::vector<int> primes;
    int primes_found{0};

    explicit PrimeNumberConsumer(RingBuffer<int>& buffer) : buffer(buffer) {}

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
                return;
            }

            auto is_prime = true;
            for (auto i = 2; i < item; ++i) {
                if (item % i == 0) {
                    is_prime = false;
                    break;
                }
            }

            if (is_prime) primes_found++;
        }
    }
};

double run_test(int buffer_size, int num_producers, int num_consumers, int num_items) {
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

    return duration_us / 1000.0;
}

// TODO: Remove benchmark loop. Receive parameters as arguments and benchmark using a bash script instead
int main() {
    const auto num_items = 10000;
    const auto max_num_threads = 8;
    const auto num_runs = 10;

    std::cout << std::left << std::setw(15) << "buffer_size"
              << std::left << std::setw(20) << "producer_threads"
              << std::left << std::setw(20) << "consumer_threads"
              << std::left << std::setw(15) << "avg_time" << std::endl;

    for (auto buffer_size = 16; buffer_size <= (1 << 12); buffer_size <<= 1) {
       for (auto num_producers = 1; num_producers <= max_num_threads; ++num_producers) {
           for (auto num_consumers = 1; num_consumers <= max_num_threads; ++num_consumers) {
                std::vector<double> runtimes(num_runs);

                for (auto i = 0; i < num_runs; ++i) {
                    runtimes[i] = run_test(buffer_size, num_producers, num_consumers, num_items);
                }

                auto avg_time = std::accumulate(runtimes.begin(), runtimes.end(), 0.0) / num_runs;

                std::cout.precision(6);

                std::cout << std::left << std::setw(15) << buffer_size
                          << std::left << std::setw(20) << num_producers
                          << std::left << std::setw(20) << num_consumers
                          << std::left << std::setw(15) << avg_time << std::endl;
           }
       }
    }

    return 0;
}
