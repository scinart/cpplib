#ifndef GITHUB_SCINART_CPPLIB_TYPE_TRAITS_HPP
#define GITHUB_SCINART_CPPLIB_TYPE_TRAITS_HPP

#include <type_traits>
// https://stackoverflow.com/questions/27687389/how-does-void-t-work
// https://www.v2ex.com/t/387904#reply12

namespace oy
{

template <typename T, typename=void> struct is_container : std::false_type {};
template <typename T>                struct is_container <T, std::__void_t< typename T::value_type > > : std::true_type {};

}

#endif