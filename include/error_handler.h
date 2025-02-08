// error_handler.h
/**
 * @file error_handler.h
 * @brief Implementation of parsing error handling mechanisms, supporting panic mode error recovery.
 * @author pkuyo
 * @date 2025-02-07
 * @copyright Copyright (c) 2025 pkuyo. All rights reserved.
 *
 * Includes:
 * - Standard parsing exception class parser_exception
 * - Error recovery strategy panic_mode_recovery
 * - Global error handling configuration template
 * - Default error handling implementation
 */

#ifndef LIGHT_PARSER_ERROR_HANDLER_H
#define LIGHT_PARSER_ERROR_HANDLER_H

#include<string>
#include<functional>
#include<format>

#include"traits.h"

namespace pkuyo::parsers {


    // Standard exception class for error handling in the parser
    class parser_exception : public std::runtime_error {

    public:

        parser_exception(const std::string_view error_part, const std::string_view & parser_name,
                         const std::string_view & position, const std::string_view & stream_name)
                : std::runtime_error(std::format("parser exception in token {}. At parser:{}, pos:{}. Stream: {}",
                                                 parser_name,error_part,position,stream_name)),
                error_part(error_part),
                parser_name(parser_name),
                position(position),
                stream_name(stream_name)
        {}

        std::string_view error_part;
        std::string_view parser_name;
        std::string_view position;
        std::string_view stream_name;
    };

    // Used to reset the position point when the parser fails, in order to achieve error recovery functionality.
    template<typename token_type>
    class panic_mode_recovery {
    public:

        typedef bool(* sync_pred_t)(const token_type&);
        using sync_pred_func = std::function<std::remove_pointer_t<sync_pred_t>>;


        // Passes in a function for resetting the position point, with the function type being bool(const token_type&).
        explicit panic_mode_recovery(sync_pred_func pred) : sync_check(pred) {}

        // Checks from iterator `it` using `sync_check` for a synchronization point. Returns the sync point if found, otherwise `end`.
        template<typename Stream>
        void recover(Stream & stream) const {
            while(!stream.Eof()) {
                if(sync_check(stream.Peek()))
                    break;
                stream.Seek(1);
            }
        }
        template<typename Stream>
        void recover(Stream & stream,bool(*func)(const token_type&)) const {
            while(!stream.Eof()) {
                if(func(stream.Peek()))
                    break;
                stream.Seek(1);
            }
        }
    private:
        sync_pred_func sync_check;
    };

    template<typename T>
    class _abstract_parser;




    template<typename token_type>
    class parser_error_handler {

        template<typename T>
        friend class _abstract_parser;

    public:
        typedef void(*error_handler_t)( const _abstract_parser<token_type> & parser,
                                        const std::optional<token_type> & token,
                                        const std::string & token_value,
                                        const std::string & token_pos,
                                        const std::string_view & stream_name);

    public:
        template<typename FF>
        static void DefaultRecovery(FF && _recovery) {
            parser_error_handler<token_type>::recovery = std::move(panic_mode_recovery<token_type>(_recovery));
        }

        template<typename FF>
        static void DefaultOnError(FF && _func)  {
            parser_error_handler<token_type>::error_handler =
                    std::function<std::remove_pointer_t<error_handler_t>>(std::forward<FF>(_func));
        }
    private:
        static panic_mode_recovery<token_type> recovery;
        static std::function<std::remove_pointer_t<error_handler_t>> error_handler;
    };

    template<typename token_type>
    panic_mode_recovery<token_type> parser_error_handler<token_type>::recovery =
            panic_mode_recovery<token_type>([](auto && t) {return false;});

    template<typename token_type>
    std::function<std::remove_pointer_t<typename parser_error_handler<token_type>::error_handler_t>>
            parser_error_handler<token_type>::error_handler([](const _abstract_parser<token_type> & parser,
                                                               const std::optional<token_type> & token,
                                                               const std::string & token_value,
                                                               const std::string & token_pos,
                                                               const std::string_view & stream_name) {
            throw parser_exception(parser.Name(),token_value,token_pos,stream_name);
    });


}
#endif //LIGHT_PARSER_ERROR_HANDLER_H
