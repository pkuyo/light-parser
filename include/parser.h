/**
 * @file parser.h
 * @brief This is a lightweight parsing library that uses a compositional structure to build parsers.
 *
 * Main features include:
 * - parser_container: A container class for managing the lifecycle of parsers.
 * - parser_wrapper: A wrapper class for safely using and constructing parsers.
 *
 * @author pkuyo
 * @date 2025-02-04
 * @version 0.1.1
 * @copyright Copyright (c) 2025 pkuyo. All rights reserved.
 *
 * @warning This library is still under development, and the API may change.
 */

#ifndef LIGHT_PARSER_PARSER_H
#define LIGHT_PARSER_PARSER_H

#include <map>
#include <set>
#include <format>
#include <vector>
#include <variant>
#include <string>
#include <regex>

namespace pkuyo::parsers {

    template<size_t N>
    struct ct_string {
        constexpr ct_string(const char(&str)[N]) {
            std::copy_n(str, N, value);
        }

    public:
        static constexpr size_t size = N;
        char value[N]{};
    };


    template<ct_string cts>
    consteval auto operator ""_ct() {
        return cts;
    }

    // Standard exception class for error handling in the parser
    class parser_exception : public std::runtime_error {

    public:

        parser_exception(const std::string_view error_part, const std::string_view & parser_name)
                : std::runtime_error(std::format("parser exception in {}, at {}",parser_name,error_part)), error_part(error_part),  parser_name(parser_name)
        {}

        std::string_view error_part;
        std::string_view parser_name;

    };

    // Used to reset the position point when the parser fails, in order to achieve error recovery functionality.
    template<typename token_type>
    class panic_mode_recovery {
    public:
        using sync_pred_t =  std::function<bool(const token_type&)>;

        // Passes in a function for resetting the position point, with the function type being bool(const token_type&).
        explicit panic_mode_recovery(sync_pred_t pred) : sync_check(pred) {}

        // Checks from iterator `it` using `sync_check` for a synchronization point. Returns the sync point if found, otherwise `end`.
        template<typename Iter>
        Iter recover(Iter it, Iter end) const {
            while(it != end) {
                if(sync_check(*it)) return it;
                ++it;
            }
            return end;
        }

    private:
        sync_pred_t sync_check;
    };



    template<typename token_type>
    class parser_container;


    // A helper template for extracting the raw type from smart pointer.
    template <typename T>
    struct remove_smart_pointer {
        using type = T;
    };

    template <typename T>
    struct remove_smart_pointer<std::unique_ptr<T>> {
        using type = T;
    };

    template<typename T>
    using remove_smart_pointer_t = remove_smart_pointer<T>::type;


    // Helper template to check if a type is a pointer or an array.
    template<typename C,typename T>
    struct is_array_or_pointer : std::bool_constant<
            std::is_same_v<std::decay_t<T>, C*> ||
            std::is_same_v<std::decay_t<T>, const C*> ||
            std::is_array_v<T> && (
                    std::is_same_v<std::remove_extent_t<T>, C> ||
                    std::is_same_v<std::remove_extent_t<T>, const C>
            )
    > {};
    template<typename C,typename T>
    inline constexpr bool is_array_or_pointer_v = is_array_or_pointer<C,T>::value;

    // A helper template for extracting the raw type from pointer or extent.
    template<typename T>
    struct remove_extent_or_pointer{
        typedef std::remove_pointer_t<std::remove_extent_t<T>> type;
    };

    template<typename T>
    using remove_extent_or_pointer_t = remove_extent_or_pointer<T>::type;

    // A helper template to determine whether a type can be used in `std::format`.
    template<typename T, typename = void>
    struct is_formattable : std::false_type {};

    template<typename T>
    struct is_formattable<T, std::void_t<decltype(std::formatter<T>())>> : std::true_type {};



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


}
namespace pkuyo::parsers {


    // Abstract base class for `parser`. Used when:
    //  1. Only the name matters;
    //  2. Unified exception handling is needed;
    //  3. Storing `parser` instances with different `result_type`s uniformly.
    template<typename token_type>
    class _abstract_parser {
    public:
        typedef token_type token_t;
        typedef std::vector<token_t>::const_iterator input_iterator_t;
        virtual ~_abstract_parser() = default;

        // Sets an alias for `parser`.
        // (Usually called automatically in `parser_wrapper`.)
        void set_name(const std::string_view & name) { parser_name = name; }

        // Sets the exception handler. Falls back to `parser_container`'s handler if none is provided.
        // (Usually called automatically in `parser_wrapper`.)
        void set_error_handler(std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)> && func) {error_handler = func;}

        // Sets for panic mode recovery. Falls back to `parser_container`'s recovery if none is provided.
        // (Usually called automatically in `parser_wrapper`.)
        void set_recovery(panic_mode_recovery<token_type> && _recovery) { this->recovery = std::move(_recovery);}

        template<typename T>
        friend class parser_container;

        // Gets the alias for `parser`.
        [[nodiscard]] std::string_view Name() const {return parser_name;}


    protected:
        _abstract_parser() : container(nullptr) {}

        // Handles exception recovery. Invoked on `Parse` errors and may throw `parser_exception`.
        void error_handle_recovery(input_iterator_t &token_it,const input_iterator_t &token_end) {
            auto last_it = token_it;
            token_it = (this->recovery ? this->recovery.value() : this->container->default_recovery).recover(token_it,token_end);
            (this->error_handler ? this->error_handler.value() : this->container->default_error_handler)(*this, last_it == token_end
            ? std::nullopt : std::make_optional(*last_it));

        }

        std::string parser_name;
        parser_container<token_type>* container;

    private:
        std::optional<panic_mode_recovery<token_type>> recovery;
        std::optional<std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)>> error_handler;


    };


    // Generic base class for `parser`. Takes `token_type` as input and returns `std::optional<return_type>`.
    template<typename token_type, typename return_type>
    class base_parser : public _abstract_parser<token_type> {
    public:


        //便于lazy类型推导
        typedef base_parser parser_base_t;

        typedef token_type token_t;
        typedef std::vector<token_t>::const_iterator input_iterator_t;
        typedef std::optional<return_type> return_t;


    public:

        // Predicts if this `parser` can correctly parse the input (single-character lookahead).
        virtual bool Peek(const input_iterator_t &token_it,const input_iterator_t &token_end) = 0;

        // Parses the input sequence from `token_it` to `token_end`. Returns `std::nullopt` only when parsing fails.
        // (May throw exceptions.)
        return_t Parse(const input_iterator_t &token_it,const input_iterator_t &token_end) {
            auto it = token_it;
            return parse_impl(it,token_end);
        }

        // (May throw exceptions.)
        // Advances `token_it` to the end of parsing.
        return_t Parse(input_iterator_t &token_it,const input_iterator_t &token_end) {
            return parse_impl(token_it,token_end);
        }

        template<typename It>
        return_t Parse(It token_it,It token_end) {
            std::vector tmp(token_it,token_end);
            auto it = tmp.cbegin();
            return parse_impl(it,tmp.cend());
        }

        // Parses the input sequence from `token_it` to `token_end`. Returns `std::nullopt` only when parsing fails.


        // Parses the input sequence from `token_it` to `token_end`. Returns `std::nullopt` only when parsing fails.
        // (May throw exceptions.)
        // Advances `token_it` to the end of parsing.
        virtual return_t parse_impl(input_iterator_t &token_it,const input_iterator_t &token_end) = 0;

        virtual ~base_parser() = default;

    protected:
        base_parser() = default;


    };


}

namespace pkuyo::parsers {

    // A parser that accepts any token without consuming it, returning 'nullptr_t'
    template<typename TokenType>
    class parser_empty : public base_parser<TokenType, nullptr_t> {

    public:
        using parser_base_t = base_parser<TokenType, nullptr_t>;

        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t&,
                                               const typename parser_base_t::input_iterator_t&) override {
            return nullptr;
        }

        bool Peek(const typename parser_base_t::input_iterator_t&,
                  const typename parser_base_t::input_iterator_t&) override {

            return true;
        }
    };


    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'nullptr_t';
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_check : public base_parser<token_type,nullptr_t>{
    public:
        typedef base_parser<token_type, nullptr_t> parser_base_t;
        friend class parser_container<token_type>;
    protected:
        explicit parser_check(const cmp_type &_cmp_value) : cmp_value(_cmp_value) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            token_it++;
            return std::make_optional(nullptr);

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == cmp_value;
        }


    private:
        cmp_type cmp_value;
    };
    // A parser that matches tokens.
    //  If the match is successful, it returns a 'nullptr'
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename SequenceType>
    class parser_multi_check : public base_parser<token_type, nullptr_t> {
    public:
        typedef base_parser<token_type, nullptr_t> parser_base_t;
        friend class parser_container<token_type>;

        bool Peek(const typename parser_base_t::input_iterator_t &it,
                  const typename parser_base_t::input_iterator_t &end) override {
            return std::equal(expected_seq.begin(), expected_seq.end(), it,
                              [](const auto& a, const auto& b) { return a == b; });
        }

    protected:
        explicit parser_multi_check(SequenceType _expected_seq)
                : expected_seq(std::move(_expected_seq)) {}


        typename parser_base_t::return_t parse_impl(
                typename parser_base_t::input_iterator_t &it,
                const typename parser_base_t::input_iterator_t &end) override
        {
            auto last_it = it;
            for (const auto& expected : expected_seq) {
                if (it == end || *it != expected) {
                    it = last_it; // restore_it
                    this->error_handle_recovery(it, end);
                    return std::nullopt;
                }
                ++it;
            }
            return nullptr;
        }


    private:
        SequenceType expected_seq;
    };

    // A parser that only matches the first token.
    //  Uses a 'std::function<bool(const token_type &)>' to determine whether the token matches.
    //  If the match is successful, it returns a 'std::unique_ptr<result_type>' constructed with the token as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type>
    class parser_ptr_with_func : public base_parser<token_type, std::unique_ptr<return_type>> {

    public:
        typedef base_parser<token_type, std::unique_ptr<return_type>> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_ptr_with_func(const std::function<bool(const token_type&)> &_cmp_func)
                : cmp_func(_cmp_func){}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            return std::make_optional(std::move(std::make_unique<return_type>(*(token_it++))));

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && cmp_func(*token_it);
        }

    private:
        std::function<bool(const token_type&)> cmp_func;
    };

    // A parser that only matches the first token.
    //  Uses a 'std::function<bool(const token_type &)>' to determine whether the token matches.
    //  If the match is successful, it returns a 'result_type' constructed with the token as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type>
    class parser_value_with_func : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_value_with_func(const std::function<bool(const token_type&)> &_cmp_func)
                : cmp_func(_cmp_func){}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            return std::make_optional<return_type>(*(token_it++));

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && cmp_func(*token_it);
        }

    private:
        std::function<bool(const token_type&)> cmp_func;
    };


    // A parser that matches tokens.
    //  If the match is successful, it returns a 'std::unique<result_type>' constructed with the sequence as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type, typename sequence_type>
    class parser_multi_ptr : public base_parser<token_type, std::unique_ptr<return_type>> {
    public:
        typedef base_parser<token_type, std::unique_ptr<return_type>> parser_base_t;
        friend class parser_container<token_type>;

        bool Peek(const typename parser_base_t::input_iterator_t &it,
                  const typename parser_base_t::input_iterator_t &end) override {
            return std::equal(expected_seq.begin(), expected_seq.end(), it,
                              [](const auto& a, const auto& b) { return a == b; });
        }

    protected:
        parser_multi_ptr(sequence_type _expected_seq,
                         std::function<std::unique_ptr<return_type>(sequence_type)> _constructor)
                : expected_seq(std::move(_expected_seq)), constructor(std::move(_constructor)) {}



        typename parser_base_t::return_t parse_impl(
                typename parser_base_t::input_iterator_t &it,
                const typename parser_base_t::input_iterator_t &end) override
        {
            auto last_it = it;
            for (const auto& expected : expected_seq) {
                if (it == end || *it != expected) {
                    it = last_it;
                    this->error_handle_recovery(it, end);
                    return std::nullopt;
                }
                ++it;
            }
            return constructor(expected_seq);
        }

    private:
        sequence_type expected_seq;
        std::function<std::unique_ptr<return_type>(sequence_type)> constructor;
    };

    // A parser that matches tokens.
    //  If the match is successful, it returns a 'result_type' constructed with the sequence as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type, typename sequence_type>
    class parser_multi_value : public base_parser<token_type, return_type> {
    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

        bool Peek(const typename parser_base_t::input_iterator_t &it,
                  const typename parser_base_t::input_iterator_t &end) override {
            return std::equal(expected_seq.begin(), expected_seq.end(), it,
                              [](const auto& a, const auto& b) { return a == b; });
        }
    protected:

        parser_multi_value(sequence_type _expected_seq, std::function<return_type(sequence_type)> _constructor)
                : expected_seq(std::move(_expected_seq)), constructor(std::move(_constructor)) {}
        typename parser_base_t::return_t parse_impl(
                typename parser_base_t::input_iterator_t &it,
                const typename parser_base_t::input_iterator_t &end) override
        {
            auto last_it = it;
            for (const auto& expected : expected_seq) {
                if (it == end || *it != expected) {
                    it = last_it;
                    this->error_handle_recovery(it, end);
                    return std::nullopt;
                }
                ++it;
            }
            return constructor(expected_seq);
        }

    private:
        sequence_type expected_seq;
        std::function<return_type(sequence_type)> constructor;
    };

    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'std::unique_ptr<return_type>';
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename return_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_ptr : public base_parser<token_type, std::unique_ptr<return_type>> {

    public:
        typedef base_parser<token_type, std::unique_ptr<return_type>> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_ptr(const cmp_type &_cmp_value,
                                             std::function<std::unique_ptr<return_type>(const token_type &)> && constructor)
        : cmp_value(_cmp_value), constructor(constructor) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }

            return std::make_optional(std::move((constructor(*(token_it++)))));


        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == cmp_value;
        }

    private:
        std::function<std::unique_ptr<return_type>(const token_type &)> constructor;
        cmp_type cmp_value;
    };

    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'return_type‘;
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename return_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_value : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_value(const cmp_type &_cmp_value,std::function<return_type(const token_type &)> && constructor)
        : cmp_value(_cmp_value), constructor(constructor) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)) {
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }

            return std::optional<return_type>(std::move(constructor(*(token_it++))));
        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == cmp_value;
        }

    private:

        std::function<return_type(const token_type &)> constructor;
        cmp_type cmp_value;
    };


    // A parser that uses 'std::basic_string_view<token_type>' to compare 'token_type'.
    //  If the match is successful, it returns std::optional<std::basic_string_view<token_type>>;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    //  Typically used when the token_type is 'char' or 'wchar_t'.
    template<typename token_type>
    requires std::equality_comparable<token_type>
    class parser_str : public base_parser<token_type, std::basic_string_view<token_type>> {

    public:
        typedef base_parser<token_type, std::basic_string_view<token_type>> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_str(const std::basic_string_view<token_type> &_cmp_value)
                : cmp_value(_cmp_value) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            auto last_it = token_it;

            for(auto c_it : cmp_value) {
                if(*token_it != c_it) {
                    parser_base_t::error_handle_recovery(token_it,token_end);
                    return std::nullopt;
                }
                token_it++;
            }
            return cmp_value;

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == *cmp_value.begin();
        }

    private:


        std::basic_string_view<token_type> cmp_value;
    };

    //TODO : Comment for regex
    template<typename token_type>
    class parser_regex : public base_parser<token_type, std::string> {
    public:
        typedef base_parser<token_type, std::string> parser_base_t;

        explicit parser_regex(const std::string& _pattern)
                : pattern(_pattern),
                  regex(_pattern, std::regex_constants::ECMAScript | std::regex_constants::optimize)
        {
            // Compile the regular expression, throw an exception if it fails
            if (!regex.flags()) {
                throw std::runtime_error("Invalid regex pattern: " + pattern);
            }
        }

        bool Peek(const typename parser_base_t::input_iterator_t &it,
                  const typename parser_base_t::input_iterator_t &end) override
        {
            // Quick check: at least one character is required and the first character must match
            if (it == end) return false;
            std::string candidate(1, *it); // Check if the first character might match
            return std::regex_search(candidate, regex, std::regex_constants::match_continuous);
        }

        typename parser_base_t::return_t parse_impl(
                typename parser_base_t::input_iterator_t &it,
                const typename parser_base_t::input_iterator_t &end) override
        {
            auto start = it;
            std::string candidate(start, end); // Create a string of the remaining input
            std::smatch match;

            // Perform the regex match (must match continuously from the beginning)
            if (std::regex_search(candidate, match, regex,
                                  std::regex_constants::match_continuous))
            {
                size_t length = match.length(); // Get the length of the match
                std::advance(it, length);       // Move the iterator forward
                return std::string(start, it);  // Return the matched string
            } else {
                this->error_handle_recovery(it, end);
                return std::nullopt;
            }
        }

    private:
        std::string pattern;
        std::regex regex;
    };

    // A parser that sequentially satisfies both base_parser<..., input_type_left> and base_parser<..., input_type_right>.
    //  If the return_type of one of the sub-parsers is nullptr_t, it directly returns the result of the other sub-parser;
    //  otherwise, it returns std::pair<input_type_left, input_type_right>.
    //  If the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type, typename input_type_left,typename input_type_right>
    class parser_then : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_then(base_parser<token_type, input_type_left>* parser_left,base_parser<token_type, input_type_right>* parser_right)
                : children_parsers(std::make_pair(parser_left,parser_right)){}

        parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it
                ,const parser_base_t::input_iterator_t &token_end) override {
            auto last_it = token_it;
            auto left_re = children_parsers.first->parse_impl(token_it,token_end);

            // If the match fails, attempts to recover and restores the token index, then returns nullopt.
            if(left_re == std::nullopt) {
                parser_base_t::error_handle_recovery(token_it,token_end);

                //Restores token_it.
                token_it = last_it;
                return std::nullopt;
            }

            auto right_re = children_parsers.second->parse_impl(token_it,token_end);
            if(right_re == std::nullopt) {
                parser_base_t::error_handle_recovery(token_it,token_end);

                //Restores token_it.
                token_it = last_it;
                return std::nullopt;
            }
            if constexpr (std::is_same_v<input_type_left,nullptr_t>) {
                return std::make_optional(std::move(right_re.value()));
            }
            else if constexpr (std::is_same_v<input_type_right,nullptr_t>) {
                return std::make_optional(std::move(left_re.value()));
            }
            else {
                return std::make_optional<return_type>(std::move(left_re.value()),std::move(right_re.value()));
            }
        }

    public:
        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return children_parsers.first->Peek(token_it,token_end);
        }

    private:
        std::pair<base_parser<token_type, input_type_left>*,base_parser<token_type, input_type_right>*> children_parsers;

    };


    // A parser that queries and returns the result of the first sub-parser that satisfies the condition.
    // Note that it only predicts one token; backtracking for more than one token will result in a parser_exception.
    // The result_type is either the base class of each sub-parser or std::variant<types...>.
    template<typename token_type, typename return_type, typename ...input_types>
    class parser_or : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_or(base_parser<token_type, input_types>*... parsers) : children_parsers(std::make_tuple(parsers...)){}

        parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it
                ,const parser_base_t::input_iterator_t &token_end) override {
            // 遍历 children_parsers，找到第一个 Peek 返回 true 的解析器并调用其 parse_impl
            return parse_impl_or(token_it, token_end, std::make_index_sequence<sizeof...(input_types)>());
        }
    public:
        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return peek_impl(token_it,token_end,std::make_index_sequence<sizeof...(input_types)>());
        }

    private:
        template<std::size_t... Indices>
        parser_base_t::return_t parse_impl_or(typename parser_base_t::input_iterator_t &token_it,
                                           const parser_base_t::input_iterator_t &token_end,
                                           std::index_sequence<Indices...>) {

            // Iterate through the tuple and try to find the first parser where Peek returns true
            return parse_first_valid(token_it, token_end, std::get<Indices>(children_parsers)...);
        }

        template <typename Firstparse_implr, typename... Remainingparse_implrs>
        parser_base_t::return_t parse_first_valid(typename parser_base_t::input_iterator_t &token_it,
                                                  const parser_base_t::input_iterator_t &token_end,
                                                  Firstparse_implr *first, Remainingparse_implrs *...remaining) {

            if (first->Peek(token_it, token_end)) {
                auto last_it = token_it;
                auto re = first->parse_impl(token_it, token_end);
                if(re == std::nullopt) {
                    parser_base_t::error_handle_recovery(token_it,token_end);
                    //Restores token_it.
                    token_it = last_it;
                    return std::nullopt;
                }
                return re;
            } else if constexpr (sizeof...(Remainingparse_implrs) > 0) {
                // Otherwise, recursively process the remaining parsers
                return parse_first_valid(token_it, token_end, remaining...);
            } else {
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;

            }
        }
        template<std::size_t... Indices>
        bool peek_impl(const typename parser_base_t::input_iterator_t &token_it
                ,const parser_base_t::input_iterator_t &token_end
                ,std::index_sequence<Indices...>)  {
            return (std::get<Indices>(children_parsers)->Peek(token_it,token_end) || ...);
        }
    private:
        std::tuple<base_parser<token_type, input_types>*...> children_parsers;

    };

    // Match the child parser 0 or more times, returning std::vector<child_return_type> (std::basic_string<child_return_type> for char or wchar_t).
    template<typename token_type, typename child_return_type>
    class parser_many : public base_parser<token_type,
            std::conditional_t<
                    std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>,
                    std::basic_string<child_return_type>,
                    std::vector<child_return_type>
            >
    > {
    public:
        typedef base_parser<token_type,
                std::conditional_t<
                        std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>,
                        std::basic_string<child_return_type>,
                        std::vector<child_return_type>
                >
        > parser_base_t;
        friend class parser_container<token_type>;

        bool Peek(const typename parser_base_t::input_iterator_t &,
                  const typename parser_base_t::input_iterator_t &) override {
            return true;
        }

    protected:
        explicit parser_many(base_parser<token_type, child_return_type>* child): child_parser(child) {}

        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it,
                                                    const typename parser_base_t::input_iterator_t &token_end) override {
            if constexpr ( std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>) {
                std::basic_string<child_return_type> results;
                while (token_it != token_end && child_parser->Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser->parse_impl(token_it, token_end);
                    if (!result) {
                        this->error_handle_recovery(token_it, token_end);
                        token_it = last_it;
                        break;
                    }
                    results.push_back(*result);
                }
                return std::make_optional(results);
            } else {
                std::vector<child_return_type> results;
                while (token_it != token_end && child_parser->Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser->parse_impl(token_it, token_end);
                    if (!result) {
                        this->error_handle_recovery(token_it, token_end);
                        token_it = last_it;
                        break;
                    }
                    results.push_back(std::move(*result));
                }
                return std::make_optional(std::move(results));
            }
        }

    private:
        base_parser<token_type, child_return_type>* child_parser;
    };

    // Match the child parser 1 or more times, returning std::vector<child_return_type> (std::basic_string<child_return_type> for char or wchar_t).
    // If matching fails, attempt error recovery strategy and return std::nullopt.
    template<typename token_type, typename child_return_type>
    class parser_more : public base_parser<token_type,
            std::conditional_t<
                    std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>,
                    std::basic_string<child_return_type>,
                    std::vector<child_return_type>
            >
    > {
    public:
        typedef base_parser<token_type,
                std::conditional_t<
                        std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>,
                        std::basic_string<child_return_type>,
                        std::vector<child_return_type>
                >
        > parser_base_t;
        friend class parser_container<token_type>;

        bool Peek(const typename parser_base_t::input_iterator_t & it,
                  const typename parser_base_t::input_iterator_t & end) override {
            return child_parser->Peek(it, end);
        }

    protected:
        explicit parser_more(base_parser<token_type, child_return_type>* child): child_parser(child) {}

        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it,
                                                    const typename parser_base_t::input_iterator_t &token_end) override {
            if constexpr ( std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>) {
                std::basic_string<child_return_type> results;
                auto first_result = child_parser->parse_impl(token_it, token_end);
                if (!first_result) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::nullopt;
                }
                results.push_back(*first_result);

                while (token_it != token_end && child_parser->Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser->parse_impl(token_it, token_end);
                    if (!result) {
                        this->error_handle_recovery(token_it, token_end);
                        token_it = last_it;
                        break;
                    }
                    results.push_back(*result);
                }
                return std::make_optional(std::move(results));
            } else {
                std::vector<child_return_type> results;
                auto first_result = child_parser->parse_impl(token_it, token_end);
                if (!first_result) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::nullopt;
                }
                results.push_back(std::move(*first_result));

                while (token_it != token_end && child_parser->Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser->parse_impl(token_it, token_end);
                    if (!result) {
                        this->error_handle_recovery(token_it, token_end);
                        token_it = last_it;
                        break;
                    }
                    results.push_back(std::move(*result));
                }
                return std::make_optional(results);
            }
        }

    private:
        base_parser<token_type, child_return_type>* child_parser;
    };

    // Match an optional element parser. If no match, return std::optional<xxx>(std::nullopt).
    // If match is successful, return std::optional<xxx>(return_value), where return_value is the child type's return value.
    // If matching fails, attempt error recovery strategy and return std::nullopt.
    template<typename token_type, typename return_type>
    class parser_optional : public base_parser<token_type, std::optional<return_type>> {
    public:
        typedef base_parser<token_type, std::optional<return_type>> parser_base_t;
        friend class parser_container<token_type>;

    public:
        bool Peek(const typename parser_base_t::input_iterator_t &,
                  const typename parser_base_t::input_iterator_t &) override {
            return true;
        }

    protected:
        explicit parser_optional(base_parser<token_type, return_type>* child): child_parser(child) {}
        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it,
                                                    const typename parser_base_t::input_iterator_t &token_end) override {
            std::optional<return_type> result(std::nullopt);
            if(child_parser->Peek(token_it,token_end)) {
                auto re = child_parser->parse_impl(token_it, token_end);
                if (re == std::nullopt) {
                    parser_base_t::error_handle_recovery(token_it, token_end);
                    return std::nullopt;
                }
                 result = std::move(re.value());
            }

            return std::make_optional(std::move(result));
        }

    private:
        base_parser<token_type, return_type>* child_parser;
    };


    // Lazy initialization parser to resolve recursive dependencies between parsers.
    // No error recovery.
    // Pass a function of type 'base_parser<token_type, return_type>*()' for lazy initialization.
    template<typename token_type, typename return_type>
    class parser_lazy : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        typedef std::function<base_parser<token_type, return_type>*()> parser_factory_t;
        friend class parser_container<token_type>;

    public:


        bool Peek(const typename parser_base_t::input_iterator_t &token_it,
                  const typename parser_base_t::input_iterator_t &token_end) override {
            if (!real_parser) initialize();
            return real_parser->Peek(token_it, token_end);
        }

    protected:
        explicit parser_lazy(parser_factory_t factory) : factory(factory) {}
        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it,
                                                    const typename parser_base_t::input_iterator_t &token_end) override {
            if (!real_parser) initialize();
            return real_parser->parse_impl(token_it, token_end);
        }

    private:
        void initialize() {
            real_parser = factory();
        }

        parser_factory_t factory;
        base_parser<token_type, return_type>* real_parser = nullptr;
    };

    // Convert the parsing result to the target type's parser.
    // No error recovery.
    template<typename token_type, typename SourceType, typename TargetType>
    class parser_map : public base_parser<token_type, TargetType> {
    public:
        typedef base_parser<token_type, TargetType> parser_base_t;
        typedef std::function<TargetType(SourceType)> mapper_t;
        friend class parser_container<token_type>;

    public:
        bool Peek(const typename parser_base_t::input_iterator_t &token_it,
                  const typename parser_base_t::input_iterator_t &token_end) override {
            return source->Peek(token_it, token_end);
        }

    protected:
        parser_map(base_parser<token_type, SourceType>* source, mapper_t mapper)
                : source(source), mapper(mapper) {}

        typename parser_base_t::return_t parse_impl(
                typename parser_base_t::input_iterator_t &token_it,
                const typename parser_base_t::input_iterator_t &token_end
        ) override {

            auto source_result = source->parse_impl(token_it, token_end);
            if (source_result == std::nullopt) {
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            return std::make_optional(std::move(mapper(std::move(source_result.value()))));
        }
    private:
        base_parser<token_type, SourceType>* source;
        mapper_t mapper;
    };

    template<typename token_type, typename ReturnType>
    class parser_where : public base_parser<token_type, ReturnType> {
    public:

        typedef base_parser<token_type, ReturnType> parser_base_t;
        friend class parser_container<token_type>;
        using Predicate = std::function<bool(const ReturnType&)>;



        bool Peek(const typename parser_base_t::input_iterator_t &token_it,
                  const typename parser_base_t::input_iterator_t &token_end) override {
            return inner_parser->Peek(token_it, token_end);
        }

    protected:
        parser_where(base_parser<token_type, ReturnType>* parser, Predicate pred)
                : inner_parser(parser), predicate(pred) {}
        typename base_parser<token_type, ReturnType>::return_t parse_impl(
                parser_base_t::input_iterator_t& it,
                const parser_base_t::input_iterator_t& end
        ) override {
            auto result = inner_parser->parse_impl(it, end);
            if(!result || !predicate(*result)) {
                return std::nullopt; // 触发错误恢复
            }
            return result;
        }



    private:
        base_parser<token_type, ReturnType>* inner_parser;
        Predicate predicate;
    };
}

namespace pkuyo::parsers {

    // parser_wrapper class used for constructing parser combinators and actual expression parsing.
    template<typename token_type, typename return_type>
    class parser_wrapper {


    public:
        parser_wrapper(parser_container<token_type>& c, base_parser<token_type, return_type>* p)
                : container(c), parser(p) {}

        base_parser<token_type, return_type>* operator->() { return parser; }

        base_parser<token_type, return_type>* Get() {return parser;}

        parser_container<token_type>& Container() {return container;}


        // Overloads '[]' for semantic action injection, returning the injected function's result.
        template <typename Action>
        auto operator[](Action&& action) {
            using new_return_t = decltype(action(std::declval<return_type>()));
            return container.template Map<return_type, new_return_t>(
                    parser,
                    [action = std::forward<Action>(action)](return_type&& val) {
                        return action(std::forward<return_type>(val));
                    }
            );
        }

        // Overloads '>=' for semantic action injection, retaining the original return value.
        template <typename Action>
        auto operator>>=(Action&& action) {
            return container.template Map<return_type, return_type>(
                    parser,
                    [action = std::forward<Action>(action)](return_type&& val) {
                        action(val);
                        return val; // return original value
                    }
            );
        }
        // Creates a parser_or using *this and 'other'.
        // If one of the child parsers' return type is nullptr_t, the return type of parser_or will be the return type of the other child parser.
        // If the return_type of the two child parsers are the same, the return type of parser_or will be the return_type of the child parsers.
        // If the return_type of parsers are pointers and there exists a base class relationship between the types,
        //      the return_type will be the base class of the two return_types.
        // Otherwise, return_type will be std::variant<...>.
        template<typename other_return_t>

        auto operator|(parser_wrapper<token_type, other_return_t> other) {
            if constexpr ( std::is_same_v<return_type,other_return_t>){
                return container.template Or<return_type>(parser, other.parser);
            }
            //non pointer type
            else if constexpr (std::is_same_v<remove_smart_pointer_t<return_type>, return_type> ||
                               std::is_same_v<remove_smart_pointer_t<other_return_t>, other_return_t>) {
                if constexpr (   std::is_same_v<other_return_t, nullptr_t>) {
                    return container.template Or<return_type>(parser, other.parser);
                }
                else if constexpr (   std::is_same_v<return_type, nullptr_t>) {
                    return container.template Or<other_return_t>(parser, other.parser);

                }
                else {
                    return container.template Or<std::variant<return_type, other_return_t>>(parser, other.parser);
                }
            }
            else {
                if constexpr (
                        std::is_base_of_v<remove_smart_pointer_t<return_type>, remove_smart_pointer_t<other_return_t>> ||
                        std::is_same_v<other_return_t, nullptr_t> )
                    return container.template Or<return_type>(parser, other.parser);
                else if constexpr (
                        std::is_base_of_v<remove_smart_pointer_t<other_return_t>, remove_smart_pointer_t<return_type>> ||
                        std::is_same_v<return_type, nullptr_t>)
                    return container.template Or<other_return_t>(parser, other.parser);
                else
                    return container.template Or<std::variant<return_type, other_return_t>>(parser, other.parser);
            }

        }

        // Creates a parser_then using *this and 'other'.
        template<typename other_return_t>
        auto operator>>(parser_wrapper<token_type, other_return_t> other) {
            return container.template Then<return_type, other_return_t>(parser, other.parser);
        }

        // Creates a parser_then using *this and 'other'.
        template<typename other_return_t>
        auto Then(parser_wrapper<token_type, other_return_t> other) {
            return container.template Then<return_type,other_return_t>(parser,other.parser);
        }

        // Creates a parser_or using *this and 'other', with the return_t specified manually.
        template<typename return_t,typename ...other_return_t>
        auto Or(const parser_wrapper<token_type, other_return_t...> & other) {
            return container.template Or<return_t,return_type,other_return_t...>(parser,other);
        }

        // Creates a parser_many using *this
        auto Many() {
            return container.template Many<return_type>(parser);
        }

        // Creates a parser_more using *this
        auto More() {
            return container.template More<return_type>(parser);
        }

        // Creates a parser_where using *this
        template<typename Pred>
        auto Where(Pred pred) {
            using WrappedParser = parser_where<token_type, return_type>;
            auto new_parser = new WrappedParser(this->parser, pred);
            return container.template CreateWrapper<return_type>(new_parser);
        }

        // Creates a parser_optional using *this
        auto Optional() {
            return container.template Optional<return_type>(parser);
        }

        // Discard the original parsing output and return nullptr.
        auto Ignore() {
            return this->Map([](auto &&) {return nullptr;}).Name("Ignore");
        }

        // Creates a parser_map using *this
        template<typename Mapper>
        auto Map(Mapper mapper) {
            using MappedType = decltype(mapper(std::declval<return_type>()));
            return container.template Map<return_type, MappedType>(parser, mapper);
        }

        // Sets an alias for parser.
        parser_wrapper& Name(const std::string_view & new_name) {
            parser->set_name(new_name);
            return *this;
        }
        // Sets an error handle for parser.
        template<typename FF>
        parser_wrapper& OnError(FF && func) {
            parser->set_error_handler(func);
            return *this;
        }

        // Sets an panic recovery for parser.
        template<typename FF>
        parser_wrapper& Recovery(FF && func) {
            parser->set_recovery(panic_mode_recovery(func));
            return *this;
        }


    private:
        parser_container<token_type>& container;
        base_parser<token_type, return_type>* parser;

        template<typename T,typename Y>
        friend class parser_wrapper;

    };

    template<typename token_type, typename R>
    parser_wrapper<token_type, R> operator>>(
            const char* str,
            parser_wrapper<token_type, R> right_parser
    ) {
        auto check_parser = right_parser.Container().Check(std::string_view(str));
        return check_parser >> right_parser;
    }

    template<typename token_type, typename R>
    parser_wrapper<token_type, R> operator>>(
            parser_wrapper<token_type, R> left_parser,
            const char* str
    ) {
        auto check_parser = left_parser.Container().Check(std::string_view(str));
        return left_parser >> check_parser;
    }

}

namespace pkuyo::parsers {


    // Container for storing parsers, managing the lifecycle of parsers.
    // Any parser should be created within the functions of parser_container or parser_wrapper.
    // When `parser_container` is destroyed, all managed `parser` instances are also destroyed.
    // Avoid using any `parser` or `parser_wrapper` after `parser_container` is destroyed.
    template<typename token_type>
    class parser_container {

    public:
        template<typename T>
        friend class _abstract_parser;

        parser_container() : default_error_handler([](const _abstract_parser<token_type> & self,const std::optional<token_type> & token) {

            if(token) {
                if constexpr(is_formattable<token_type>::value) {
                    throw parser_exception(std::format("{}",token.value()), self.Name());
                }
                else {
                    throw parser_exception("TOKEN", self.Name());
                }
            }
            else {
                throw parser_exception("EOF", self.Name());
            }
        }), default_recovery([](const auto &) {return false;}) {}

        ~parser_container() {
            parsers_set.clear();
        }

        template<typename cmp_type>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type>
        parser_wrapper<token_type,nullptr_t> Check(const cmp_type & cmp_value) {

            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_check<token_type,std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Check",result_parser_t>(std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type,nullptr_t>(*this, ptr);
            }
            else {
                using result_parser_t = parser_check<token_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Check",result_parser_t>(cmp_value));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, nullptr_t>(*this, ptr);
            }
        }

        // for string
        auto Check(const char* str) {
            return CheckImpl(std::string_view(str));
        }
        auto Check(const wchar_t* str) {
            return CheckImpl(std::basic_string_view<wchar_t>(str));
        }

        template<typename SequenceType>
        auto Check(const SequenceType& seq) {
            return CheckImpl(seq);
        }

        // Creates a parser_str parser.
        auto Str(const std::basic_string_view<token_type> & cmp_value) {
            using result_parser_t = parser_str<token_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"String",result_parser_t>(cmp_value));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,std::basic_string_view<token_type>>(*this, ptr);

        }

        //TODO: Comment
        auto Regex(const std::string& pattern) {
            using result_parser_t = parser_regex<token_type>;
            try {
                auto tmp = std::unique_ptr<result_parser_t>(new result_parser_t(pattern));
                tmp->set_name("Regex(" + pattern + ")");
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::string>(*this, ptr);
            } catch (const std::regex_error& e) {
                throw std::runtime_error("Regex error: " + std::string(e.what()) + " - Pattern: " + pattern);
            }
        }

        // Creates a parser_value parser.
        template<typename return_type = token_type, typename cmp_type>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type>
        auto SingleValue(const cmp_type & cmp_value) {
            auto constructor = [](const token_type& s) { return return_type(s); };
            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_value<token_type,return_type,std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(
                        std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value),constructor));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type,return_type>(*this, ptr);
            }
            else {
                using result_parser_t = parser_value<token_type,return_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(cmp_value,constructor));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, return_type>(*this, ptr);
            }

        }


        // Creates a parser_ptr parser.
        template<typename return_type, typename cmp_type>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type>
        auto SinglePtr(const cmp_type & cmp_value) {
            auto constructor = [](const token_type& s) {
                return std::make_unique<return_type>(s);
            };
            // Handling for strings.
            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_ptr<token_type,return_type,std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(
                        std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value),constructor));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);
            }
            else {
                using result_parser_t = parser_ptr<token_type,return_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(cmp_value,constructor));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);
            }

        }
        // Creates a parser_value parser with a compare function.
        template<typename return_type = token_type, typename FF>
        requires is_convertible_to_function_v<FF,std::function<bool(const token_type &)>>
        auto SingleValue(FF && compare_func) {

            using result_parser_t = parser_value_with_func<token_type, return_type>;
            auto tmp = std::unique_ptr<result_parser_t>(
                    create_parser<"Single", result_parser_t>(std::forward<FF>(compare_func)));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type, return_type>(*this, ptr);
        }


        // Creates a parser_ptr parser with a compare function.
        template<typename return_type,typename FF>
        requires is_convertible_to_function_v<FF,std::function<bool(const token_type &)>>
        auto SinglePtr(FF && compare_func) {

            using result_parser_t = parser_ptr_with_func<token_type, return_type>;
            auto tmp = std::unique_ptr<result_parser_t>(
                    create_parser<"Single", result_parser_t>(std::forward<FF>(compare_func)));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);


        }
        // Creates a parser_value parser with a custom constructor.
        template<typename return_type = token_type, typename cmp_type,typename FF>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type> &&
                is_convertible_to_function_v<FF,std::function<return_type(const token_type &)>>
        auto SingleValue(const cmp_type & cmp_value,FF && constructor) {

            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_value<token_type,return_type,std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(
                        std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value),std::forward<FF>(constructor)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type,return_type>(*this, ptr);
            }
            else {
                using result_parser_t = parser_value<token_type,return_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(cmp_value,std::forward<FF>(constructor)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, return_type>(*this, ptr);
            }

        }


        // Creates a parser_ptr parser with a custom constructor.
        template<typename return_type, typename cmp_type,typename FF>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type> &&
                is_convertible_to_function_v<FF,std::function<std::unique_ptr<return_type>(const token_type &)>>
        auto SinglePtr(const cmp_type & cmp_value,
                     FF && constructor) {
            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_ptr<token_type,return_type,
                std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(
                        std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value),std::forward<FF>(constructor)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);
            }
            else {
                using result_parser_t = parser_ptr<token_type,return_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(cmp_value,std::forward<FF>(constructor)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);
            }

        }


        template<typename return_type, typename SequenceType>
        auto SeqValue(const SequenceType& seq) {
            using sequence_type = std::decay_t<SequenceType>;
            using result_parser_t = parser_multi_value<token_type, return_type, sequence_type>;

            auto constructor = [](const sequence_type& s) { return return_type(s); };
            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Seq",result_parser_t>(seq, constructor));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,return_type>(*this, ptr);
        }

        template<typename return_type, typename SequenceType>
        auto SeqValue(const SequenceType& seq, std::function<return_type(SequenceType)> constructor) {
            using sequence_type = std::decay_t<SequenceType>;
            using result_parser_t = parser_multi_value<token_type, return_type, sequence_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Seq",result_parser_t>(seq, constructor));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,return_type>(*this, ptr);
        }

        template<typename return_type, typename SequenceType>
        auto SeqPtr(const SequenceType& seq) {
            using sequence_type = std::decay_t<SequenceType>;
            using result_parser_t = parser_multi_ptr<token_type, return_type, sequence_type>;

            auto constructor = [](const sequence_type& s) {
                return std::make_unique<return_type>(s);
            };
            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Seq",result_parser_t>(seq, constructor));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,std::unique_ptr<return_type>>(*this, ptr);
        }

        template<typename return_type, typename SequenceType>
        auto SeqPtr(const SequenceType& seq,
                    std::function<std::unique_ptr<return_type>(SequenceType)> constructor) {
            using sequence_type = std::decay_t<SequenceType>;
            using result_parser_t = parser_multi_ptr<token_type, return_type, sequence_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Seq",result_parser_t>(seq, constructor));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,std::unique_ptr<return_type>>(*this, ptr);
        }


        // Creates a parser_then parser. It is recommended to use the operator overload form of parser_wrapper.
        template<typename input_type_left, typename input_type_right>
        auto Then(base_parser<token_type, input_type_left>*parser_left, base_parser<token_type, input_type_right>* parser_right) {

            if constexpr (std::is_same_v<input_type_left,nullptr_t>) {
                using result_parser_t = parser_then<token_type,input_type_right,input_type_left, input_type_right>;

                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Then", result_parser_t>(parser_left,parser_right));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, input_type_right>(*this, ptr);
            }
            else if constexpr (std::is_same_v<input_type_right,nullptr_t>){
                using result_parser_t = parser_then<token_type, input_type_left,input_type_left, input_type_right>;

                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Then", result_parser_t>(parser_left,parser_right));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, input_type_left>(*this, ptr);
            }
            else{
                using result_parser_t = parser_then<token_type, std::pair<input_type_left,input_type_right>,input_type_left, input_type_right>;

                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Then", result_parser_t>(parser_left,parser_right));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::pair<input_type_left,input_type_right>>(*this, ptr);
            }


        }
        
        // Creates a parser_or parser. It is recommended to use the operator overload form of parser_wrapper.
        template<typename return_type,typename ...input_types>
        parser_wrapper<token_type,return_type> Or(base_parser<token_type, input_types>*... parsers) {
            using result_parser_t = parser_or<token_type,return_type,input_types...>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Or",result_parser_t>(parsers...));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,return_type>(*this, ptr);
        }

        template<typename function_type>
        auto Lazy(function_type && factory) {
            using factory_type = decltype(std::function{std::forward<function_type>(factory)});
            using base_parser = std::remove_pointer_t<decltype(factory())>::parser_base_t;
            
            using return_type = typename base_parser::return_t::value_type;
            using result_parser_t = parser_lazy<token_type, return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Lazy",result_parser_t>(factory_type(factory)));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,return_type>(*this, ptr);
        }

        // Creates a parser_optional parser. It is recommended to use the function of parser_wrapper.
        template<typename return_type>
        auto Optional(base_parser<token_type, return_type>* child) {
            using result_parser_t = parser_optional<token_type, return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Optional",result_parser_t>(child));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));

            return parser_wrapper<token_type, std::optional<return_type>>(*this, ptr);

        }


        // Creates a parser_many parser. It is recommended to use the function of parser_wrapper.
        template<typename child_return_type>
       auto Many(base_parser<token_type, child_return_type>* child) {
            using result_parser_t = parser_many<token_type, child_return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Many",result_parser_t>(child));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,
                    std::conditional_t<
                            std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>,
                            std::basic_string<child_return_type>,
                            std::vector<child_return_type>
                    >>(*this, ptr);
        }

        template<typename child_return_type>
        auto More(base_parser<token_type, child_return_type>* child) {
            using result_parser_t = parser_more<token_type, child_return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"More",result_parser_t>(child));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,
                    std::conditional_t<
                            std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>,
                            std::basic_string<child_return_type>,
                            std::vector<child_return_type>
                    >>(*this, ptr);
        }

        // Creates a parser_map parser. It is recommended to use the function of parser_wrapper.
        template<typename source_type, typename target_type>
        parser_wrapper<token_type,target_type> Map(base_parser<token_type, source_type>* source,std::function<target_type(source_type)> mapper) {
            using result_parser_t = parser_map<token_type, source_type, target_type>;
            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Map",result_parser_t>(source, mapper));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,target_type>(*this, ptr);
        }

        // Creates a parser_empty parser.
        parser_wrapper<token_type,nullptr_t> Empty(){
            if(!empty) {
                auto tmp = std::unique_ptr<parser_empty<token_type>>(create_parser<"Empty",parser_empty<token_type>>());
                empty = tmp.get();
                parsers_set.emplace(std::move(tmp));
            }
            return parser_wrapper<token_type,nullptr_t>(*this,empty);
        }

        // Sets the default panic recovery.
        template<typename FF>
        parser_container& DefaultRecovery(FF && sync_func)
        requires is_convertible_to_function_v<FF,std::function<bool(const token_type &)>>{
            default_recovery = panic_mode_recovery<token_type>(sync_func);
            return *this;
        }

        // Sets the default exception handling function.
        template<typename FF>
        parser_container&  DefaultError(FF && on_error)
        requires is_convertible_to_function_v<FF,std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)>>{
            default_error_handler =std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)> (on_error);
            return *this;
        }

    private:

        template<ct_string str, typename T, typename... Args>
        T* create_parser(Args&&... args) {
            auto p = new T((args)...);
            p->set_name(str.value);
            p->container = this;
            return p;
        }


    private:
        template<typename SequenceType>
        auto CheckImpl(const SequenceType& seq) {
            using parser_t = parser_multi_check<token_type, SequenceType>;
            auto tmp = std::unique_ptr<parser_t>(new parser_t(seq));
            tmp->set_name("MultiCheck");
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type, nullptr_t>(*this, ptr);
        }

    private:
        std::set<std::unique_ptr<_abstract_parser<token_type>>> parsers_set;
        panic_mode_recovery<token_type> default_recovery;
        std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)> default_error_handler;
        parser_empty<token_type>* empty;



    };


}


#endif //LIGHT_PARSER_PARSER_H
