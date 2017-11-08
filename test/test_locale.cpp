#define BOOST_TEST_MODULE test_locale

#include <boost/test/unit_test.hpp>

#include "locale.hpp"
#include <algorithm>

using namespace oy;

BOOST_AUTO_TEST_SUITE(gbk_utf8_conversion_test)

BOOST_AUTO_TEST_CASE(test_utf8_to_gbk)
{
    std::string utf8_str = "测试";
    char gbk_str[] = {static_cast<char>(0262),static_cast<char>(0342),static_cast<char>(0312),static_cast<char>(0324), 0};
    auto converted_gbk = utf8_to_gbk(utf8_str);

    BOOST_CHECK(converted_gbk == gbk_str);
}

BOOST_AUTO_TEST_CASE(test_gbk_to_utf8)
{
    std::string utf8_str = "测试";
    char gbk_str[] = {static_cast<char>(0262),static_cast<char>(0342),static_cast<char>(0312),static_cast<char>(0324), 0};
    auto converted_gbk = gbk_to_utf8(gbk_str);

    BOOST_CHECK(converted_gbk == utf8_str);
}

BOOST_AUTO_TEST_SUITE_END()
