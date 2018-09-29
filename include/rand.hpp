#ifndef GITHUB_SCINART_CPPLIB_RAND_HPP_
#define GITHUB_SCINART_CPPLIB_RAND_HPP_

#include <random>
#include <limits>

namespace oy
{

template <typename IntType = int>
class RandInt
{
public:
    RandInt(IntType a = std::numeric_limits<IntType>::min(),
            IntType b = std::numeric_limits<IntType>::max())
        :gen(rd()), dis(a,b){}
    IntType get() {return dis(gen); }
    IntType set_range(IntType a, IntType b) { dis = std::uniform_int_distribution<IntType>(a, b); }
    IntType get(IntType a, IntType b) { return std::uniform_int_distribution<IntType>(a, b)(gen); }
private:
    std::random_device rd;  // Will be used to obtain a seed for the random number engine
    std::mt19937_64 gen;       // Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<IntType> dis;
};

}

#endif