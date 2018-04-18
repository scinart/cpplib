#ifndef  _GITHUB_SCINART_CPPLIB_SEMAPHORE_H_
#define  _GITHUB_SCINART_CPPLIB_SEMAPHORE_H_

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace oy
{

class Semaphore {
public:
    Semaphore (int count_ = 0)
        : count(count_) {}

    inline void notify()
    {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        cv.notify_one();
    }

    inline void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);

        while(count == 0){
            cv.wait(lock);
        }
        count--;
    }
    template <typename Duration>
    bool wait_for(const Duration duration)
    {
        std::unique_lock<std::mutex> lock(mtx);
        auto wait_for_it_success = cv.wait_for(lock, duration, [this]() { return count > 0; });
        return wait_for_it_success;
    }
    inline void reset()
    {
        if(mtx.try_lock())
        {
            count=0;
            mtx.unlock();
        }
        else
        {
            throw std::runtime_error("reset Semaphore failed.");
        }
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
};

// TODO: has BUG when unsigned long long overflow
class Semaphore_chronological {
public:
    // constexpr unsigned long long int MAX_SEM_WAIT = 123456789;
    Semaphore_chronological (unsigned long long int count_ = 0) : notify_turn(count_){}

    void notify()
    {
        std::unique_lock<std::mutex> lock(mtx);
        //count++;
        notify_turn++;
        cv.notify_all();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        unsigned int my_wait_turn = wait_turn++;
        cv.wait(lock, [this, my_wait_turn]()
                { // TODO: BUG: fix this when unsigned long long overflow
                    return my_wait_turn <= notify_turn;
                });
    }
    template <typename Duration>
    bool wait_for(const Duration duration)
    {
        std::unique_lock<std::mutex> lock(mtx);
        unsigned int my_wait_turn = wait_turn++;
        auto wait_for_it_success = cv.wait_for(lock, duration, [this, my_wait_turn]()
                { // TODO: BUG: fix this when unsigned long long overflow
                    return my_wait_turn <= notify_turn;
                });
        if (!wait_for_it_success)
        {
            notify_turn++;
            cv.notify_all();
        }
        return wait_for_it_success;
    }
private:
    std::mutex mtx;
    std::condition_variable cv;
    unsigned long long int wait_turn = 1;
    unsigned long long int notify_turn = 0;
};

}

#endif