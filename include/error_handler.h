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

    template<typename T>
    class _abstract_parser;




    template<typename token_type>
    class parser_error_handler {

        template<typename T>
        friend class _abstract_parser;

    public:
        template<typename FF>
        static void DefaultRecovery(FF && _recovery) {
            parser_error_handler<token_type>::recovery = std::move(panic_mode_recovery<token_type>(_recovery));
        }

        template<typename FF>
        static void DefaultOnError(FF && _func)  {
            parser_error_handler<token_type>::error_handler =
                    std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)>(std::forward<FF>(_func));
        }
    private:
        static panic_mode_recovery<token_type> recovery;
        static std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)> error_handler;
    };

    template<typename token_type>
    panic_mode_recovery<token_type> parser_error_handler<token_type>::recovery = panic_mode_recovery<token_type>([](auto && t) {return false;});

    template<typename token_type>
    std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)>
            parser_error_handler<token_type>::error_handler([](const _abstract_parser<token_type> & self,auto && token) {

        if(token) {
            if constexpr(is_formattable<token_type>::value) {
                throw parser_exception(std::format("{}",*token), self.Name());
            }
            else {
                throw parser_exception("TOKEN", self.Name());
            }
        }
        else {
            throw parser_exception("EOF", self.Name());
        }
    });


}
#endif //LIGHT_PARSER_ERROR_HANDLER_H
