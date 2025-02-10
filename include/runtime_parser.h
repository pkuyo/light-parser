// runtime_parser.h
/**
 * @file runtime_parser.h
 * @brief Runtime parser implementations, supporting dynamic pattern matching.
 * @author pkuyo
 * @date 2025-02-07
 * @copyright Copyright (c) 2025 pkuyo. All rights reserved.
 *
 * Includes:
 * - Regular expression parser parser_regex
 * - Dynamic sequence checking parser parser_multi_check
 * - Runtime sequence construction parsers (VALUE/PTR)
 */


#ifndef LIGHT_PARSER_RUNTIME_PARSER_H
#define LIGHT_PARSER_RUNTIME_PARSER_H
#include "base_parser.h"
#include "compile_time_parser.h"
#include <regex>

namespace pkuyo::parsers {
    // A parser that matches tokens.
    //  If the match is successful, it returns a 'nullptr'
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename sequence_type>
    class parser_multi_check : public base_parser<token_type, parser_multi_check<token_type,sequence_type>> {
    public:


        parser_multi_check(sequence_type && _expected_seq): expected_seq(std::forward<sequence_type>(_expected_seq)) {}

        parser_multi_check(const sequence_type & _expected_seq) : expected_seq(_expected_seq) {}


        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&)  const {
            if(!this->peek_impl(stream)) {
                this->error_handle_recovery(stream);
                return std::optional<nullptr_t>();
            }
            stream.Seek(expected_seq.size());
            return std::make_optional(nullptr);
        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            int index = 0;
            for(const auto & it : expected_seq) {
                if(stream.Eof(index) || stream.Peek(index) != it)
                    return false;
                index++;
            }
            return true;

        }


    private:
        sequence_type expected_seq;
    };


    // A parser class that uses regular expressions for comparison.
    //This class is primarily used to parse and compare input strings using regular expressions.
    // During prediction, the length of the input string is controlled by the `pred_count` parameter, which defaults to 1.
//    template<typename token_type,size_t pred_count>
//    class parser_regex : public base_parser<token_type, parser_regex<token_type,pred_count>> {
//
//    public:
//
//        explicit parser_regex(const std::string _pattern)
//                : pattern(_pattern),
//                  regex(_pattern, std::regex_constants::ECMAScript | std::regex_constants::optimize)
//        {
//            // Compile the regular expression, throw an exception if it fails
//            if (!regex.flags()) {
//                throw std::runtime_error("Invalid regex pattern: " + pattern);
//            }
//        }
//
//        template<typename Stream>
//        bool peek_impl(Iter it, Iter end) const {
//            if (it == end) return false;
//            std::match_results<Iter> match;
//            return std::regex_search(it, std::next(it, pred_count), match, regex);
//        }
//
//
//        template<typename Stream>
//        auto parse_impl(Stream & stream) const {
//
//            auto start = it;
//            std::string candidate(start, end); // Create a string of the remaining input
//            std::smatch match;
//
//            // Perform the regex match (must match continuously from the beginning)
//            if (std::regex_search(candidate, match, regex,
//                                  std::regex_constants::match_continuous))
//            {
//                size_t length = match.length(); // Get the length of the match
//                std::advance(it, length);       // Move the iterator forward
//                return std::make_optional(std::string(start, it));  // Return the matched string
//            } else {
//                this->error_handle_recovery(stream);
//                return std::optional<std::string>();
//            }
//        }
//
//    private:
//        std::string pattern;
//        std::regex regex;
//    };

    // A parser that matches tokens.
    //  If the match is successful, it returns a 'result_type' constructed with the sequence as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type, typename sequence_type, typename FF>
    class parser_multi_value : public base_parser<token_type, parser_multi_value<token_type,return_type,sequence_type, FF>> {

        parser_multi_value(sequence_type && _expected_seq, std::function<return_type(sequence_type)> _constructor)
                : expected_seq(std::forward<sequence_type>(_expected_seq)), constructor(std::move(_constructor)) {}


        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&)  const {
            if(!this->peek_impl(stream)) {
                this->error_handle_recovery(stream);
                return std::optional<return_type>();
            }
            stream.Seek(expected_seq.size());
            return constructor(expected_seq);
        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            int index = 0;
            for(const auto & it : expected_seq) {
                if(stream.Eof(index) || stream.Peek(index) != it)
                    return false;
                index++;
            }
            return true;
        }

    private:
        sequence_type expected_seq;
        FF constructor;
    };


}



namespace pkuyo::parsers {

    template<typename token_type, typename seq_type>
    auto SeqCheck(seq_type &&cmp) {
        return parser_multi_check<token_type, seq_type>(std::forward<seq_type>(cmp));
    }

    template<typename token_type, typename seq_type>
    auto SeqCheck(const seq_type &cmp) {
        return parser_multi_check<token_type, seq_type>(cmp);
    }

//    auto SeqCheck(const char *str) {
//        return SeqCheck<char>(std::string_view(str));
//    }
//
//    auto SeqCheck(const wchar_t *str) {
//        return SeqCheck<wchar_t>(std::wstring_view(str));
//    }
//
//
//    template<typename token_type,size_t pred_count = 1>
//    auto Regex(const std::string & pattern) {
//        return parser_regex<token_type,pred_count>(pattern);
//    }
//
//
//    template<typename token_type = char,size_t pred_count = 1>
//    auto Regex(const char* pattern) {
//        return parser_regex<token_type,pred_count>(std::string(pattern));
//    }

    template<typename token_type, typename cmp_type, typename return_type>
    auto SeqValue(const cmp_type & cmp_value) {
        auto constructor = [](const cmp_type& s) { return return_type(s); };
        return parser_multi_value<token_type,return_type,cmp_type, decltype(constructor)>(cmp_value,constructor);
    }

    template<typename token_type, typename cmp_type, typename return_type>
    auto SeqPtr(const cmp_type & cmp_value) {
        auto constructor = [](const cmp_type& s) { return std::make_unique<return_type>(s); };
        return parser_multi_ptr<token_type,std::unique_ptr<return_type>,cmp_type, decltype(constructor)>(cmp_value,constructor);
    }


    template<typename token_type, typename cmp_type, typename return_type,typename FF>
    auto SeqValue(const cmp_type & cmp_value, FF&& constructor) {
        return parser_multi_value<token_type,return_type,cmp_type,FF>(cmp_value,constructor);
    }

    template<typename token_type, typename cmp_type, typename return_type,typename FF>
    auto SeqPtr(const cmp_type & cmp_value,FF&& constructor) {
        return parser_multi_ptr<token_type,return_type,cmp_type,FF>(cmp_value,constructor);
    }


    template<typename parser_type_l,typename chr>
    requires is_parser<parser_type_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_l>::token_t, std::basic_string_view<chr>>
    constexpr auto operator|(parser_type_l && left,const chr* str) {
        auto check_parser = Check<typename std::decay_t<parser_type_l>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_r = decltype(check_parser);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(left),std::forward<parser_type_r>(check_parser));
    }

    template<typename parser_l,typename chr>
    requires is_parser<parser_l> && std::__weakly_equality_comparable_with<typename std::decay_t<parser_l>::token_t, std::basic_string_view<chr>>
    constexpr auto operator>>(parser_l && left,const chr* str) {
        auto check_parser = Check<typename std::decay_t<parser_l>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_r = decltype(check_parser);
        return Then<parser_l,parser_type_r>
                (std::forward<parser_l>(left),std::forward<parser_type_r>(check_parser));
    }

    template<typename chr,typename parser_r>
    requires is_parser<parser_r> && (std::__weakly_equality_comparable_with<typename std::decay_t<parser_r>::token_t, std::basic_string_view<chr>>)
    auto operator>>(const chr* str,parser_r && right) {
        auto check_parser = Check<typename std::decay_t<parser_r>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_l = decltype(check_parser);
        return Then<parser_type_l,parser_r>
                (std::forward<parser_type_l>(check_parser),std::forward<parser_r>(right));
    }


    template<typename chr,typename parser_type_r>
    requires is_parser<parser_type_r>  && std::__weakly_equality_comparable_with<typename std::decay_t<parser_type_r>::token_t, std::basic_string_view<chr>>
    auto operator|(const chr* str,parser_type_r && right) {
        auto check_parser = Check<typename std::decay_t<parser_type_r>::token_t,std::basic_string_view<chr>>(std::basic_string_view<chr>(str));
        using parser_type_l = decltype(check_parser);
        return Or<parser_type_l,parser_type_r>(std::forward<parser_type_l>(check_parser),std::forward<parser_type_r>(right));
    }
}


#endif //LIGHT_PARSER_RUNTIME_PARSER_H
