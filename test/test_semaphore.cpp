#include "semaphore.hpp"
#include <vector>
#include <thread>
#include <future>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>

#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>

using namespace std::chrono_literals;
using namespace std;
using namespace oy;

Semaphore sem;

constexpr int thread_num = 100;
constexpr int ping_pong  = 3;

void f(int id)
{
    if(id==0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
#ifdef SCINART_CPPLIB_DEBUG_SEMAPHORE
        cout<<"id: "<<id<<" notify"<<endl;
#endif
        sem.notify();
    }

    for(int i=0;i<ping_pong;i++)
    {
        sem.wait();
#ifdef SCINART_CPPLIB_DEBUG_SEMAPHORE
        if(!(id==0 && i==ping_pong-1))
            cout<<"id: "<<id<<" notify"<<endl;
        else
            cout<<"id: "<<id<<" extra notify"<<endl;
#endif
        sem.notify();
    }
}

BOOST_AUTO_TEST_SUITE(semaphore_test)
BOOST_AUTO_TEST_CASE(semaphore_wait_notify)
{
    std::vector<std::thread> v;
    for(int i=0;i<thread_num;i++)
        v.emplace_back(std::thread(f,i));
    for(auto& t : v)
        t.join();
}

BOOST_AUTO_TEST_CASE(semaphore_notify)
{
    Semaphore sem(3);
    sem.wait();
    sem.notify();
    sem.wait();
    sem.wait();
    sem.wait();
}

BOOST_AUTO_TEST_CASE(semaphore_wait_timeout)
{
    auto timer_presicion = 10ms;
    Semaphore sem;

    // signal after 300ms
    auto notify_process = std::async(std::launch::async, [&sem](){
            this_thread::sleep_for(300ms);
            sem.notify();}
        );

    // wait for 100ms, should fail.
    {
        boost::timer::cpu_timer timer;
        auto first_try = sem.wait_for(100ms);
        timer.stop();
        BOOST_CHECK(chrono::nanoseconds(timer.elapsed().wall) < 100ms + timer_presicion);
        BOOST_CHECK(chrono::nanoseconds(timer.elapsed().wall) > 100ms - timer_presicion);
        BOOST_CHECK(!first_try);
    }

    // wait for another 100ms, should fail
    {
        boost::timer::cpu_timer timer;
        auto second_try = sem.wait_for(100ms);
        timer.stop();
        BOOST_CHECK(chrono::nanoseconds(timer.elapsed().wall) < 100ms + timer_presicion);
        BOOST_CHECK(chrono::nanoseconds(timer.elapsed().wall) > 100ms - timer_presicion);
        BOOST_CHECK(!second_try);
    }

    // wait for another 100ms, should success;
    {
        this_thread::sleep_for(110ms);
        boost::timer::cpu_timer timer;
        auto third_try = sem.wait_for(100s);
        timer.stop();
        BOOST_CHECK(chrono::nanoseconds(timer.elapsed().wall) < timer_presicion);
        BOOST_CHECK(third_try);
    }
}


BOOST_AUTO_TEST_SUITE_END()