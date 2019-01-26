#include "rand.hpp"
#include <iostream>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(rand_test)

BOOST_AUTO_TEST_CASE(rand_int)
{
    oy::Rand<int> x(0,9);
    std::vector<int> v(10);

    constexpr int N = 1000;
    for (int i=0; i < N; ++i)
        v[x()]++;

    BOOST_CHECK(std::accumulate(v.begin(), v.end(), 0) == N);
    for (auto i:v)
        BOOST_CHECK_MESSAGE(i>50 && i<150, "Random generator not so random?");
}

BOOST_AUTO_TEST_CASE(rand_real)
{
    oy::Rand<double> x(0,10);
    std::vector<int> v(10);

    constexpr int N = 1000;
    for (int i=0; i < N; ++i)
        v[static_cast<int>(x())]++;

    BOOST_CHECK(std::accumulate(v.begin(), v.end(), 0) == N);
    for (auto i:v)
        BOOST_CHECK_MESSAGE(i>50 && i<150, "Random generator not so random?");
}


BOOST_AUTO_TEST_SUITE_END()
