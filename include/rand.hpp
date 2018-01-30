#ifndef GITHUB_SCINART_CPPLIB_RAND_HPP_
#define GITHUB_SCINART_CPPLIB_RAND_HPP_

#include <random>

namespace oy
{

class RandInt
{
public:
    RandInt():gen(rd()){}
    int getRandInt(int begin_inclusive, int end_inclusive){
        std::uniform_int_distribution<> dis(begin_inclusive,end_inclusive);
        return dis(gen);
    }
    int operator()(int begin_inclusive, int end_inclusive){return getRandInt(begin_inclusive, end_inclusive);}
private:
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen; //Standard mersenne_twister_engine seeded with rd()
};

}

#endif