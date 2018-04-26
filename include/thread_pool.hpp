#ifndef _GITHUB_SCINART_CPPLIB_THREAD_POOL_HPP_
#define _GITHUB_SCINART_CPPLIB_THREAD_POOL_HPP_

// source: https://github.com/mlang/wikiwordfreq
// modified my be

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace oy
{

/**
 * Thread Pool of function signature void(*)(T)
 */

template <typename Type, typename Queue = std::queue<std::remove_reference_t<Type>>>
class Distributor: Queue, std::mutex, std::condition_variable {
    typename Queue::size_type capacity;
    bool done = false;
    std::vector<std::thread> threads;

public:
    template<typename Function>
    Distributor( Function function,
                 unsigned int concurrency = std::thread::hardware_concurrency(),
                 typename Queue::size_type capacity_ = std::thread::hardware_concurrency())
        :capacity(capacity_)
    {
        if (not concurrency)
            throw std::invalid_argument("Concurrency must be non-zero");
        if (not capacity)
            throw std::invalid_argument("Queue capacity must be non-zero");

        for (unsigned int count {0}; count < concurrency; count += 1)
            threads.emplace_back(static_cast<void (Distributor::*)(Function)>
                                 (&Distributor::consume), this, function);
    }

    Distributor(Distributor &&) = default;
    Distributor &operator=(Distributor &&) = delete;

    ~Distributor()
    {
        {
            std::lock_guard<std::mutex> guard(*this);
            done = true;
            notify_all();
        }
        for (auto &&thread: threads) thread.join();
    }

    template <typename T>
    void operator()(T &&value)
    {
        std::unique_lock<std::mutex> lock(*this);
        while (Queue::size() == capacity) wait(lock);
        Queue::emplace(std::forward<T>(value));
        notify_one();
    }

private:
    template <typename Function>
    void consume(Function process)
    {
        std::unique_lock<std::mutex> lock(*this);
        while (true) {
            if (not Queue::empty()) {
                std::remove_reference_t<Type> item { std::move(Queue::front()) };
                Queue::pop();
                notify_one();
                lock.unlock();
                process(std::forward<Type>(item));
                lock.lock();
            } else if (done) {
                break;
            } else {
                wait(lock);
            }
        }
    }
};

}

#endif