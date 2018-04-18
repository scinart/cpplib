#ifndef GITHUB_SCINART_CPPLIB_RAND_HPP_
#define GITHUB_SCINART_CPPLIB_RAND_HPP_

#include <random>

namespace oy
{

class RandInt
{
public:
    RandInt(int begin_inclusive, int end_inclusive):dis(begin_inclusive,end_inclusive){}
    int operator()(){return dis(gen);}
private:
    std::random_device rd;  // Will be used to obtain a seed for the random number engine
    std::mt19937 gen;       // Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> dis;
};

}

#endif