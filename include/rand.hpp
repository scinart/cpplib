#ifndef GITHUB_SCINART_CPPLIB_RAND_HPP_
#define GITHUB_SCINART_CPPLIB_RAND_HPP_

#include <random>
#include <limits>

namespace oy
{

template<typename Numeric, typename Generator = std::default_random_engine>
class Rand
{
    using Distribution = typename std::conditional<
        std::is_integral<Numeric>::value
        , std::uniform_int_distribution<Numeric>
        , std::uniform_real_distribution<Numeric>
    >::type;
public:
    Rand():gen(rd()){}
    template <typename ...Args>
    Rand(Args... args):gen(rd()), dis(args...){}
    Numeric operator()() { return get(); }
    Numeric get() {return dis(gen); }
    template <typename ...Args>
    Numeric set_range(Args... args) { dis = Distribution(args...); }
    template <typename ...Args>
    Numeric get(Args... args) { return Distribution(args...)(gen); }
    void seed(typename Generator::result_type val){gen.seed(val);}
    template <class Sseq>
    void seed(Sseq& q){gen.seed(q);}
private:
    std::random_device rd;
    Generator gen;
    Distribution dis;
};

template <typename ContainerIn, typename ContainerOut = ContainerIn>
inline ContainerOut generateRandomId(const ContainerIn& str, unsigned int length)
{
    static_assert(std::is_same<typename std::iterator_traits<decltype(str.begin())>::iterator_category, std::random_access_iterator_tag>::value, "");
    auto len = str.end() - str.begin();
    static thread_local std::default_random_engine randomEngine(std::random_device{}());
    std::uniform_int_distribution<int> randomDistribution(0, len-1);
    ContainerOut ret;
    for (unsigned int i = 0; i<length; i++)
        *std::back_inserter(ret) = *(str.begin() + randomDistribution(randomEngine));
    return ret;
}

}

#endif