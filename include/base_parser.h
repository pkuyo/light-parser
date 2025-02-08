// base_parser.h
/**
 * @file base_parser.h
 * @brief Base class definitions for parsers, providing core abstractions and foundational implementations for combinatory parsers.
 * @author pkuyo
 * @date 2025-02-07
 * @copyright Copyright (c) 2025 pkuyo. All rights reserved.
 *
 * Includes:
 * - Abstract parser base class _abstract_parser
 * - Generic parser template base_parser
 * - Parser naming wrapper named_parser
 * - Parser concept definition is_parser
 */

#ifndef LIGHT_PARSER_BASE_PARSER_H
#define LIGHT_PARSER_BASE_PARSER_H

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

        // Gets the alias for `parser`.
        [[nodiscard]] std::string_view Name() const {return parser_name;}


    protected:
        constexpr _abstract_parser() = default;

        // Handles exception recovery. Invoked on `Parse` errors and may throw `parser_exception`.
        template<typename Iter>
        void error_handle_recovery(Iter &token_it, Iter token_end) const {
            auto last_it = token_it;
            if (!recovery_func)     token_it = parser_error_handler<token_type>::recovery.recover(token_it,token_end);
            else                    token_it = parser_error_handler<token_type>::recovery.recover(token_it,token_end,recovery_func);

            if (!error_handler)     parser_error_handler<token_type>::error_handler(*this,last_it==token_end ? std::nullopt : std::make_optional(*last_it));
            else                    error_handler(*this,last_it==token_end ? std::nullopt : std::make_optional(*last_it));

        }



        char parser_name[20]{};

        template<typename parser_type,size_t size>
        friend class named_parser;

    protected:

        bool(*recovery_func)(const token_type&) = nullptr;
        void(*error_handler)(const _abstract_parser<token_type> &,const std::optional<token_t> &) = nullptr;


    };


    // Generic base class for `parser`. Takes `token_type` as input and returns `std::optional<return_type>`.
    template<typename token_type,typename derived_type>
    class base_parser : public _abstract_parser<token_type> {
    public:
        // Sets an alias for `parser`.
        // (Usually called automatically in `parser_wrapper`.)
        constexpr derived_type& Name(const std::string_view & name)
        { std::copy_n(name.data(),name.size(),this->parser_name); return static_cast<derived_type&>(*this); }

        // Sets the exception handler. Falls back to `parser_container`'s handler if none is provided.
        // (Usually called automatically in `parser_wrapper`.)
        constexpr derived_type& OnError( void(*error_handler)(const _abstract_parser<token_type> &,const std::optional<token_type> &))
        {this->error_handler = error_handler; return static_cast<derived_type&>(*this);}

        // Sets for panic mode recovery. Falls back to `parser_container`'s recovery if none is provided.
        // (Usually called automatically in `parser_wrapper`.)
        constexpr derived_type& OnRecovery(bool(*recovery_func)(const token_type&))
        { this->recovery_func = recovery_func; return static_cast<derived_type&>(*this);}

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
        constexpr base_parser() = default;
    };

    template <typename T>
    concept is_parser = std::is_base_of_v<_abstract_parser<typename std::remove_reference_t<T>::token_t>,std::remove_reference_t<T>>;


}
#endif //LIGHT_PARSER_BASE_PARSER_H
