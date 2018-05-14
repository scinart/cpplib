// #define BOOST_TEST_MODULE test_thread_pool

#include <boost/test/unit_test.hpp>

#include "thread_pool.hpp"
#include <chrono>
#include <thread>
#include <string>
#include <atomic>
#include <iostream>

using namespace oy;
using namespace std::chrono_literals;

static std::atomic<int> count = 0;
namespace
{

class Int
{
public:
    Int (int v_):v(v_){}
    Int (Int&& r):v(r.v){}
    Int operator+(const Int& b) const { return Int(v+b.v); }
private:
    int v;
};

Int add_1(Int a)
{
    count++;
    std::this_thread::sleep_for(100ms);
    return a+Int(1);
}

void string_op(std::string)
{

}

}

BOOST_AUTO_TEST_SUITE(thread_pool_test)
BOOST_AUTO_TEST_CASE(test_destructor_finishes_all)
{
    {
        Distributor<Int> f(add_1, 1, 4);
        for(int i=0;i<5;i++) f(i);
    }
    BOOST_CHECK_MESSAGE(count == 5, (std::string("should call 5 times, actual call count: ") + std::to_string(count)));
}

BOOST_AUTO_TEST_CASE(test_lambda)
{
    int x=0;
    {
        Distributor<int> f([&x](int v){return x=v;}, 1, 4);
        f(10);
    }
    BOOST_CHECK_MESSAGE(x == 10, "lambda capture failed");
}

BOOST_AUTO_TEST_CASE(test_capacity)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    Distributor<int> f([](int){std::this_thread::sleep_for(100ms);}, 4, 8);
    for(int i=0;i<20;i++) f(i);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    BOOST_CHECK_MESSAGE(diff_ms >= 199 && diff_ms <= 220, (std::string("it is actually ") + std::to_string(diff_ms)).c_str());
}

BOOST_AUTO_TEST_CASE(test_lvalue)
{
    std::string s;
    Distributor<std::string> f(string_op, 4, 4);
    f(s);
    f(std::move(s));
}

BOOST_AUTO_TEST_CASE(test_rvalue)
{
    std::string s;
    Distributor<std::string&&> f(string_op, 4, 4);
    f(s);
    f(std::move(s));
}


BOOST_AUTO_TEST_SUITE_END()
