/**
 * @file parser.h
 * @brief This is a lightweight parsing library that uses a compositional structure to build parsers.
 *
 *
 *
 * @author pkuyo
 * @date 2025-02-06
 * @version 0.2.0
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

#include "traits.h"
#include "error_handler.h"


namespace pkuyo::parsers {

    // Abstract base class for `parser`. Used when:
    //  1. Only the name matters;
    //  2. Unified exception handling is needed;
    //  3. Storing `parser` instances with different `result_type`s uniformly.
    template<typename token_type>
    class _abstract_parser {
    public:
        typedef token_type token_t;
        virtual ~_abstract_parser() = default;

        // Gets the alias for `parser`.
        [[nodiscard]] std::string_view Name() const {return parser_name;}


    protected:
        _abstract_parser()  {}

        // Handles exception recovery. Invoked on `Parse` errors and may throw `parser_exception`.
        template<typename Iter>
        void error_handle_recovery(Iter &token_it, Iter token_end) const {
            auto last_it = token_it;
            token_it = (this->recovery ? this->recovery.value() : default_parser_error_handler<token_type>::recovery).recover(token_it,token_end);
            (this->error_handler ? this->error_handler.value() : default_parser_error_handler<token_type>::error_handler)(*this,
                                                                                                                          last_it==token_end ? std::nullopt : std::make_optional(*last_it));

        }

        std::string parser_name;

    private:
        std::optional<panic_mode_recovery<token_type>> recovery;
        std::optional<std::function<void(const _abstract_parser<token_type> &,const std::optional<token_t> &)>> error_handler;


    };


    // Generic base class for `parser`. Takes `token_type` as input and returns `std::optional<return_type>`.
    template<typename token_type,typename derived_type>
    class base_parser : public _abstract_parser<token_type> {
    public:
        // Sets an alias for `parser`.
        // (Usually called automatically in `parser_wrapper`.)
        derived_type& Name(const std::string_view & name) { this->parser_name = name; return static_cast<derived_type&>(*this); }

        // Sets the exception handler. Falls back to `parser_container`'s handler if none is provided.
        // (Usually called automatically in `parser_wrapper`.)
        template<typename FF>
        derived_type& OnError(FF && func) {this->error_handler = std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)>(func); return static_cast<derived_type&>(*this);}

        // Sets for panic mode recovery. Falls back to `parser_container`'s recovery if none is provided.
        // (Usually called automatically in `parser_wrapper`.)
        template<typename FF>
        derived_type& Recovery(FF && _recovery) { this->recovery = panic_mode_recovery<token_type>(_recovery); return static_cast<derived_type&>(*this);}

        // Predicts if this `parser` can correctly parse the input (single-character lookahead).
        template <typename Iter>
        bool Peek(Iter begin, Iter end) const {
            return static_cast<const derived_type&>(*this).peek_impl(begin, end);
        }
        // Parses the input sequence from `token_it` to `token_end`. Returns `std::nullopt` only when parsing fails.
        // (May throw exceptions.)
        template <typename Iter>
        auto Parse(Iter& begin, Iter end) const {
            return static_cast<const derived_type&>(*this).parse_impl(begin, end);
        }
        template <typename Container>
        auto Parse(const Container& container) const {
            auto it = container.begin();
            return static_cast<const derived_type&>(*this).parse_impl(it, container.end());
        }


    protected:
        base_parser() = default;


    };


}

namespace pkuyo::parsers {

    // A parser that only peek tokens, where child_parser->Peek() == false, returning 'nullptr'
    template<typename child_type>
    class parser_not : public base_parser<typename std::decay_t<child_type>::token_t,parser_not<child_type>>{
    public:

        explicit parser_not(child_type && _child_parser) : child_parser(std::forward<child_type>(_child_parser)) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(this->Peek(token_it,token_end))
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }
    public:

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return !child_parser.Peek(token_it,token_end);
        }


    private:
        child_type child_parser;
    };

    // A parser that only peek tokens, where child_parser->Peek() == true, returning 'nullptr'
    template<typename child_type>
    class parser_pred : public base_parser<typename std::decay_t<child_type>::token_t,parser_pred<child_type>>{
    public:

        explicit parser_pred(const child_type & _child_parser) : child_parser(_child_parser) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(this->Peek(token_it,token_end))
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return child_parser.Peek(token_it,token_end);
        }


    private:
        child_type  child_parser;
    };

    template<typename child_type>
    class parser_ignore : public base_parser<typename std::decay_t<child_type>::token_t,parser_ignore<child_type>>{
    public:

        explicit parser_ignore(const child_type & _child_parser) : child_parser(_child_parser) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            auto re = child_parser.Parse(token_it,token_end);
            if(re)
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return child_parser.Peek(token_it,token_end);
        }


    private:
        child_type  child_parser;
    };

    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'nullptr_t';
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_check : public base_parser<token_type,parser_check<token_type,cmp_type>>{
    public:

        explicit parser_check(cmp_type && _cmp_value) : cmp_value(std::forward<cmp_type>(_cmp_value)) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->Peek(token_it,token_end)){
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

        parser_until(const cmp_type & _cmp) : cmp(_cmp) {}

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
        cmp_type  cmp;
    };

    // A parser that matches tokens.
    //  If the match is successful, it returns a 'nullptr'
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename sequence_type>
    class parser_multi_check : public base_parser<token_type, parser_multi_check<token_type,sequence_type>> {
    public:


        parser_multi_check(sequence_type && _expected_seq): expected_seq(std::forward<sequence_type>(_expected_seq)) {}

        parser_multi_check(const sequence_type & _expected_seq) : expected_seq(_expected_seq) {}


        template <typename Iter>
        auto parse_impl(Iter& it, Iter end) const {
            auto last_it = it;
            for (const auto& expected : expected_seq) {
                if (it == end || *it != expected) {
                    it = last_it; // restore_it
                    this->error_handle_recovery(it, end);
                    return std::optional<nullptr_t>();
                }
                ++it;
            }
            return std::make_optional(nullptr);
        }

        template <typename Iter>
        bool peek_impl(Iter it, Iter end) const {
            return std::equal(expected_seq.begin(), expected_seq.end(), it,
                              [](const auto& a, const auto& b) { return a == b; });
        }


    private:
        sequence_type expected_seq;
    };

    // A parser that only matches the first token.
    //  Uses a 'std::function<bool(const token_type &)>' to determine whether the token matches.
    //  If the match is successful, it returns a 'std::unique_ptr<result_type>' constructed with the token as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type>
    class parser_ptr_with_func : public base_parser<token_type, parser_ptr_with_func<token_type,return_type>> {


        explicit parser_ptr_with_func(std::function<bool(const token_type&)> _cmp_func)
                : cmp_func(_cmp_func){}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->Peek(token_it,token_end)){
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
        std::function<bool(const token_type&)> cmp_func;
    };

    // A parser that only matches the first token.
    //  Uses a 'std::function<bool(const token_type &)>' to determine whether the token matches.
    //  If the match is successful, it returns a 'result_type' constructed with the token as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type>
    class parser_value_with_func : public base_parser<token_type, parser_value_with_func<token_type,return_type>> {

    public:

        explicit parser_value_with_func(std::function<bool(const token_type&)> _cmp_func)
                : cmp_func(_cmp_func){}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->Peek(token_it,token_end)){
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
        std::function<bool(const token_type&)> cmp_func;
    };


    // A parser that matches tokens.
    //  If the match is successful, it returns a 'std::unique<result_type>' constructed with the sequence as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type, typename sequence_type>
    class parser_multi_ptr : public base_parser<token_type, parser_multi_ptr<token_type,return_type,sequence_type>> {
    public:
        parser_multi_ptr(sequence_type && _expected_seq, std::function<std::unique_ptr<return_type>(sequence_type)> _constructor)
                : expected_seq(std::forward<sequence_type>(_expected_seq)), constructor(std::move(_constructor)) {}


        template <typename Iter>
        auto parse_impl(Iter& it, Iter end) const {
            auto last_it = it;
            for (const auto& expected : expected_seq) {
                if (it == end || *it != expected) {
                    it = last_it;
                    this->error_handle_recovery(it, end);
                    return std::optional<std::unique_ptr<return_type>>();
                }
                ++it;
            }
            return constructor(expected_seq);
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return std::equal(expected_seq.begin(), expected_seq.end(), token_it,
                              [](const auto& a, const auto& b) { return a == b; });
        }


    private:
        sequence_type expected_seq;
        std::function<std::unique_ptr<return_type>(sequence_type)> constructor;
    };

    // A parser that matches tokens.
    //  If the match is successful, it returns a 'result_type' constructed with the sequence as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type, typename sequence_type>
    class parser_multi_value : public base_parser<token_type, parser_multi_value<token_type,return_type,sequence_type>> {

        parser_multi_value(sequence_type && _expected_seq, std::function<return_type(sequence_type)> _constructor)
                : expected_seq(std::forward<sequence_type>(_expected_seq)), constructor(std::move(_constructor)) {}


        template <typename Iter>
        auto parse_impl(Iter& it, Iter end) const {
            auto last_it = it;
            for (const auto& expected : expected_seq) {
                if (it == end || *it != expected) {
                    it = last_it;
                    this->error_handle_recovery(it, end);
                    return std::optional<return_type>();

                }
                ++it;
            }
            return constructor(expected_seq);
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return std::equal(expected_seq.begin(), expected_seq.end(), token_it,
                              [](const auto& a, const auto& b) { return a == b; });
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
    class parser_ptr : public base_parser<token_type, parser_ptr<token_type,return_type,cmp_type>> {

    public:

        parser_ptr(cmp_type && _cmp_value,
                   std::function<std::unique_ptr<return_type>(const token_type &)> && constructor)
                : cmp_value(std::forward<cmp_type>(_cmp_value)), constructor(constructor) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->Peek(token_it,token_end)){
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
        std::function<std::unique_ptr<return_type>(const token_type &)> constructor;
        cmp_type cmp_value;
    };

    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'return_type‘;
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename return_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_value : public base_parser<token_type, parser_value<token_type,return_type,cmp_type>> {

    public:

        parser_value(cmp_type && _cmp_value,std::function<return_type(const token_type &)> && constructor)
                : cmp_value(std::forward<cmp_type>(_cmp_value)), constructor(constructor) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            if(!this->Peek(token_it,token_end)) {
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

        std::function<return_type(const token_type &)> constructor;
        cmp_type cmp_value;
    };


    // A parser that uses 'std::basic_string_view<token_type>' to compare 'token_type'.
    //  If the match is successful, it returns std::optional<std::basic_string_view<token_type>>;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    //  Typically used when the token_type is 'char' or 'wchar_t'.
    template<typename token_type>
    requires std::equality_comparable<token_type>
    class parser_str : public base_parser<token_type, parser_str<token_type>> {

    public:

        explicit parser_str(const std::basic_string_view<token_type> &_cmp_value)
                : cmp_value(_cmp_value) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            auto last_it = token_it;

            for(auto c_it : cmp_value) {
                if(*token_it != c_it) {
                    this->error_handle_recovery(token_it,token_end);
                    return std::optional<std::basic_string_view<token_type>>();
                }
                token_it++;
            }
            return std::make_optional(cmp_value);

        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return token_end != token_it && (*token_it) == *cmp_value.begin();
        }

    private:

        std::basic_string_view<token_type> cmp_value;
    };


    // A parser class that uses regular expressions for comparison.
    //This class is primarily used to parse and compare input strings using regular expressions.
    // During prediction, the length of the input string is controlled by the `pred_count` parameter, which defaults to 1.
    template<typename token_type,size_t pred_count>
    class parser_regex : public base_parser<token_type, parser_regex<token_type,pred_count>> {

    public:

        explicit parser_regex(const std::string _pattern)
                : pattern(_pattern),
                  regex(_pattern, std::regex_constants::ECMAScript | std::regex_constants::optimize)
        {
            // Compile the regular expression, throw an exception if it fails
            if (!regex.flags()) {
                throw std::runtime_error("Invalid regex pattern: " + pattern);
            }
        }

        template <typename Iter>
        bool peek_impl(Iter it, Iter end) const {
            if (it == end) return false;
            std::string candidate(it, (it+pred_count));
            return std::regex_search(candidate, regex, std::regex_constants::match_continuous);
        }


        template <typename Iter>
        auto parse_impl(Iter& it, Iter end) const {

            auto start = it;
            std::string candidate(start, end); // Create a string of the remaining input
            std::smatch match;

            // Perform the regex match (must match continuously from the beginning)
            if (std::regex_search(candidate, match, regex,
                                  std::regex_constants::match_continuous))
            {
                size_t length = match.length(); // Get the length of the match
                std::advance(it, length);       // Move the iterator forward
                return std::make_optional(std::string(start, it));  // Return the matched string
            } else {
                this->error_handle_recovery(it, end);
                return std::optional<std::string>();
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
    template<typename input_type_left, typename input_type_right>
    class parser_then : public base_parser<typename std::decay_t<input_type_left>::token_t, parser_then<input_type_left,input_type_right>> {

    public:


        parser_then(const input_type_left &parser_left,const input_type_right & parser_right)
                : children_parsers(std::make_pair(parser_left,parser_right)){}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using result_r = parser_result_t<input_type_right>;
            using result_l = parser_result_t<input_type_left>;

            using return_type = then_result_t<result_l,result_r>;
            auto last_it = token_it;
            auto left_re = children_parsers.first.Parse(token_it,token_end);

            // If the match fails, attempts to recover and restores the token index, then returns nullopt.
            if(left_re == std::nullopt) {
                this->error_handle_recovery(token_it,token_end);

                //Restores token_it.
                token_it = last_it;
                return std::optional<return_type>();
            }

            auto right_re = children_parsers.second.Parse(token_it,token_end);
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
            return children_parsers.first.Peek(token_it,token_end);
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

        parser_or(const input_type_l &left,const input_type_r & right) : children_parsers(std::make_pair(left,right)){}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            auto last_it = token_it;

            using result_t = filter_or_t<input_type_l,input_type_r>;
            if(!this->Peek(token_it,token_end)) {
                this->error_handle_recovery(token_it, token_end);
                return std::optional<result_t>();
            }
            else if(children_parsers.first.Peek(token_it,token_end)) {
                auto re = children_parsers.first.Parse(token_it,token_end);
                if(!re) {
                    this->error_handle_recovery(token_it, token_end);
                    token_it = last_it;
                    return std::optional<result_t>();
                }
                else return std::optional<result_t>(std::move(re.value()));
            }
            else {
                auto re = children_parsers.second.Parse(token_it,token_end);
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
            return children_parsers.first.Peek(token_it,token_end) || children_parsers.second.Peek(token_it,token_end);
        }

    private:

    private:
        std::pair<input_type_l,input_type_r> children_parsers;

    };

    // Match the child parser 0 or more times, returning std::vector<child_return_type> (std::basic_string<child_return_type> for char or wchar_t).
    template<typename child_type>
    class parser_many : public base_parser<typename std::decay_t<child_type>::token_t,parser_many<child_type>> {

    public:
        explicit parser_many(const child_type & child): child_parser(child) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using child_return_type = decltype(child_parser.Parse(token_it,token_end))::value_type;
            if constexpr ( std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>) {
                std::basic_string<child_return_type> results;
                while (token_it != token_end && child_parser.Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.Parse(token_it, token_end);
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
                while (token_it != token_end && child_parser.Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.Parse(token_it, token_end);
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
                while (token_it != token_end && child_parser.Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.Parse(token_it, token_end);
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

        explicit parser_more(const child_type &  child): child_parser(child) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using child_return_type = decltype(child_parser.Parse(token_it,token_end))::value_type;
            if constexpr ( std::is_same_v<child_return_type, char> ||  std::is_same_v<child_return_type, wchar_t>) {
                std::basic_string<child_return_type> results;
                auto first_result = child_parser.Parse(token_it, token_end);
                if (!first_result) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::optional<std::basic_string<child_return_type>>();
                }
                results.push_back(*first_result);

                while (token_it != token_end && child_parser.Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.Parse(token_it, token_end);
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
                auto first_result = child_parser.Parse(token_it, token_end);
                if (!first_result) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::optional<nullptr_t>();
                }

                while (token_it != token_end && child_parser.Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.Parse(token_it, token_end);
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
                auto first_result = child_parser.Parse(token_it, token_end);
                if (!first_result) {
                    this->error_handle_recovery(token_it, token_end);
                    return std::optional<std::vector<child_return_type>>();
                }
                results.push_back(std::move(*first_result));

                while (token_it != token_end && child_parser.Peek(token_it, token_end)) {
                    auto last_it = token_it;
                    auto result = child_parser.Parse(token_it, token_end);
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
            return child_parser.Peek(it, end);
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

        explicit parser_optional(const child_type & child): child_parser(child) {}
        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using return_type = decltype(child_parser.Parse(token_it,token_end))::value_type;
            std::optional<return_type> result(std::nullopt);
            if(child_parser.Peek(token_it,token_end)) {
                auto re = child_parser.Parse(token_it, token_end);
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
            return factory().Peek(begin, end);
        }

        template <typename Iter>
        auto parse_impl(Iter& begin, Iter end) const {
            return factory().Parse(begin, end);
        }
    private:
        real_type& factory() const{
            static std::unique_ptr<real_type> real;
            if(!real)
                real = std::move(std::make_unique<real_type>());
            return *real;
        }
    };


    // Convert the parsing result to the target type's parser.
    // No error recovery.
    template<typename child_type, typename FF>
    class parser_map : public base_parser<typename std::decay_t<child_type>::token_t, parser_map<child_type,FF>> {
    public:

        using mapper_t = FF;

        parser_map(const child_type & source, mapper_t mapper)
                : source(source), mapper(mapper) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using SourceType = parser_result_t<child_type>;
            using TargetType = decltype(std::declval<mapper_t>()(std::declval<SourceType>()));

            auto source_result = source.Parse(token_it, token_end);
            if (source_result == std::nullopt) {
                this->error_handle_recovery(token_it,token_end);
                return std::optional<TargetType>();
            }
            return std::make_optional(std::move(mapper(std::move(source_result.value()))));
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return source.Peek(token_it, token_end);
        }



    private:
        child_type source;
        mapper_t mapper;
    };

    template<typename child_type,typename FF>
    class parser_where : public base_parser<typename std::decay_t<child_type>::token_t, parser_where<child_type,FF>> {
    public:
        using Predicate = FF;

        parser_where(const child_type &  parser, Predicate pred)
                : child_parser(parser), predicate(pred) {}

        template <typename Iter>
        auto parse_impl(Iter& token_it, Iter token_end) const {
            using ReturnType = parser_result_t<child_type>;

            auto result = child_parser.Parse(token_it, token_end);
            if(!result || !predicate(*result)) {
                return std::optional<ReturnType>();
            }
            return result;
        }

        template <typename Iter>
        bool peek_impl(Iter token_it, Iter token_end) const {
            return child_parser.Peek(token_it, token_end);
        }
    private:
        child_type child_parser;
        Predicate predicate;
    };
}


namespace pkuyo::parsers {

    template <typename T>
    concept is_parser = std::is_base_of_v<_abstract_parser<typename std::remove_reference_t<T>::token_t>,std::remove_reference_t<T>>;

    template<typename token_type, typename cmp_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    auto Check(cmp_type &&cmp) {
        return parser_check<token_type, cmp_type>(std::forward<cmp_type>(cmp));
    }

    template<typename token_type, typename cmp_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    auto Until(cmp_type &&cmp) {
        return parser_until<token_type, cmp_type>(std::forward<cmp_type>(cmp));
    }

    template<typename token_type, typename seq_type>
    auto SeqCheck(seq_type &&cmp) {
        return parser_multi_check<token_type, seq_type>(std::forward<seq_type>(cmp));
    }

    template<typename token_type, typename seq_type>
    auto SeqCheck(const seq_type &cmp) {
        return parser_multi_check<token_type, seq_type>(cmp);
    }

    auto SeqCheck(const char *str) {
        return SeqCheck<char>(std::string_view(str));
    }

    auto SeqCheck(const wchar_t *str) {
        return SeqCheck<wchar_t>(std::wstring_view(str));
    }


    template<typename token_type>
    auto Str(const std::basic_string_view<token_type> & str) {
        return parser_str(str);
    }

    template<typename token_type,size_t pred_count = 1>
    auto Regex(const std::string & pattern) {
        return parser_regex<token_type,pred_count>(pattern);
    }


    template<typename token_type = char,size_t pred_count = 1>
    auto Regex(const char* pattern) {
        return parser_regex<token_type,pred_count>(std::string(pattern));
    }

    template<typename token_type, typename cmp_type = token_type,typename return_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    auto SingleValue(cmp_type && cmp_value) {
        auto constructor = [](const token_type& s) { return return_type(s); };
        return parser_value<token_type,return_type,cmp_type>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    auto SinglePtr(cmp_type && cmp_value) {
        auto constructor = [](const token_type& s) { return std::make_unique<return_type>(s); };
        return parser_ptr<token_type,return_type,cmp_type>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type,typename FF>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    auto SingleValue(cmp_type && cmp_value, FF constructor) {
        return parser_value<token_type,return_type,cmp_type>(std::forward<cmp_type>(cmp_value),constructor);
    }

    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type,typename FF>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    auto SinglePtr(cmp_type && cmp_value,FF constructor) {
        return parser_ptr<token_type,return_type,cmp_type>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type,typename return_type = token_type, typename FF>
    requires is_convertible_to_function_v<FF,std::function<bool(const token_type &)>>
    auto SingleValue(FF compare_func) {
        return parser_value_with_func<token_type, return_type>(compare_func);
    }

    template<typename token_type,typename return_type, typename FF>
    requires is_convertible_to_function_v<FF,std::function<bool(const token_type &)>>
    auto SinglePtr(FF compare_func) {
        return parser_ptr_with_func<token_type, return_type>(compare_func);
    }


    template<typename token_type, typename cmp_type, typename return_type>
    auto SeqValue(const cmp_type & cmp_value) {
        auto constructor = [](const token_type& s) { return return_type(s); };
        return parser_multi_value<token_type,return_type,cmp_type>(cmp_value,constructor);
    }

    template<typename token_type, typename cmp_type, typename return_type>
    auto SeqPtr(const cmp_type & cmp_value) {
        auto constructor = [](const token_type& s) { return std::make_unique<return_type>(s); };
        return parser_multi_ptr<token_type,return_type,cmp_type>(cmp_value,constructor);
    }


    template<typename token_type, typename cmp_type, typename return_type,typename FF>
    auto SeqValue(const cmp_type & cmp_value, FF&& constructor) {
        return parser_multi_value<token_type,return_type,cmp_type>(cmp_value,constructor);
    }

    template<typename token_type, typename cmp_type, typename return_type,typename FF>
    auto SeqPtr(const cmp_type & cmp_value,FF&& constructor) {
        return parser_multi_ptr<token_type,return_type,cmp_type>(cmp_value,constructor);
    }

    template<typename child_type>
    requires is_parser<child_type>
    auto Not(child_type && parser) {
        return parser_not(std::forward<std::remove_reference_t<child_type>>(parser));
    }

    template<typename child_type>
    requires is_parser<child_type>
    auto Pred(child_type && parser) {
        return parser_pred(std::forward<std::remove_reference_t<child_type>>(parser));
    }


    template <typename token_type,typename real_parser>
    //requires is_parser<real_parser>
    auto Lazy() {
        return parser_lazy<token_type,real_parser>();
    }

    template<typename child_type>
    requires is_parser<child_type>
    auto Optional(child_type && child) {
        return parser_optional<std::remove_reference_t<child_type>>(std::forward<child_type>(child));
    }

    template<typename child_type>
    requires is_parser<child_type>
    auto Many(child_type && child) {
        return parser_many<std::remove_reference_t<child_type>>(std::forward<child_type>(child));
    }

    template<typename child_type>
    requires is_parser<child_type>
    auto More(child_type && child) {
        return parser_more<std::remove_reference_t<child_type>>(std::forward<child_type>(child));
    }



    template<typename child_type, typename FF>
    requires is_parser<child_type>
    auto Map(child_type && child, FF && mapper) {
        return parser_map<std::remove_reference_t<child_type>, std::remove_reference_t<GetFunc<FF>>>
        (std::forward<child_type>(child),std::forward<FF>(mapper));
    }

    template<typename child_type, typename FF>
    requires is_parser<child_type>
    auto Where(child_type && child, FF && mapper) {
        return parser_where<std::remove_reference_t<child_type>,std::remove_reference_t<GetFunc<FF>>>
        (std::forward<child_type>(child),std::forward<FF>(mapper));
    }

    template<typename l,typename r>
    requires std::is_same_v<typename std::remove_reference_t<l>::token_t,typename std::remove_reference_t<r>::token_t> && is_parser<l> && is_parser<r>
    auto Or(l&& left, r&& right) {
        return parser_or<std::remove_reference_t<l>,std::remove_reference_t<r>>(std::forward<l>(left),std::forward<r>(right));
    }

    template<typename l,typename r>
    requires std::is_same_v<typename std::remove_reference_t<l>::token_t,typename std::remove_reference_t<r>::token_t> && is_parser<l> && is_parser<r>
    auto Then(l&& left, r&& right) {
        return parser_then<std::remove_reference_t<l>,std::remove_reference_t<r>>(std::forward<l>(left),std::forward<r>(right));
    }

    template<typename child_type>
    requires is_parser<child_type>
    auto Ignore(child_type && child) {
        return parser_ignore<std::remove_reference_t<child_type>>(child);
    }
}


namespace pkuyo::parsers {




    // Creates a parser_or.
    template<typename parser_type_l, typename parser_type_r>
    requires std::is_same_v<typename std::decay_t<parser_type_l>::token_t,typename std::decay_t<parser_type_r>::token_t> &&
             is_parser<parser_type_l> &&
             is_parser<parser_type_r>
    auto operator|(parser_type_l&& left,parser_type_r&& right) {
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(left),std::forward<parser_type_r>(right));
    }

    template<typename parser_type_l, typename cmp_type>
    requires is_parser<parser_type_l> &&  std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, cmp_type>
    auto operator|(parser_type_l&& left,cmp_type&& right) {
        auto r = Check<typename std::decay_t<parser_type_l>::token_t,cmp_type>(std::forward<cmp_type>(right));
        using parser_type_r = decltype(r);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(left),std::forward<parser_type_r>(r));
    }

    template<typename cmp_type, typename parser_type_r>
    requires is_parser<parser_type_r> &&  std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, cmp_type>
    auto operator|(cmp_type&& left,parser_type_r&& right) {
        auto l = Check<typename std::decay_t<parser_type_r>::token_t,cmp_type>(std::forward<cmp_type>(left));
        using parser_type_l = decltype(l);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(l),std::forward<parser_type_r>(right));
    }

    template<typename parser_type_l,typename chr>
    requires is_parser<parser_type_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, chr>
    auto operator|(parser_type_l && left,const chr* str) {
        auto check_parser = SeqCheck<typename std::decay_t<parser_type_l>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_r = decltype(check_parser);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(left),std::forward<parser_type_r>(check_parser));
    }
    template<typename parser_type_l,typename chr>
    requires is_parser<parser_type_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, std::basic_string_view<chr>>
    auto operator|(parser_type_l && left,const chr* str) {
        auto check_parser = Check<typename std::decay_t<parser_type_l>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_r = decltype(check_parser);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(left),std::forward<parser_type_r>(check_parser));
    }

    template<typename chr,typename parser_type_r>
    requires is_parser<parser_type_r> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, chr>
    auto operator|(const chr* str,parser_type_r && right) {
        auto check_parser = SeqCheck<typename std::decay_t<parser_type_r>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_l = decltype(check_parser);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(check_parser),std::forward<parser_type_r>(right));
    }
    template<typename chr,typename parser_type_r>
    requires is_parser<parser_type_r>  && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, std::basic_string_view<chr>>
    auto operator|(const chr* str,parser_type_r && right) {
        auto check_parser = Check<typename std::decay_t<parser_type_r>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_l = decltype(check_parser);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(check_parser),std::forward<parser_type_r>(right));
    }


    // Creates a parser_then
    template<typename parser_type_l, typename cmp_type>
    requires is_parser<parser_type_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, cmp_type>
    auto operator>>(parser_type_l&& left,cmp_type&& right) {
        using token_t = std::decay_t<parser_type_l>::token_t;

        auto r = Check<token_t,cmp_type>(std::forward<cmp_type>(right));
        return Then<parser_type_l, decltype(r)>
                (std::forward<parser_type_l>(left),std::move(r));
    }

    template<typename cmp_type, typename parser_type_r>
    requires is_parser<parser_type_r> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, cmp_type>
    auto operator>>(cmp_type&& left,parser_type_r&& right) {
        using token_t = std::decay_t<parser_type_r>::token_t;

        auto l = Check<token_t,cmp_type>(std::forward<cmp_type>(left));
        return Then<decltype(l),parser_type_r>
                (std::move(l),std::forward<parser_type_r>(right));
    }

    template<typename parser_type_l, typename parser_type_r>
    requires is_parser<parser_type_l> &&  is_parser<parser_type_r>
    auto operator>>(parser_type_l&& left,parser_type_r&& right) {
        return Then<parser_type_l,parser_type_r>
                (std::forward<parser_type_l>(left),std::forward<parser_type_r>(right));
    }

    template<typename chr,typename parser_r>
    requires is_parser<parser_r> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_r>::token_t, chr>
    auto operator>>(const chr* str,parser_r && right) {
        auto check_parser = SeqCheck<typename std::decay_t<parser_r>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_l = decltype(check_parser);
        return Then<parser_type_l,parser_r>
                (std::forward<parser_type_l>(check_parser),std::forward<parser_r>(right));
    }
    template<typename chr,typename parser_r>
    requires is_parser<parser_r> && (std::__weakly_equality_comparable_with<typename std::decay_t<parser_r>::token_t, std::basic_string_view<chr>>)
    auto operator>>(const chr* str,parser_r && right) {
        auto check_parser = Check<typename std::decay_t<parser_r>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_l = decltype(check_parser);
        return Then<parser_type_l,parser_r>
                (std::forward<parser_type_l>(check_parser),std::forward<parser_r>(right));
    }

    template<typename parser_l,typename chr>
    requires is_parser<parser_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_l>::token_t, chr>
    auto operator>>(parser_l && left,const chr* str) {
        auto check_parser = SeqCheck<typename std::decay_t<parser_l>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_r = decltype(check_parser);
        return Then<parser_l,parser_type_r>
                (std::forward<parser_l>(left),std::forward<parser_type_r>(check_parser));
    }
    template<typename parser_l,typename chr>
    requires is_parser<parser_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_l>::token_t, std::basic_string_view<chr>>
    auto operator>>(parser_l && left,const chr* str) {
        auto check_parser = Check<typename std::decay_t<parser_l>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_r = decltype(check_parser);
        return Then<parser_l,parser_type_r>
                (std::forward<parser_l>(left),std::forward<parser_type_r>(check_parser));
    }
    // Creates a parser_not that only pred tokens using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    auto operator!(parser_type && parser) {return Not(std::forward<parser_type>(parser));}

    // Creates a parser_many using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    auto operator*(parser_type && parser) {return Many(std::forward<parser_type>(parser));}

    // Creates a parser_more using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    auto operator+(parser_type && parser) {return More(std::forward<parser_type>(parser));}

    // Creates a parser that only pred tokens using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    auto operator&(parser_type && parser) {return Pred(std::forward<parser_type>(parser));}

    // Creates a optional parser that
    template<typename parser_type>
    requires is_parser<parser_type>
    auto operator~(parser_type && parser) {return Optional(std::forward<parser_type>(parser));}

    // Creates a parser that ignord original output and return nullptr using parser.
    template<typename parser_type>
    requires is_parser<parser_type>
    auto operator-(const parser_type & parser) {return Ignore(parser);}

    // Overloads '>>=' for semantic action injection, returning the injected function's result.
    template <typename parser_type,typename Mapper>
    requires is_parser<parser_type>
    auto operator>>=(parser_type && parser, Mapper&& mapper) {
        return Map(std::forward<parser_type>(parser),mapper);
    }

    // Overloads '<<=' for semantic action injection, retaining the original return value.
    template <typename parser_type,typename Action>
    requires is_parser<parser_type>
    auto operator<<=(const parser_type & parser,Action&& action) {
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
    auto operator&&(const parser_type & parser,Action&& pred) {
        return Where(
                parser,
                [action = std::forward<Action>(pred)](GetArg<0,Action> && val) {
                    return action(std::forward<GetArg<0,Action>>(val));
                }
        );
    }


}

#endif //LIGHT_PARSER_PARSER_H
