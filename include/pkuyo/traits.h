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
#include <cstddef>
#include <variant>
#include "token_stream.h"

using nullptr_t = std::nullptr_t;

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



    template<typename T,typename G,typename S>
    struct then_result;


    template<typename G,typename S,typename ...input_types>
    struct then_result<std::tuple<input_types...>,G,S> {



        using raw_type = decltype(std::tuple_cat(
                std::declval<std::conditional_t<is_tuple_v<parser_result_t<input_types,G,S>>,
                        parser_result_t<input_types,G,S>,
                        std::tuple<parser_result_t<input_types,G,S>>>>()...));

        using type = filter_nullptr_t<raw_type>;
    };

    template<typename T,typename G,typename S>
    using then_result_t = then_result<T,G,S>::type;

    template<typename T,typename G,typename S>
    using raw_then_result_t = then_result<T,G,S>::raw_type;

    template<typename Tuple,size_t to_index,size_t set_index,size_t current_index>
    struct tuple_set_index {
        static constexpr size_t value = tuple_set_index<Tuple,
                to_index,
                (std::is_same_v<std::tuple_element_t<current_index,Tuple>,nullptr_t> ? 0 : 1) + set_index,
                current_index+1>::value;
    };

    template<typename Tuple,size_t to_index,size_t set_index>
    struct tuple_set_index<Tuple,to_index,set_index,to_index> {
        static constexpr size_t value = set_index - 1;
    };

    template<typename T,size_t to_index>
    constexpr int tuple_set_index_v = tuple_set_index<T,to_index+1,0,0>::value;

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

    template<typename GState, typename State, typename Tuple>
    struct filter_or_combine;


    template<typename GState, typename State, typename L, typename R>
    struct filter_or_combine<GState, State, std::tuple<L,R>> {
        using type = filter_or_t<L, R, GState, State>;
    };


    template<typename GState, typename State, typename First, typename Second, typename... Rest>
    struct filter_or_combine<GState, State, std::tuple<First, Second, Rest...>> {
        using type = filter_or_impl_t<parser_result_t<First,GState, State>, typename filter_or_combine<GState, State, std::tuple<Second, Rest...>>::type>;
    };

    template<typename Tuple, typename GState, typename State>
    using multi_filter_or_t = typename filter_or_combine<GState, State,Tuple>::type;

    template<typename T>
    using result_container_t = std::conditional_t<std::is_same_v<T,nullptr_t>,nullptr_t,
            std::conditional_t<
                    std::is_same_v<T, char> || std::is_same_v<T, wchar_t>,
                    std::basic_string<T>,
                    std::vector<T>
            >>;

    template <class L, class R>
    concept weakly_equality_comparable_with =
    requires(std::add_lvalue_reference_t<L> __t, std::add_lvalue_reference_t<R> __u) {
        { __t == __u };
        { __t != __u };
        { __u == __t };
        { __u != __t };
    };


    template <typename T, typename = void>
    struct is_complete : std::false_type {};

    template <typename T>
    struct is_complete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};
    template<typename T>
    constexpr bool is_complete_v = is_complete<T>::value;


    template<typename token_type>
    class _abstract_parser;

    template <typename T>
    concept is_parser = (!is_complete_v<T>) || std::is_base_of_v<_abstract_parser<typename std::remove_reference_t<T>::token_t>,std::remove_reference_t<T>>;



    template<typename T>
    class parser_then;

    template<typename T>
    struct is_parser_then : std::false_type {};

    template<typename T>
    struct is_parser_then<parser_then<T>> : std::true_type {};

    template<typename T>
    constexpr bool is_parser_then_v = is_parser_then<T>::value;



    template<typename T,bool with_back_track>
    class parser_or;

    template<typename T>
    struct is_parser_or : std::false_type {};

    template<typename T,bool with_back_track>
    struct is_parser_or<parser_or<T,with_back_track>> : std::true_type {};

    template<typename T>
    constexpr bool is_parser_or_v = is_parser_or<T>::value;

}

#endif //LIGHT_PARSER_TRAITS_H
