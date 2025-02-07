//
// Created by pkuyo on 2025/2/6.
//

#ifndef LIGHT_PARSER_TRAITS_H
#define LIGHT_PARSER_TRAITS_H
#include <type_traits>
#include <format>

namespace pkuyo::parsers {

    // A helper template to determine whether a type can be used in `std::format`.
    template<typename T, typename = void>
    struct is_formattable : std::false_type {
    };

    template<typename T>
    struct is_formattable<T, std::void_t<decltype(std::formatter<T>())>> : std::true_type {
    };


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
    using remove_smart_pointer_t = remove_smart_pointer<T>::type;

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
        template <typename Ret, typename... Args>
        std::tuple<Args...> getArgs(Ret (*)(Args...));


        template <typename F, typename Ret, typename... Args>
        std::tuple<Args...>getArgs(Ret (F::*)(Args...));


        template <typename F, typename Ret, typename... Args>
        std::tuple<Args...> getArgs(Ret (F::*)(Args...) const);

        template <typename F>
        decltype(getArgs(&std::remove_reference_t<F>::operator())) getArgs(F);


    };

    template <size_t N,typename F>
    using GetArg =std::tuple_element_t<N,decltype(details::getArgs(std::declval<F>()))>;
    template <typename F>
    using GetArgs = decltype(details::getArgs(std::declval<F>()));

}

namespace pkuyo::parsers{

    template <typename parser_type>
    using parser_result_t = decltype(std::declval<std::decay_t<parser_type>>().Parse(
            std::declval<typename std::vector<typename std::decay_t<parser_type>::token_t>>()))::value_type;


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
    template <typename l, typename r>
    using filter_or_t =filter_or_impl_t<parser_result_t<l>,parser_result_t<r>>;
}

#endif //LIGHT_PARSER_TRAITS_H
