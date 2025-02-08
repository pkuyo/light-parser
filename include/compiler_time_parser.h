// compiler_time_parser.h
/**
 * @file compiler_time_parser.h
 * @brief Compile-time parser combinator framework, supporting parser logic composition via operators.
 * @author pkuyo
 * @date 2025-02-07
 * @copyright Copyright (c) 2025 pkuyo. All rights reserved.
 *
 * Includes:
 * - Logical combinators (NOT/PRED/THEN/OR)
 * - Sequence processing parsers (UNTIL/STR/SEQ)
 * - Repetition matching parsers (MANY/MORE)
 * - Lazy parsers (LAZY)
 * - Semantic action parsers (MAP/WHERE)
 * - Operator overloading for syntactic composition
 */

#ifndef LIGHT_PARSER_COMPILER_TIME_PARSER_H
#define LIGHT_PARSER_COMPILER_TIME_PARSER_H

#include "base_parser.h"

namespace pkuyo::parsers {

    // A parser that only peek tokens, where child_parser->Peek() == false, returning 'nullptr'
    template<typename child_type>
    class parser_not : public base_parser<typename std::decay_t<child_type>::token_t,parser_not<child_type>>{
    public:

        constexpr explicit parser_not(child_type && _child_parser) : child_parser(std::forward<child_type>(_child_parser)) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(this->peek_impl(token_it,token_end))
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }
    public:

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return !child_parser.peek_impl(token_it,token_end);
        }


    private:
        child_type child_parser;
    };

    // A parser that only peek tokens, where child_parser->Peek() == true, returning 'nullptr'
    template<typename child_type>
    class parser_pred : public base_parser<typename std::decay_t<child_type>::token_t,parser_pred<child_type>>{
    public:

        constexpr explicit parser_pred(const child_type & _child_parser) : child_parser(_child_parser) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(this->peek_impl(token_it,token_end))
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return child_parser.peek_impl(token_it,token_end);
        }


    private:
        child_type  child_parser;
    };

    template<typename child_type>
    class parser_ignore : public base_parser<typename std::decay_t<child_type>::token_t,parser_ignore<child_type>>{
    public:

        constexpr explicit parser_ignore(const child_type & _child_parser) : child_parser(_child_parser) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            auto re = child_parser.parse_impl(token_it,token_end);
            if(re)
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return child_parser.peek_impl(token_it,token_end);
        }


    private:
        child_type child_parser;
    };

    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'nullptr_t';
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_check : public base_parser<token_type,parser_check<token_type,cmp_type>>{
    public:

        constexpr explicit parser_check(cmp_type && _cmp_value) : cmp_value(std::forward<cmp_type>(_cmp_value)) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->peek_impl(token_it,token_end)){
                this->error_handle_recovery(token_it,token_end);
                return std::optional<nullptr_t>();
            }
            token_it++;
            return std::make_optional(nullptr);

        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return token_end != token_it && (*token_it) == cmp_value;
        }


    private:
        cmp_type cmp_value;
    };


    // A parser class that checks and stores input values until the input equals a specified comparison value.
    // This class is used to continuously receive input and stop storing when the input value equals `cmp_value`. It is primarily designed for parsing data under specific conditions
    template<typename token_type,typename cmp_type>
    struct parser_until : public pkuyo::parsers::base_parser<token_type,parser_until<token_type,cmp_type>> {

        constexpr parser_until(const cmp_type & _cmp) : cmp(_cmp) {}

        auto parse_impl(auto& begin, auto end) const {
            if constexpr(std::is_same_v<token_type,char> ||std::is_same_v<token_type,wchar_t>) {
                if (!peek_impl(begin, end))
                    return std::optional<std::basic_string<token_type>>();
                std::basic_string<token_type> result;
                while (begin != end && *begin != cmp) {
                    result.push_back(*begin);
                    begin++;
                }
                return std::make_optional(std::move(result));
            }
            else {
                if (!peek_impl(begin, end))
                    return std::optional<std::vector<token_type>>();
                std::vector<token_type> result;
                while (begin != end && *begin != cmp) {
                    result.push_back(*begin);
                    begin++;
                }
                return std::make_optional(std::move(result));
            }
        }
        bool peek_impl(auto begin, auto end) const {
            if(begin == end || *begin == cmp)
                return false;
            return true;
        }
        cmp_type cmp;
    };



    // A parser that only matches the first token.
    //  Uses a function to determine whether the token matches.
    //  If the match is successful, it returns a 'std::unique_ptr<result_type>' constructed with the token as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type,typename FF>
    class parser_ptr_with_func : public base_parser<token_type, parser_ptr_with_func<token_type,return_type,FF>> {


        constexpr explicit parser_ptr_with_func(FF _cmp_func)
                : cmp_func(_cmp_func){}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->peek_impl(token_it,token_end)){
                this->error_handle_recovery(token_it,token_end);
                return std::optional<std::unique_ptr<return_type>>();
            }
            return std::make_optional(std::move(std::make_unique<return_type>(*(token_it++))));

        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return token_end != token_it && cmp_func(*token_it);
        }

    private:
        FF cmp_func;
    };

    // A parser that only matches the first token.
    //  Uses a function to determine whether the token matches.
    //  If the match is successful, it returns a 'result_type' constructed with the token as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type, typename FF>
    class parser_value_with_func : public base_parser<token_type, parser_value_with_func<token_type,return_type, FF>> {

    public:

        constexpr explicit parser_value_with_func(FF _cmp_func)
                : cmp_func(_cmp_func){}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->peek_impl(token_it,token_end)){
                this->error_handle_recovery(token_it,token_end);
                return std::optional<return_type>();
            }
            return std::make_optional<return_type>(*(token_it++));

        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return token_end != token_it && cmp_func(*token_it);
        }

    private:
        FF cmp_func;
    };

    // A parser that uses 'std::basic_string_view<token_type>' to compare 'token_type'.
    //  If the match is successful, it returns std::optional<std::basic_string_view<token_type>>;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    //  Typically used when the token_type is 'char' or 'wchar_t'.
    template<typename token_type,size_t size>
    requires std::equality_comparable<token_type>
    class parser_str : public base_parser<token_type, parser_str<token_type,size>> {

    public:

        constexpr explicit parser_str(const token_type (&_cmp_value)[size]){
            std::copy_n(_cmp_value,size,cmp_value);
        }

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            auto last_it = token_it;
            for(int i = 0;i<real_size;i++) {
                if(*token_it != cmp_value[i]) {
                    this->error_handle_recovery(token_it,token_end);

                    //Restores token_it.
                    token_it = last_it;
                    return std::optional<std::basic_string_view<token_type>>();
                }
                token_it++;
            }

            return std::make_optional(std::basic_string_view<token_type>(cmp_value));

        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return token_end != token_it && (*token_it) == *cmp_value;
        }

    private:
        static constexpr size_t real_size = (std::is_same_v<token_type,char> || std::is_same_v<token_type,wchar_t>) ? size-1 : size;

        token_type cmp_value[size]{};
    };

    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'std::unique_ptr<return_type>';
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename return_type, typename cmp_type, typename FF>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_ptr : public base_parser<token_type, parser_ptr<token_type,return_type,cmp_type,FF>> {

    public:

        constexpr parser_ptr(cmp_type && _cmp_value,FF constructor)
                : cmp_value(std::forward<cmp_type>(_cmp_value)), constructor(constructor) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->peek_impl(token_it,token_end)){
                this->error_handle_recovery(token_it,token_end);
                return std::optional<std::unique_ptr<return_type>>();
            }

            return std::make_optional(std::move((constructor(*(token_it++)))));


        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return token_end != token_it && (*token_it) == cmp_value;
        }

    private:
        FF constructor;
        cmp_type cmp_value;
    };

    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'return_type‘;
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename return_type, typename cmp_type,typename FF>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_value : public base_parser<token_type, parser_value<token_type,return_type,cmp_type,FF>> {

    public:

        constexpr parser_value(cmp_type && _cmp_value,FF constructor)
                : cmp_value(std::forward<cmp_type>(_cmp_value)), constructor(constructor) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->peek_impl(token_it,token_end)) {
                this->error_handle_recovery(token_it,token_end);
                return std::optional<return_type>();
            }
            return std::optional<return_type>(std::move(constructor(*(token_it++))));
        }
    public:

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return token_end != token_it && (*token_it) == cmp_value;
        }

    private:

        FF constructor;
        cmp_type cmp_value;
    };


    // A parser that sequentially satisfies both base_parser<..., input_type_left> and base_parser<..., input_type_right>.
    //  If the return_type of one of the sub-parsers is nullptr_t, it directly returns the result of the other sub-parser;
    //  otherwise, it returns std::pair<input_type_left, input_type_right>.
    //  If the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename input_type_left, typename input_type_right>
    class parser_then : public base_parser<typename std::decay_t<input_type_left>::token_t, parser_then<input_type_left,input_type_right>> {

    public:


        constexpr parser_then(const input_type_left &parser_left,const input_type_right & parser_right)
                : children_parsers(std::make_pair(parser_left,parser_right)){}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using result_r = parser_result_t<input_type_right>;
            using result_l = parser_result_t<input_type_left>;

            using return_type = then_result_t<result_l,result_r>;
            auto last_it = token_it;
            auto left_re = children_parsers.first.parse_impl(token_it,token_end);

            // If the match fails, attempts to recover and restores the token index, then returns nullopt.
            if(left_re == std::nullopt) {
                this->error_handle_recovery(token_it,token_end);

                //Restores token_it.
                token_it = last_it;
                return std::optional<return_type>();
            }

            auto right_re = children_parsers.second.parse_impl(token_it,token_end);
            if(right_re == std::nullopt) {
                this->error_handle_recovery(token_it,token_end);

                //Restores token_it.
                token_it = last_it;
                return std::optional<return_type>();
            }
            if constexpr (std::is_same_v<result_l,nullptr_t>) {
                return std::make_optional(std::move(right_re.value()));
            }
            else if constexpr (std::is_same_v<result_r,nullptr_t>) {
                return std::make_optional(std::move(left_re.value()));
            }
            else if constexpr (is_tuple_v<result_l> && is_tuple_v<result_r>){
                return std::make_optional<return_type>(std::tuple_cat(std::move(left_re.value()),std::move(right_re.value())));
            }
            else if constexpr (is_tuple_v<result_l>){
                return std::make_optional<return_type>(std::tuple_cat(std::move(left_re.value()),std::make_tuple(std::move(right_re.value()))));
            }
            else {
                return std::make_optional<return_type>(std::make_tuple(std::move(left_re.value()),std::move(right_re.value())));
            }
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return children_parsers.first.peek_impl(token_it,token_end);
        }

    private:

    private:
        std::pair<input_type_left,input_type_right> children_parsers;

    };


    // A parser that queries and returns the result of the first sub-parser that satisfies the condition.
    // Note that it only predicts one token; backtracking for more than one token will result in a parser_exception.
    // The result_type is either the base class of each sub-parser or std::variant<types...>.
    template<typename input_type_l,typename input_type_r>
    class parser_or : public base_parser<typename std::decay_t<input_type_l>::token_t, parser_or<input_type_l,input_type_r>> {
    public:

        constexpr parser_or(const input_type_l &left,const input_type_r & right) : children_parsers(std::make_pair(left,right)){}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            auto last_it = token_it;

            using result_t = filter_or_t<input_type_l,input_type_r>;
            if(!this->peek_impl(token_it,token_end)) {
                this->error_handle_recovery(token_it, token_end);
                return std::optional<result_t>();
            }
            else if(children_parsers.first.peek_impl(token_it,token_end)) {
                auto re = children_parsers.first.parse_impl(token_it,token_end);
                if(!re) {
                    this->error_handle_recovery(token_it, token_end);
                    token_it = last_it;
                    return std::optional<result_t>();
                }
                else return std::optional<result_t>(std::move(re.value()));
            }
            else {
                auto re = children_parsers.second.parse_impl(token_it,token_end);
                if(!re) {
                    this->error_handle_recovery(token_it, token_end);
                    token_it = last_it;
                    return std::optional<result_t>();
                }
                else return std::make_optional<result_t>(std::move(re.value()));
            }

        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return children_parsers.first.peek_impl(token_it,token_end) || children_parsers.second.peek_impl(token_it,token_end);
        }

    private:

    private:
        std::pair<input_type_l,input_type_r> children_parsers;

    };

    // Match the child parser 0 or more times, returning std::vector<child_return_type> (std::basic_string<child_return_type> for char or wchar_t).
    template<typename child_type>
    class parser_many : public base_parser<typename std::decay_t<child_type>::token_t,parser_many<child_type>> {

    public:
        constexpr explicit parser_many(const child_type & child): child_parser(child) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using child_return_type = decltype(child_parser.parse_impl(token_it,token_end))::value_type;
            if constexpr ( std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>) {
                std::basic_string<child_return_type> results;
                while (token_it != token_end && child_parser.peek_impl(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.parse_impl(token_it, token_end);
                    if (!result) {
                        this->error_handle_recovery(token_it, token_end);
                        token_it = last_it;
                        break;
                    }
                    results.push_back(*result);
                }
                return std::make_optional(std::move(results));
            }
            else if constexpr(std::is_same_v<child_return_type,nullptr_t>) {
                while (token_it != token_end && child_parser.peek_impl(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.parse_impl(token_it, token_end);
                    if (!result) {
                        this->error_handle_recovery(token_it, token_end);
                        token_it = last_it;
                        break;
                    }

                }
                return std::make_optional(nullptr);
            }
            else {
                std::vector<child_return_type> results;
                while (token_it != token_end && child_parser.peek_impl(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.parse_impl(token_it, token_end);
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
        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return true;
        }

    private:
        child_type child_parser;
    };

    // Match the child parser 1 or more times, returning std::vector<child_return_type> (std::basic_string<child_return_type> for char or wchar_t).
    // If matching fails, attempt error recovery strategy and return std::nullopt.
    template<typename child_type>
    class parser_more : public base_parser<typename std::decay_t<child_type>::token_t,parser_more<child_type>> {

    public:

        constexpr explicit parser_more(const child_type & child): child_parser(child) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using child_return_type = decltype(child_parser.parse_impl(token_it,token_end))::value_type;
            if constexpr ( std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>) {
                std::basic_string<child_return_type> results;
                auto first_result = child_parser.parse_impl(token_it, token_end);
                if (!first_result) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::optional<std::basic_string<child_return_type>>();
                }
                results.push_back(*first_result);

                while (token_it != token_end && child_parser.peek_impl(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.parse_impl(token_it, token_end);
                    if (!result) {
                        this->error_handle_recovery(token_it, token_end);
                        token_it = last_it;
                        break;
                    }
                    results.push_back(*result);
                }
                return std::make_optional(std::move(results));
            }
            else if constexpr ( std::is_same_v<child_return_type, nullptr_t> ) {
                auto first_result = child_parser.parse_impl(token_it, token_end);
                if (!first_result) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::optional<nullptr_t>();
                }

                while (token_it != token_end && child_parser.peek_impl(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.parse_impl(token_it, token_end);
                    if (!result) {
                        this->error_handle_recovery(token_it, token_end);
                        token_it = last_it;
                        break;
                    }
                }
                return std::make_optional(nullptr);
            }
            else {
                std::vector<child_return_type> results;
                auto first_result = child_parser.parse_impl(token_it, token_end);
                if (!first_result) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::optional<std::vector<child_return_type>>();
                }
                results.push_back(std::move(*first_result));

                while (token_it != token_end && child_parser.peek_impl(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.parse_impl(token_it, token_end);
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

        template <typename Iter>
        bool peek_impl(Iter it, Iter end) const {
            return child_parser.peek_impl(it, end);
        }
    private:
        child_type child_parser;
    };

    // Match an optional element parser. If no match, return std::optional<xxx>(std::nullopt).
    // If match is successful, return std::optional<xxx>(return_value), where return_value is the child type's return value.
    // If matching fails, attempt error recovery strategy and return std::nullopt.
    template<typename child_type>
    class parser_optional : public base_parser<typename std::decay_t<child_type>::token_t, parser_optional<child_type>> {
    public:

        constexpr explicit parser_optional(const child_type & child): child_parser(child) {}
        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using return_type = decltype(child_parser.parse_impl(token_it,token_end))::value_type;
            std::optional<return_type> result(std::nullopt);
            if(child_parser.peek_impl(token_it,token_end)) {
                auto re = child_parser.parse_impl(token_it, token_end);
                if (re == std::nullopt) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::optional<std::optional<return_type>>();
                }
                result = std::move(re.value());
            }

            return std::make_optional(std::move(result));
        }

        template <typename Iter>
        bool peek_impl(Iter it, Iter end) const {
            return true;
        }

    private:
        child_type child_parser;
    };


    // Lazy initialization parser to resolve recursive dependencies between parsers.
    // No error recovery.
    template <typename token_type, typename real_type>
    class parser_lazy : public base_parser<token_type,parser_lazy<token_type,real_type>> {
    public:

        template <typename Iter>
        bool peek_impl(Iter begin, Iter end) const {
            return factory().peek_impl(begin, end);
        }

        template <typename Iter>
        auto parse_impl(Iter& begin, Iter end) const {
            return factory().parse_impl(begin, end);
        }
    private:
       static real_type& factory() {
            static real_type real;
            return real;
        }

    };


    // Convert the parsing result to the target type's parser.
    // No error recovery.
    template<typename child_type, typename FF>
    class parser_map : public base_parser<typename std::decay_t<child_type>::token_t, parser_map<child_type,FF>> {
    public:

        using mapper_t = FF;

        constexpr parser_map(const child_type & source, mapper_t mapper)
                : source(source), mapper(mapper) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using SourceType = parser_result_t<child_type>;
            using TargetType = decltype(std::declval<mapper_t>()(std::declval<SourceType>()));

            auto source_result = source.parse_impl(token_it, token_end);
            if (source_result == std::nullopt) {
                this->error_handle_recovery(token_it,token_end);
                return std::optional<TargetType>();
            }
            return std::make_optional(std::move(mapper(std::move(source_result.value()))));
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return source.peek_impl(token_it, token_end);
        }



    private:
        child_type source;
        mapper_t mapper;
    };

    template<typename child_type,typename FF>
    class parser_where : public base_parser<typename std::decay_t<child_type>::token_t, parser_where<child_type,FF>> {
    public:
        using Predicate = FF;

        constexpr parser_where(const child_type &  parser, Predicate pred)
                : child_parser(parser), predicate(pred) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using ReturnType = parser_result_t<child_type>;

            auto result = child_parser.parse_impl(token_it, token_end);
            if(!result || !predicate(*result)) {
                return std::optional<ReturnType>();
            }
            return result;
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return child_parser.peek_impl(token_it, token_end);
        }
    private:
        child_type child_parser;
        Predicate predicate;
    };

    template<typename token_type,typename buff_type,size_t buff_size>
    class parser_buff_seq_check : public base_parser<token_type,parser_buff_seq_check<token_type,buff_type,buff_size>> {
    public:
        constexpr parser_buff_seq_check(const buff_type (&input)[buff_size]) {
            std::copy_n(input,buff_size,cmp);
        }

        template <typename Iter>
        auto parse_impl(Iter& it, Iter end) const {
            if(!this->peek_impl(it,end)) {
                this->error_handle_recovery(it, end);
                return std::optional<nullptr_t>();
            }
            if constexpr (std::random_access_iterator<Iter>) {
                it +=real_size;
                return std::make_optional(nullptr);
            } else {
                for (int i = 0; i< real_size; i++,++it);
                return std::make_optional(nullptr);
            }
        }
        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            if(std::distance(token_it,token_end) < real_size)
                return false;
            auto re = std::equal(token_it,token_it+real_size,cmp,
                                 [](const auto &a, const auto &b) { return a == b; });
            return re;
        }
        static constexpr size_t real_size = (std::is_same_v<buff_type,char> || std::is_same_v<buff_type,wchar_t>) ? buff_size-1 : buff_size;

        buff_type cmp[buff_size]{};
    };

    template<typename token_type,typename return_type,typename buff_type,size_t buff_size,typename FF>
    class parser_buff_seq_value : public base_parser<token_type,parser_buff_seq_value<token_type,return_type,buff_type,buff_size,FF>> {
    public:
        constexpr parser_buff_seq_value(const buff_type (&input)[buff_size], FF constructor) : constructor(constructor) {
            std::copy_n(input,buff_size,cmp);
        }

        template <typename Iter>
        auto parse_impl(Iter& it, Iter end) const {
            if(!this->peek_impl(it,end)) {
                this->error_handle_recovery(it, end);
                return std::optional<return_type>();
            }
            if constexpr (std::random_access_iterator<Iter>) {
                it += real_size;
                return std::make_optional(constructor(cmp));
            } else {
                for (int i = 0; i< real_size; i++,++it);
                return std::make_optional(constructor(cmp));
            }
        }
        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            if(std::distance(token_it,token_end) < real_size)
                return false;
            auto re = std::equal(token_it,token_it+real_size,cmp,
                                 [](const auto &a, const auto &b) { return a == b; });
            return re;
        }

        buff_type cmp[buff_size]{};

        static constexpr size_t real_size = (std::is_same_v<buff_type,char> || std::is_same_v<buff_type,wchar_t>) ? buff_size-1 : buff_size;

        FF constructor;
    };

    template<typename token_type,typename return_type,typename buff_type,size_t buff_size,typename FF>
    class parser_buff_seq_ptr : public base_parser<token_type,parser_buff_seq_ptr<token_type,return_type,buff_type,buff_size,FF>> {
    public:
        constexpr parser_buff_seq_ptr(const buff_type (&input)[buff_size], FF constructor) : constructor(constructor) {
            std::copy_n(input,buff_size,cmp);
        }

        template <typename Iter>
        auto parse_impl(Iter& it, Iter end) const {
            if(!this->peek_impl(it,end)) {
                this->error_handle_recovery(it, end);
                return std::optional<std::unique_ptr<return_type>>();
            }
            if constexpr (std::random_access_iterator<Iter>) {
                it += real_size;
                return std::make_optional(constructor(cmp));
            } else {
                for (int i = 0; i< real_size; i++,++it);
                return std::make_optional(constructor(cmp));
            }
        }
        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            if(std::distance(token_it,token_end) < real_size)
                return false;
            auto re = std::equal(token_it,token_it+real_size,cmp,
                                 [](const auto &a, const auto &b) { return a == b; });
            return re;
        }
        buff_type cmp[buff_size]{};

        static constexpr size_t real_size = (std::is_same_v<buff_type,char> || std::is_same_v<buff_type,wchar_t>) ? buff_size-1 : buff_size;

        FF constructor;
    };

}

namespace pkuyo::parsers {

    template<typename token_type, typename cmp_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    constexpr auto Check(cmp_type &&cmp) {
        return parser_check<token_type, cmp_type>(std::forward<cmp_type>(cmp));
    }

    template<typename token_type, typename cmp_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    constexpr auto Until(cmp_type &&cmp) {
        return parser_until<token_type, cmp_type>(std::forward<cmp_type>(cmp));
    }

    template<typename token_type,size_t size>
    auto Str(const token_type (&str)[size]) {
        return parser_str<token_type,size>(str);
    }

    template<typename token_type, typename cmp_type = token_type,typename return_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    constexpr auto SingleValue(cmp_type && cmp_value) {
        auto constructor = [](const token_type& s) { return return_type(s); };
        return parser_value<token_type,return_type,cmp_type, decltype(constructor)>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    constexpr auto SinglePtr(cmp_type && cmp_value) {
        auto constructor = [](const token_type& s) { return std::make_unique<return_type>(s); };
        return parser_ptr<token_type,return_type,cmp_type, decltype(constructor)>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type,typename FF>
    constexpr auto SingleValue(cmp_type && cmp_value, FF constructor) {
        return parser_value<token_type,return_type,cmp_type, FF>(std::forward<cmp_type>(cmp_value),constructor);
    }

    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type,typename FF>
    constexpr auto SinglePtr(cmp_type && cmp_value,FF constructor) {
        return parser_ptr<token_type,return_type,cmp_type,FF>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type,typename return_type = token_type, typename FF>
    requires (!std::__weakly_equality_comparable_with<token_type, FF>)
    constexpr auto SingleValue(FF compare_func) {
        return parser_value_with_func<token_type, return_type,FF>(compare_func);
    }

    template<typename token_type,typename return_type, typename FF>
    requires (!std::__weakly_equality_comparable_with<token_type, FF>)
    constexpr auto SinglePtr(FF compare_func) {
        return parser_ptr_with_func<token_type, return_type,FF>(compare_func);
    }

    template<typename token_type, typename cmp_type, size_t size>
    constexpr auto SeqCheck(const cmp_type (&s)[size]) {
        return parser_buff_seq_check<token_type, cmp_type,size>(s);
    }

    template<typename token_type,typename return_type, typename cmp_type, size_t size>
    constexpr auto SeqValue(const cmp_type (&cmp)[size]) {
        constexpr auto constructor = [](const cmp_type (&s)[size]) { return return_type(s); };
        return parser_buff_seq_value<token_type,return_type, cmp_type,size, decltype(constructor)>(cmp,constructor);
    }

    template<typename token_type,typename return_type, typename cmp_type, size_t size>
    constexpr auto SeqPtr(const cmp_type (&cmp)[size]) {
        constexpr auto constructor = [](const cmp_type (&s)[size]) { return std::make_unique<return_type>(s); };
        return parser_buff_seq_ptr<token_type,return_type, cmp_type,size, decltype(constructor)>(cmp,constructor);
    }

    template<typename token_type,typename return_type,typename FF, typename cmp_type, size_t size>
    constexpr auto SeqValue(const cmp_type (&cmp)[size],FF ff) {
        return parser_buff_seq_value<token_type,return_type, cmp_type,size, FF>(cmp,ff);
    }

    template<typename token_type,typename return_type,typename FF, typename cmp_type, size_t size>
    constexpr auto SeqPtr(const cmp_type (&cmp)[size], FF ff) {
        return parser_buff_seq_ptr<token_type,return_type, cmp_type,size, FF>(cmp,ff);
    }

    template<typename child_type>
    requires is_parser<child_type>
    constexpr auto Not(child_type && parser) {
        return parser_not(std::forward<std::remove_reference_t<child_type>>(parser));
    }

    template<typename child_type>
    requires is_parser<child_type>
    constexpr auto Pred(child_type && parser) {
        return parser_pred(std::forward<std::remove_reference_t<child_type>>(parser));
    }


    template <typename token_type,typename real_parser>
    //requires is_parser<real_parser>
    constexpr auto Lazy() {
        return parser_lazy<token_type,real_parser>();
    }

    template<typename child_type>
    requires is_parser<child_type>
    constexpr auto Optional(child_type && child) {
        return parser_optional<std::remove_reference_t<child_type>>(std::forward<child_type>(child));
    }

    template<typename child_type>
    requires is_parser<child_type>
    constexpr auto Many(child_type && child) {
        return parser_many<std::remove_reference_t<child_type>>(std::forward<child_type>(child));
    }

    template<typename child_type>
    requires is_parser<child_type>
    constexpr auto More(child_type && child) {
        return parser_more<std::remove_reference_t<child_type>>(std::forward<child_type>(child));
    }



    template<typename child_type, typename FF>
    requires is_parser<child_type>
    constexpr auto Map(child_type && child, FF && mapper) {
        return parser_map<std::remove_reference_t<child_type>, std::remove_reference_t<FF>>
                                                                                         (std::forward<child_type>(child),std::forward<FF>(mapper));
    }

    template<typename child_type, typename FF>
    requires is_parser<child_type>
    constexpr auto Where(child_type && child, FF && mapper) {
        return parser_where<std::remove_reference_t<child_type>,std::remove_reference_t<FF>>
                                                                                          (std::forward<child_type>(child),std::forward<FF>(mapper));
    }

    template<typename l,typename r>
    requires std::is_same_v<typename std::remove_reference_t<l>::token_t,typename std::remove_reference_t<r>::token_t> && is_parser<l> && is_parser<r>
    constexpr auto Or(l&& left, r&& right) {
        return parser_or<std::remove_reference_t<l>,std::remove_reference_t<r>>(std::forward<l>(left),std::forward<r>(right));
    }

    template<typename l,typename r>
    requires std::is_same_v<typename std::remove_reference_t<l>::token_t,typename std::remove_reference_t<r>::token_t> && is_parser<l> && is_parser<r>
    constexpr auto Then(l&& left, r&& right) {
        return parser_then<std::remove_reference_t<l>,std::remove_reference_t<r>>(std::forward<l>(left),std::forward<r>(right));
    }

    template<typename child_type>
    requires is_parser<child_type>
    constexpr auto Ignore(child_type && child) {
        return parser_ignore<std::remove_reference_t<child_type>>(child);
    }
}

namespace pkuyo::parsers {

    template<typename chr,typename parser_type_r,size_t size>
    requires is_parser<parser_type_r> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, chr>
    constexpr auto operator>>(const chr (&str)[size],parser_type_r && right) {
        auto check_parser = SeqCheck<typename std::decay_t<parser_type_r>::token_t,chr,size>(str);
        using parser_type_l = decltype(check_parser);
        return Then<parser_type_l,parser_type_r>
                (std::forward<parser_type_l>(check_parser),std::forward<parser_type_r>(right));
    }

    template<typename chr,typename parser_type_r,size_t size>
    requires is_parser<parser_type_r> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, chr>
    constexpr auto operator|(const chr (&str)[size],parser_type_r && right) {
        auto check_parser = SeqCheck<typename std::decay_t<parser_type_r>::token_t,chr,size>(str);
        using parser_type_l = decltype(check_parser);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(check_parser),std::forward<parser_type_r>(right));
    }

    template<typename parser_type_l,typename chr,size_t size>
    requires is_parser<parser_type_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, chr>
    constexpr auto operator|(parser_type_l && left,const chr (&str)[size]) {
        auto check_parser = SeqCheck<typename std::decay_t<parser_type_l>::token_t,chr,size>(str);
        using parser_type_r = decltype(check_parser);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(left),std::forward<parser_type_r>(check_parser));
    }
    template<typename parser_type_l,typename chr,size_t size>
    requires is_parser<parser_type_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, chr>
    constexpr auto operator>>(parser_type_l && left,const chr (&str)[size]) {
        auto check_parser = SeqCheck<typename std::decay_t<parser_type_l>::token_t,chr,size>(str);
        using parser_type_r = decltype(check_parser);
        return Then<parser_type_l,parser_type_r>
                (std::forward<parser_type_l>(left),std::forward<parser_type_r>(check_parser));
    }

    // Creates a parser_or.
    template<typename parser_type_l, typename parser_type_r>
    requires std::is_same_v<typename std::decay_t<parser_type_l>::token_t,typename std::decay_t<parser_type_r>::token_t> &&
             is_parser<parser_type_l> &&
             is_parser<parser_type_r>
    constexpr auto operator|(parser_type_l&& left,parser_type_r&& right) {
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(left),std::forward<parser_type_r>(right));
    }

    template<typename parser_type_l, typename cmp_type>
    requires is_parser<parser_type_l> &&  std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, cmp_type>
    constexpr auto operator|(parser_type_l&& left,cmp_type&& right) {
        auto r = Check<typename std::decay_t<parser_type_l>::token_t,cmp_type>(std::forward<cmp_type>(right));
        using parser_type_r = decltype(r);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(left),std::forward<parser_type_r>(r));
    }

    template<typename cmp_type, typename parser_type_r>
    requires is_parser<parser_type_r> &&  std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, cmp_type>
    constexpr auto operator|(cmp_type&& left,parser_type_r&& right) {
        auto l = Check<typename std::decay_t<parser_type_r>::token_t,cmp_type>(std::forward<cmp_type>(left));
        using parser_type_l = decltype(l);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(l),std::forward<parser_type_r>(right));
    }


    // Creates a parser_then
    template<typename parser_type_l, typename cmp_type>
    requires is_parser<parser_type_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, cmp_type>
    constexpr auto operator>>(parser_type_l&& left,cmp_type&& right) {
        using token_t = std::decay_t<parser_type_l>::token_t;

        auto r = Check<token_t,cmp_type>(std::forward<cmp_type>(right));
        return Then<parser_type_l, decltype(r)>
                (std::forward<parser_type_l>(left),std::move(r));
    }

    template<typename cmp_type, typename parser_type_r>
    requires is_parser<parser_type_r> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, cmp_type>
    constexpr auto operator>>(cmp_type&& left,parser_type_r&& right) {
        using token_t = std::decay_t<parser_type_r>::token_t;

        auto l = Check<token_t,cmp_type>(std::forward<cmp_type>(left));
        return Then<decltype(l),parser_type_r>
                (std::move(l),std::forward<parser_type_r>(right));
    }

    template<typename parser_type_l, typename parser_type_r>
    requires is_parser<parser_type_l> &&  is_parser<parser_type_r>
    constexpr auto operator>>(parser_type_l&& left,parser_type_r&& right) {
        return Then<parser_type_l,parser_type_r>
                (std::forward<parser_type_l>(left),std::forward<parser_type_r>(right));
    }




    // Creates a parser_not that only pred tokens using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    constexpr auto operator!(parser_type && parser) {return Not(std::forward<parser_type>(parser));}

    // Creates a parser_many using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    constexpr auto operator*(parser_type && parser) {return Many(std::forward<parser_type>(parser));}

    // Creates a parser_more using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    constexpr auto operator+(parser_type && parser) {return More(std::forward<parser_type>(parser));}

    // Creates a parser that only pred tokens using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    constexpr auto operator&(parser_type && parser) {return Pred(std::forward<parser_type>(parser));}

    // Creates a optional parser that
    template<typename parser_type>
    requires is_parser<parser_type>
    constexpr auto operator~(parser_type && parser) {return Optional(std::forward<parser_type>(parser));}

    // Creates a parser that ignord original output and return nullptr using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    constexpr auto operator-(const parser_type & parser) {return Ignore(parser);}

    // Overloads '>>=' for semantic action injection, returning the injected function's result.
    template <typename parser_type,typename Mapper>
    requires is_parser<parser_type>
    constexpr auto operator>>=(parser_type && parser, Mapper&& mapper) {
        return Map(std::forward<parser_type>(parser),mapper);
    }

    // Overloads '<<=' for semantic action injection, retaining the original return value.
    template <typename parser_type,typename Action>
    requires is_parser<parser_type>
    constexpr auto operator<<=(const parser_type & parser,Action&& action) {
        return Map(
                parser,
                [action = std::forward<Action>(action)](GetArg<0,Action> && val) {
                    action(val);
                    return val; // return original value
                }
        );
    }

    //Creates parser_where using *this and pred function.
    template <typename parser_type,typename Action>
    requires is_parser<parser_type>
    constexpr auto operator&&(const parser_type & parser,Action&& pred) {
        return Where(
                parser,
                [action = std::forward<Action>(pred)](GetArg<0,Action> && val) {
                    return action(std::forward<GetArg<0,Action>>(val));
                }
        );
    }
}
#endif //LIGHT_PARSER_COMPILER_TIME_PARSER_H
