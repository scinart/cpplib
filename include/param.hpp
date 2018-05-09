#pragma once

#include "json.hpp"
#include <tuple>

template <typename... Args>
nlohmann::json serialize(Args && ... args)
{
    return std::make_tuple(args...);
}

template <typename... Args>
std::tuple<Args...> deserialize(const nlohmann::json& j)
{
    return j.get<std::tuple<Args...> >();
}

template <size_t N>
struct call_helper
{
    template <typename Functor, typename Tuple, typename... ArgsF>
    static auto call(Functor f, Tuple && args_t, ArgsF&&... args_f) -> decltype(auto) {
        return call_helper<N-1>::call(f, std::forward<Tuple>(args_t), std::get<N-1>(args_t), std::forward<ArgsF>(args_f)...);
    }
};

template <>
struct call_helper<0>
{
    template <typename Functor, typename Tuple, typename... ArgsF>
    static auto call(Functor f, Tuple&&, ArgsF&&... args_f) -> decltype(auto) {
        return f(std::forward<ArgsF>(args_f)...);
    }
};

//! \brief Calls a functor with arguments provided as a tuple
template <typename Functor, typename Tuple>
auto call(Functor f, Tuple&& args_t) -> decltype(auto)
{
    return call_helper<std::tuple_size<std::remove_reference_t<Tuple> >::value>::call(f, std::forward<Tuple>(args_t));
}

//! \brief Provides a small function traits implementation that
//! works with a reasonably large set of functors.
template <typename T> struct func_traits : func_traits<decltype(&T::operator())> {};
template <typename C, typename R, typename... Args>
struct func_traits<R (C::*)(Args...)> : func_traits<R (*)(Args...)> {};
template <typename C, typename R, typename... Args>
struct func_traits<R (C::*)(Args...) const> : func_traits<R (*)(Args...)> {};
template <typename R, typename... Args> struct func_traits<R (*)(Args...)> {
    using result_type = R;
    using arg_count = std::integral_constant<std::size_t, sizeof...(Args)>;
    using args_type = std::tuple<typename std::decay<Args>::type...>;
};
