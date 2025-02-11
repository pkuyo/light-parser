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

    template <typename T, typename Signature>
    struct is_convertible_to_function_impl;

    template <typename T, typename R, typename... Args>
    struct is_convertible_to_function_impl<T, std::function<R(Args...)>> {
    private:
        template <typename U>
        static auto test(int) -> decltype(
        std::is_convertible<decltype(std::declval<U>()(std::declval<Args>()...)), R>::value,
                std::true_type{}
        );

        template <typename>
        static std::false_type test(...);

    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    // Helper template to check if type `T` is convertible to `Signature` type (`std::function<xxx>`).
    template <typename T, typename Signature>
    struct is_convertible_to_function
            : std::false_type {};

    // Helper template to check if type `T` is convertible to `std::function<R(Args...)>`.
    template <typename T, typename R, typename... Args>
    struct is_convertible_to_function<T, std::function<R(Args...)>>
            : is_convertible_to_function_impl<T, std::function<R(Args...)>> {};

    template <typename T, typename Signature>
    constexpr bool is_convertible_to_function_v =
            is_convertible_to_function<T, Signature>::value;


    namespace details
    {
        template <typename T, typename = void>
        struct has_operator_call : std::false_type {};

        template <typename T>
        struct has_operator_call<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

        template <typename Ret, typename... Args>
        std::tuple<Args...> getArgs(Ret (*)(Args...));
        template <typename F, typename Ret, typename... Args>
        std::tuple<Args...>getArgs(Ret (F::*)(Args...));
        template <typename F, typename Ret, typename... Args>
        std::tuple<Args...> getArgs(Ret (F::*)(Args...) const);
        template <typename F>
        requires (has_operator_call<F>::value)
        decltype(getArgs(&std::remove_reference_t<F>::operator())) getArgs(F);

        template <typename T>
        requires (!has_operator_call<T>::value)
        auto getArgs(T) {
            static_assert(false,"Template lambda not instantiated properly");
            return T();
        }


        template <typename Ret, typename... Args>
        Ret getRet(Ret (*)(Args...));
        template <typename F, typename Ret, typename... Args>
        Ret getRet(Ret (F::*)(Args...));
        template <typename F, typename Ret, typename... Args>
        Ret getRet(Ret (F::*)(Args...) const);

        template <typename T>
        requires (!has_operator_call<T>::value)
        auto getRet(T) {
            static_assert(false,"Template lambda not instantiated properly");
            return T();
        }
        template <typename F>
        requires (has_operator_call<F>::value)
        decltype(getRet(&std::remove_reference_t<F>::operator())) getRet(F);


    };
    template <typename F>
    using GetRet = decltype(details::getRet(std::declval<F>()));
    template <size_t N,typename F>
    using GetArg =std::tuple_element_t<N,decltype(details::getArgs(std::declval<F>()))>;
    template <typename F>
    using GetArgs = decltype(details::getArgs(std::declval<F>()));

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
            std::conditional_t<!std::is_same_v<r,remove_smart_pointer_t<r>> && !std::is_same_v<l,remove_smart_pointer_t<l>>,
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
