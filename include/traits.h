// traits.h
/**
 * @file traits.h
 * @brief Type trait metaprogramming utilities, supporting parser type deduction.
 * @author pkuyo
 * @date 2025-02-06
 * @copyright Copyright (c) 2025 pkuyo. All rights reserved.
 *
 * Includes:
 * - Type formatting trait is_formattable
 * - Function signature deduction tools (GetRet/GetArg)
 * - Parser result type extraction parser_result_t
 * - Type combination logic (filter_nullptr_t/filter_or_t)
 * - Tuple type trait is_tuple
 */

#ifndef LIGHT_PARSER_TRAITS_H
#define LIGHT_PARSER_TRAITS_H

#include <type_traits>
#include <format>
#include "token_stream.h"

namespace pkuyo::parsers {


    // A helper template for extracting the raw type from smart pointer.
    template <typename T>
    struct remove_smart_pointer {
        using type = T;
    };

    template <typename T>
    struct remove_smart_pointer<std::unique_ptr<T>> {
        using type = T;
    };

    template <typename T>
    struct remove_smart_pointer<std::shared_ptr<T>> {
        using type = T;
    };


    template <typename T>
    using remove_smart_pointer_t = remove_smart_pointer<T>::type;

    template <typename T>
    struct is_smart_pointer : std::false_type {};

    template <typename T>
    struct is_smart_pointer<std::unique_ptr<T>> : std::true_type {};

    template <typename T>
    struct is_smart_pointer<std::shared_ptr<T>> : std::true_type {};

    template <typename T>
    constexpr bool is_smart_pointer_v = is_smart_pointer<T>::value;
}

namespace pkuyo::parsers{

    template <typename parser_type,typename g_state_type,typename state_type>
    using parser_result_t = decltype(std::declval<std::decay_t<parser_type>>().Parse(
        std::declval<std::add_lvalue_reference_t<container_stream<std::vector<typename std::decay_t<parser_type>::token_t>>>>(),
        std::declval<std::add_lvalue_reference_t<std::decay_t<g_state_type>>>(),
        std::declval<std::add_lvalue_reference_t<std::decay_t<state_type>>>()))::value_type;


    //parser_then
    template <typename Tuple>
    struct filter_single {
        using type = Tuple;
    };
    template <typename T>
    struct filter_single<std::tuple<T>> {
        using type = T;
    };
    template <typename Tuple, typename FilteredTuple = std::tuple<>>
    struct filter_nullptr;

    template <typename FilteredTuple>
    struct filter_nullptr<std::tuple<>, FilteredTuple> {
        using type = std::conditional_t<std::is_same_v<FilteredTuple,std::tuple<>>,nullptr_t,FilteredTuple>;
    };

    template <typename T, typename... Ts, typename... FilteredTs>
    struct filter_nullptr<std::tuple<T, Ts...>, std::tuple<FilteredTs...>> {
        using type = std::conditional_t<
                std::is_same_v<nullptr_t,T> || std::is_same_v<std::nullptr_t,T>,
                typename filter_nullptr<std::tuple<Ts...>, std::tuple<FilteredTs...>>::type,
                typename filter_nullptr<std::tuple<Ts...>, std::tuple<FilteredTs..., T>>::type
        >;
    };

    template <typename Tuple>
    using filter_nullptr_t = filter_single<typename filter_nullptr<Tuple>::type>::type;


    template <typename T>
    struct is_tuple : std::false_type {};
    template <typename... Args>
    struct is_tuple<std::tuple<Args...>> : std::true_type {};
    template <typename T>
    constexpr bool is_tuple_v = is_tuple<T>::value;

    template<typename input_type_left,typename input_type_right>
    using then_result_t = filter_nullptr_t<decltype(std::tuple_cat(
            std::declval<std::conditional_t<is_tuple_v<input_type_left>,input_type_left,std::tuple<input_type_left>>>(),
            std::declval<std::conditional_t<is_tuple_v<input_type_right>,input_type_right,std::tuple<input_type_right>>>()))>;

    template <typename Tuple>
    using filter_nullptr_t = filter_single<typename filter_nullptr<Tuple>::type>::type;



    //parser_or
    template <typename l, typename r>
    using filter_or_impl_t =
            std::conditional_t<is_smart_pointer_v<r> && is_smart_pointer_v<l>,
                    std::conditional_t<std::is_base_of_v<remove_smart_pointer_t<l>,remove_smart_pointer_t<r>>,
                            l,
                            std::conditional_t<std::is_base_of_v<remove_smart_pointer_t<r>,remove_smart_pointer_t<l>>,
                                    r,
                                    std::variant<l,r>
                            >
                    >,
                    std::conditional_t<std::is_same_v<r,nullptr_t>  || std::is_same_v<l,r>,
                            l,
                            std::conditional_t<std::is_same_v<l,nullptr_t> ,
                                    r,
                                    std::variant<l,r>>
                    >
            >;
    template <typename l, typename r,typename g_state,typename state>
    using filter_or_t =filter_or_impl_t<parser_result_t<l,g_state,state>,parser_result_t<r,g_state,state>>;

    template<typename T>
    using result_container_t = std::conditional_t<std::is_same_v<T,nullptr_t>,nullptr_t,
            std::conditional_t<
            std::is_same_v<T, char> || std::is_same_v<T, wchar_t>,
            std::basic_string<T>,
            std::vector<T>
    >>;

    template<typename T,typename Y>
    constexpr bool base_or_same_v = std::is_same_v<T,Y> || std::is_base_of_v<T,Y>;
}

#endif //LIGHT_PARSER_TRAITS_H
