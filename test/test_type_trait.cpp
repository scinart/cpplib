#include "asio.hpp"
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace oy;

template <typename, typename = void> class F { public: int operator()() {return 0;} };
template <typename Container>
class F<Container, std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> >
{
public:
    int operator()() { return 1; }
};

BOOST_AUTO_TEST_SUITE(type_traits_test)

BOOST_AUTO_TEST_CASE(test_gbk_to_utf8)
{
    BOOST_CHECK(F<std::vector<int> >()() == 1);
    BOOST_CHECK(F<int>()() == 0);
}

BOOST_AUTO_TEST_SUITE_END()
