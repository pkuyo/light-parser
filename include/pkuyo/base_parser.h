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
        template<typename Stream>
        void error_handle_recovery(Stream & stream) const {
            if (no_error)
                return;
            if (!error_handler)     parser_error_handler<token_type>::error_handler(*this,stream.Eof() ?
            std::nullopt : std::make_optional(stream.Peek()),stream.Value(),stream.Pos(),stream.Name());
            else                    error_handler(*this,stream.Eof() ? std::nullopt : std::make_optional(stream.Peek()),
                                                  stream.Value(),stream.Pos(),stream.Name());

        }



        char parser_name[20]{};

    protected:

        parser_error_handler<token_type>::error_handler_t error_handler = nullptr;
        bool no_error = false;

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
        constexpr derived_type& OnError(parser_error_handler<token_type>::error_handler_t  error_handler)
        {this->error_handler = error_handler; return static_cast<derived_type&>(*this);}

        //Disable the exception handler
        constexpr derived_type& NoError() {
            NoError_Internal();
            return static_cast<derived_type&>(*this);
        }
        // Predicts if this `parser` can correctly parse the input (single-character lookahead).
        template <typename Stream>
        auto Peek(Stream& stream) const {
            return static_cast<const derived_type&>(*this).peek_impl(stream);
        }
        // Parses the input sequence from `token_it` to `token_end`. Returns `std::nullopt` only when parsing fails.
        // (May throw exceptions.)
        template <typename Stream>
        auto Parse(Stream& stream) const {
            this->Reset();
            nullptr_t local_state= nullptr;
            nullptr_t global_state = nullptr;
            return static_cast<const derived_type&>(*this).parse_impl(stream,global_state,local_state);
        }

        template <typename Stream,typename GlobalState>
        auto Parse(Stream& stream,GlobalState & global_state) const {
            this->Reset();
            nullptr_t t= nullptr;
            return static_cast<const derived_type&>(*this).parse_impl(stream,global_state,t);
        }

        template <typename Stream,typename GlobalState,typename State>
        auto Parse(Stream& stream,GlobalState & global_state, State& state) const {
            auto re =  static_cast<const derived_type&>(*this).parse_impl(stream,global_state,state);
            return re;
        }

        void reset_impl() const { }

        void no_error_impl() {}

    protected:

        void NoError_Internal() {
            this->no_error = true;
        }

        void Reset() const {
            static_cast<const derived_type&>(*this).reset_impl();
        }



        constexpr base_parser() = default;
    };

    template <typename T>
    concept is_parser = std::is_base_of_v<_abstract_parser<typename std::remove_reference_t<T>::token_t>,std::remove_reference_t<T>>;


}
#endif //LIGHT_PARSER_BASE_PARSER_H
