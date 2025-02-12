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

#ifndef LIGHT_PARSER_COMPILE_TIME_PARSER_H
#define LIGHT_PARSER_COMPILE_TIME_PARSER_H

#include "base_parser.h"

namespace pkuyo::parsers {



    // A parser that only peek tokens, where child_parser->Peek() == false, returning 'nullptr'
    template<typename child_type>
    class parser_not : public base_parser<typename std::decay_t<child_type>::token_t,parser_not<child_type>>{
    public:

        constexpr explicit parser_not(child_type && _child_parser) : child_parser(std::forward<child_type>(_child_parser)) {}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if(this->peek_impl(stream))
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }
    public:

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return !child_parser.peek_impl(stream);
        }

        void reset_impl() const {
            child_parser.reset_impl();
        }

        void no_error_impl() {
            child_parser.NoError_Internal();
        }

    private:
        child_type child_parser;
    };

    // A parser that only peek tokens, where child_parser->Peek() == true, returning 'nullptr'
    template<typename child_type>
    class parser_pred : public base_parser<typename std::decay_t<child_type>::token_t,parser_pred<child_type>>{
    public:

        constexpr explicit parser_pred(const child_type & _child_parser) : child_parser(_child_parser) {}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if(this->peek_impl(stream))
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return child_parser.peek_impl(stream);
        }
        void reset_impl() const {
            child_parser.reset_impl();
        }

        void no_error_impl() {
            child_parser.NoError_Internal();
        }

    private:
        child_type  child_parser;
    };

    template<typename child_type>
    class parser_ignore : public base_parser<typename std::decay_t<child_type>::token_t,parser_ignore<child_type>>{
    public:

        constexpr explicit parser_ignore(const child_type & _child_parser) : child_parser(_child_parser) {}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {
            auto re = child_parser.parse_impl(stream,global_state,state);
            if(re)
                return std::make_optional(nullptr);
            return std::optional<nullptr_t>();
        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return child_parser.peek_impl(stream);
        }

        void reset_impl() const {
            child_parser.reset_impl();
        }

        void no_error_impl() {
            child_parser.NoError_Internal();
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

        constexpr explicit parser_check(cmp_type && _cmp_value) : cmp_value(std::forward<cmp_type>(_cmp_value)) {
            if constexpr (std::is_same_v<cmp_type,char>)
                this->parser_name[0] = _cmp_value;
        }

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if(!this->peek_impl(stream)){
                this->error_handle_recovery(stream);
                return std::optional<nullptr_t>();
            }
            stream.Get();
            return std::make_optional(nullptr);

        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return !stream.Eof() && stream.Peek() == cmp_value;
        }

    private:
        cmp_type cmp_value;
    };
    template<typename token_type, typename FF>
    class parser_check_with_func : public base_parser<token_type,parser_check_with_func<token_type,FF>>{
    public:

        constexpr explicit parser_check_with_func(FF && _cmp_func) : cmp_func(std::forward<FF>(_cmp_func)) {}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if(!this->peek_impl(stream)){
                this->error_handle_recovery(stream);
                return std::optional<nullptr_t>();
            }
            stream.Get();
            return std::make_optional(nullptr);

        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return !stream.Eof() && cmp_func(stream.Peek());
        }


    private:
        FF cmp_func;
    };

    template<typename token_type,typename buff_type,size_t buff_size>
    class parser_buff_seq_check : public base_parser<token_type,parser_buff_seq_check<token_type,buff_type,buff_size>> {
    public:
        constexpr parser_buff_seq_check(const buff_type (&input)[buff_size]) {
            std::copy_n(input,buff_size,cmp);
            if constexpr (std::is_same_v<buff_type,char>)
                std::copy_n(input,buff_size,this->parser_name);
        }

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if(!this->peek_impl(stream)) {
                this->error_handle_recovery(stream);
                return std::optional<nullptr_t>();
            }
            stream.Seek(real_size);
            return std::make_optional(nullptr);
        }
        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            for(int i = 0; i< real_size;i++) {
                if(stream.Eof(i) || stream.Peek(i) != cmp[i]) {
                    return false;
                }
            }
            return true;
        }
        static constexpr size_t real_size = (std::is_same_v<buff_type,char> || std::is_same_v<buff_type,wchar_t>) ? buff_size-1 : buff_size;

        buff_type cmp[buff_size]{};
    };

    // A parser class that checks and stores input values until the input equals a specified comparison value.
    // This class is used to continuously receive input and stop storing when the input value equals `cmp_value`. It is primarily designed for parsing data under specific conditions
    template<typename token_type,typename cmp_type>
    struct parser_until : public pkuyo::parsers::base_parser<token_type,parser_until<token_type,cmp_type>> {

        constexpr parser_until(const cmp_type & _cmp) : cmp(_cmp) {
            if constexpr (std::is_same_v<cmp_type,char>){
                std::copy_n("Until  ",7,this->parser_name);
                this->parser_name[6] = cmp;
            }
        }

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if (!peek_impl(stream))
                return std::optional<result_container_t<token_type>>();
            result_container_t<token_type> result;
            while (!stream.Eof() && stream.Peek() != cmp) {
                result.push_back(stream.Get());
            }
            return std::make_optional(std::move(result));

        }
        bool peek_impl(auto & stream) const {
            if(stream.Eof() || stream.Peek() == cmp)
                return false;
            return true;
        }
        cmp_type cmp;
    };


    template<typename token_type,typename FF>
    struct parser_until_with_func : public pkuyo::parsers::base_parser<token_type,parser_until_with_func<token_type,FF>> {

        constexpr parser_until_with_func(const FF & _cmp) : cmp(_cmp) {
            if constexpr (std::is_same_v<FF,char>){
                std::copy_n("Until_Func",10,this->parser_name);
            }
        }

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {

            if (!peek_impl(stream))
                return std::optional<result_container_t<token_type>>();
            result_container_t<token_type> result;
            while (!stream.Eof() && !cmp(stream.Peek())) {
                result.push_back(stream.Get());
            }
            return std::make_optional(std::move(result));
        }
        bool peek_impl(auto & stream) const {
            if(stream.Eof() || cmp(stream.Peek()))
                return false;
            return true;
        }
        FF cmp;
    };

    // A parser that only matches the first token.
    //  Uses a function to determine whether the token matches.
    //  If the match is successful, it returns a 'result_type' constructed with the token as an argument;
    //  if the match fails, it attempts an error recovery strategy and returns std::nullopt.
    template<typename token_type, typename return_type, typename FF>
    class parser_with_func : public base_parser<token_type, parser_with_func<token_type,return_type, FF>> {

    public:

        constexpr explicit parser_with_func(FF _cmp_func)
                : cmp_func(_cmp_func){}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if(!this->peek_impl(stream)){
                this->error_handle_recovery(stream);
                return std::optional<return_type>();
            }
            if constexpr (is_smart_pointer_v<return_type>) {
                return std::make_optional(std::move(return_type(new remove_smart_pointer_t<return_type>(stream.Get()))));
            }
            else {
                return std::make_optional<return_type>(stream.Get());
            }

        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return !stream.Eof() && cmp_func(stream.Peek());
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

            if constexpr (std::is_same_v<token_type,char>)
                std::copy_n(_cmp_value,size,this->parser_name);

        }

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            for(int i = 0;i<real_size;i++) {
                if(stream.Peek(i) != cmp_value[i]) {
                    this->error_handle_recovery(stream);
                    return std::optional<std::basic_string_view<token_type>>();
                }
            }
            stream.Seek(real_size);
            return std::make_optional(std::basic_string_view<token_type>(cmp_value));

        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return !stream.Eof() && stream.Peek() == *cmp_value;
        }

    private:
        static constexpr size_t real_size = (std::is_same_v<token_type,char> || std::is_same_v<token_type,wchar_t>) ? size-1 : size;

        token_type cmp_value[size]{};
    };


    // A parser that only matches the first token.
    //  Requires that 'cmp_type' and 'token_type' have overloaded the == and != operators.
    //  If the match is successful, it returns 'return_type‘;
    //  if the match fails, it attempts an error recovery strategy and returns 'std::nullopt'.
    template<typename token_type, typename return_type, typename cmp_type,typename FF>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_single : public base_parser<token_type, parser_single<token_type,return_type,cmp_type,FF>> {

    public:

        constexpr parser_single(cmp_type && _cmp_value,FF constructor)
                : cmp_value(std::forward<cmp_type>(_cmp_value)), constructor(constructor) {
            if constexpr(std::is_same_v<cmp_type,char>)
                this->parser_name[0] = _cmp_value;
        }

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if(!this->peek_impl(stream)) {
                this->error_handle_recovery(stream);
                return std::optional<return_type>();
            }
            return std::optional<return_type>(std::move(constructor(stream.Get())));
        }
    public:

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return !stream.Eof() && stream.Peek() == cmp_value;
        }

    private:

        FF constructor;
        cmp_type cmp_value;
    };

    template<typename token_type,typename return_type,typename buff_type,size_t buff_size,typename FF>
    class parser_buff_seq : public base_parser<token_type,parser_buff_seq<token_type,return_type,buff_type,buff_size,FF>> {
    public:


        constexpr parser_buff_seq(const buff_type (&input)[buff_size], FF constructor) : constructor(constructor) {
            std::copy_n(input,buff_size,cmp);
            if constexpr (std::is_same_v<buff_type,char>)
                std::copy_n(input,buff_size,this->parser_name);
        }

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            if(!this->peek_impl(stream)) {
                this->error_handle_recovery(stream);
                return std::optional<return_type>();
            }
            stream.Seek(real_size);
            return std::make_optional(constructor(cmp));
        }
        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            for(int i = 0; i< real_size;i++) {
                if(stream.Eof(i) || stream.Peek(i) != cmp[i]) {
                    return false;
                }
            }
            return true;
        }

        buff_type cmp[buff_size]{};

        static constexpr size_t real_size = (std::is_same_v<buff_type,char> || std::is_same_v<buff_type,wchar_t>) ? buff_size-1 : buff_size;

        FF constructor;
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

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {
            using result_r = parser_result_t<input_type_right,GlobalState,State>;
            using result_l = parser_result_t<input_type_left,GlobalState,State>;

            using return_type = then_result_t<result_l,result_r>;
            auto left_re = children_parsers.first.parse_impl(stream,global_state,state);

            // If the match fails, attempts to recover and restores the token index, then returns nullopt.
            if(left_re == std::nullopt) {
                this->error_handle_recovery(stream);
                return std::optional<return_type>();
            }

            auto right_re = children_parsers.second.parse_impl(stream,global_state,state);
            if(right_re == std::nullopt) {
                this->error_handle_recovery(stream);
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

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return children_parsers.first.peek_impl(stream);
        }

        void reset_impl() const {
            children_parsers.first.reset_impl();
            children_parsers.second.reset_impl();
        }


        void no_error_impl() {
            children_parsers.first.NoError_Internal();
            children_parsers.second.NoError_Internal();
        }

    private:
        std::pair<input_type_left,input_type_right> children_parsers;

    };


    // A parser that queries and returns the result of the first sub-parser that satisfies the condition.
    // Note that it only predicts one token; backtracking for more than one token will result in a parser_exception.
    // The result_type is either the base class of each sub-parser or std::variant<types...>.
    template<typename input_type_l,typename input_type_r,bool with_back_track>
    class parser_or : public base_parser<typename std::decay_t<input_type_l>::token_t, parser_or<input_type_l,input_type_r,with_back_track>> {
    public:

        constexpr parser_or(const input_type_l &left,const input_type_r & right) : children_parsers(std::make_pair(left,right)){}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {

            using result_t = filter_or_t<input_type_l,input_type_r,GlobalState,State>;
            if(!this->peek_impl(stream)) {
                this->error_handle_recovery(stream);
                return std::optional<result_t>();
            }
            else if(children_parsers.first.peek_impl(stream)) {
                auto re = children_parsers.first.parse_impl(stream,global_state,state);
                if(!re) {
                    this->error_handle_recovery(stream);
                    return std::optional<result_t>();
                }
                else return std::optional<result_t>(std::move(re.value()));
            }
            else {
                auto re = children_parsers.second.parse_impl(stream,global_state,state);
                if(!re) {
                    this->error_handle_recovery(stream);
                    return std::optional<result_t>();
                }
                else return std::make_optional<result_t>(std::move(re.value()));
            }

        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return children_parsers.first.peek_impl(stream) || children_parsers.second.peek_impl(stream);
        }


        void reset_impl() const {
            children_parsers.first.reset_impl();
            children_parsers.second.reset_impl();
        }

        void no_error_impl() {
            children_parsers.first.NoError_Internal();
            children_parsers.second.NoError_Internal();
        }

    private:
        std::pair<input_type_l,input_type_r> children_parsers;

    };
    template<typename input_type_l,typename input_type_r>
    class parser_or<input_type_l,input_type_r,true> :
            public base_parser<typename std::decay_t<input_type_l>::token_t, parser_or<input_type_l,input_type_r,true>> {
    public:

        constexpr parser_or(const input_type_l &left,const input_type_r & right) : children_parsers(std::make_pair(left,right)){}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {

            using result_t = filter_or_t<input_type_l,input_type_r,GlobalState,State>;
            auto stream_state = stream.Save();

            if(children_parsers.first.peek_impl(stream)) {
                auto re = children_parsers.first.parse_impl(stream,global_state,state);
                if(!re) {
                    stream.Restore(stream_state);
                }
                else return std::optional<result_t>(std::move(re.value()));
            }
            if(children_parsers.second.peek_impl(stream))  {
                auto re = children_parsers.second.parse_impl(stream,global_state,state);
                if(!re) {
                    stream.Restore(stream_state);
                    return std::optional<result_t>();
                }
                else return std::make_optional<result_t>(std::move(re.value()));
            }
        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return children_parsers.first.peek_impl(stream) || children_parsers.second.peek_impl(stream);
        }


        void reset_impl() const {
            children_parsers.first.reset_impl();
            children_parsers.second.reset_impl();
        }

        void no_error_impl() {
            children_parsers.first.NoError_Internal();
            children_parsers.second.NoError_Internal();
        }

    private:
        std::pair<input_type_l,input_type_r> children_parsers;

    };
    // Match the child parser 0 or more times, returning std::vector<child_return_type> (std::basic_string<child_return_type> for char or wchar_t).
    template<typename child_type>
    class parser_many : public base_parser<typename std::decay_t<child_type>::token_t,parser_many<child_type>> {

    public:
        constexpr explicit parser_many(const child_type & child): child_parser(child) {}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {
            using child_return_type = decltype(child_parser.parse_impl(stream,global_state,state))::value_type;

            result_container_t<child_return_type> results;
            if constexpr(std::is_same_v<child_return_type,nullptr_t>) results = nullptr;
            while (!stream.Eof() && child_parser.peek_impl(stream)) {
                auto result = child_parser.parse_impl(stream, global_state, state);
                if (!result) {
                    this->error_handle_recovery(stream);
                    break;
                }
                if constexpr(!std::is_same_v<child_return_type,nullptr_t>)
                    results.push_back(std::move(*result));
            }
            return std::make_optional(std::move(results));

        }
        template<typename Stream>
        bool peek_impl(Stream &) const {
            return true;
        }


        void reset_impl() const {
            child_parser.reset_impl();
        }

        void no_error_impl() {
            child_parser.NoError_Internal();
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

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {
            using child_return_type = decltype(child_parser.parse_impl(stream,global_state,state))::value_type;
            result_container_t<child_return_type> results;
            if constexpr(std::is_same_v<child_return_type,nullptr_t>) results = nullptr;
            auto first_result = child_parser.parse_impl(stream, global_state, state);
            if (!first_result) {
                this->error_handle_recovery(stream);
                return std::optional<result_container_t<child_return_type>>();
            }
            if constexpr(!std::is_same_v<child_return_type,nullptr_t>)
                results.push_back(std::move(*first_result));

            while (!stream.Eof() && child_parser.peek_impl(stream)) {
                auto result = child_parser.parse_impl(stream, global_state, state);
                if (!result) {
                    this->error_handle_recovery(stream);
                    break;
                }
                if constexpr(!std::is_same_v<child_return_type,nullptr_t>)
                    results.push_back(std::move(*result));
            }
            return std::make_optional(std::move(results));

        }

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return child_parser.peek_impl(stream);
        }

        void reset_impl() const {
            child_parser.reset_impl();
        }

        void no_error_impl() {
            child_parser.NoError_Internal();
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
        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {
            using return_type = decltype(child_parser.parse_impl(stream,global_state,state))::value_type;
            std::optional<return_type> result(std::nullopt);
            if(child_parser.peek_impl(stream)) {
                auto re = child_parser.parse_impl(stream,global_state,state);
                if (re == std::nullopt) {
                    this->error_handle_recovery(stream);
                    return std::optional<std::optional<return_type>>();
                }
                result = std::move(re.value());
            }

            return std::make_optional(std::move(result));
        }

        template<typename Stream>
        bool peek_impl(Stream &) const {
            return true;
        }
        void reset_impl() const {
            child_parser.reset_impl();
        }

        void no_error_impl() {
            child_parser.NoError_Internal();
        }
    private:
        child_type child_parser;
    };


    // Lazy initialization parser to resolve recursive dependencies between parsers.
    // No error recovery.
    template <typename token_type, typename real_type>
    class parser_lazy : public base_parser<token_type,parser_lazy<token_type,real_type>> {
    public:

        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return factory().peek_impl(stream);
        }

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {
            return factory().parse_impl(stream,global_state,state);
        }
    private:
       static real_type& factory() {
            static real_type real;
            return real;
        }

    };


    // Convert the parsing result to the target type's parser.
    template<typename child_type, typename FF>
    class parser_map : public base_parser<typename std::decay_t<child_type>::token_t, parser_map<child_type,FF>> {
    public:

        using mapper_t = FF;

        constexpr parser_map(const child_type & source, mapper_t mapper)
                : source(source), mapper(mapper) {}

        template<typename Stream,typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {

            using SourceType = parser_result_t<child_type, GlobalState, State>;



            if constexpr (std::is_same_v<State, nullptr_t>) {


                if constexpr (!std::is_same_v<GlobalState, nullptr_t>) {


                    using TargetType = decltype(std::declval<mapper_t>()(std::declval<SourceType>(),
                                                                         std::declval<std::add_lvalue_reference_t<GlobalState>>()));

                    auto source_result = source.parse_impl(stream, global_state, state);
                    if (source_result == std::nullopt) {
                        this->error_handle_recovery(stream);
                        return std::optional<TargetType>();
                    }
                    return std::make_optional(std::move(mapper(std::move(source_result.value()),global_state)));
                }
                else {
                    using TargetType = decltype(std::declval<mapper_t>()(std::declval<SourceType>()));

                    auto source_result = source.parse_impl(stream, global_state, state);
                    if (source_result == std::nullopt) {
                        this->error_handle_recovery(stream);
                        return std::optional<TargetType>();
                    }
                    return std::make_optional(std::move(mapper(std::move(source_result.value()))));
                }
            }
            else {



                if constexpr (!std::is_same_v<GlobalState, nullptr_t>) {
                    using TargetType = decltype(std::declval<mapper_t>()(std::declval<SourceType>(),
                                                                         std::declval<std::add_lvalue_reference_t<GlobalState>>(),
                                                                         std::declval<std::add_lvalue_reference_t<State>>()));
                    auto source_result = source.parse_impl(stream, global_state, state);
                    if (source_result == std::nullopt) {
                        this->error_handle_recovery(stream);
                        return std::optional<TargetType>();
                    }
                    return std::make_optional(std::move(mapper(std::move(source_result.value()),global_state, state)));
                }
                else {

                    using TargetType = decltype(std::declval<mapper_t>()(std::declval<SourceType>(),
                                                                         std::declval<std::add_lvalue_reference_t<State>>()));

                    auto source_result = source.parse_impl(stream, global_state, state);
                    if (source_result == std::nullopt) {
                        this->error_handle_recovery(stream);
                        return std::optional<TargetType>();
                    }
                    return std::make_optional(std::move(mapper(std::move(source_result.value()), state)));
                }
            }
        }


        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return source.peek_impl(stream);
        }


        void reset_impl() const {
            source.reset_impl();
        }

        void no_error_impl() {
            source.NoError_Internal();
        }
    private:
        child_type source;
        mapper_t mapper;
    };


    template<typename child_type, typename FF>
    class parser_action : public base_parser<typename std::decay_t<child_type>::token_t, parser_action<child_type,FF>> {
    public:

        using action_t = FF;

        constexpr parser_action(const child_type & source, action_t action)
                : source(source), action(action) {}

        template<typename Stream,typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {

            using SourceType = parser_result_t<child_type, GlobalState, State>;

            if constexpr (std::is_same_v<State, nullptr_t>) {

                if constexpr (!std::is_same_v<GlobalState, nullptr_t>) {

                    auto source_result = source.parse_impl(stream, global_state, state);
                    if (source_result == std::nullopt) {
                        this->error_handle_recovery(stream);
                        return std::optional<SourceType>();
                    }
                    action(*source_result,global_state);
                    return source_result;
                }
                else {
                    auto source_result = source.parse_impl(stream, global_state, state);
                    if (source_result == std::nullopt) {
                        this->error_handle_recovery(stream);
                        return std::optional<SourceType>();
                    }
                    action(*source_result);
                    return source_result;
                }
            }
            else {

                if constexpr (!std::is_same_v<GlobalState, nullptr_t>) {
                    auto source_result = source.parse_impl(stream, global_state, state);
                    if (source_result == std::nullopt) {
                        this->error_handle_recovery(stream);
                        return std::optional<SourceType>();
                    }
                    action(*source_result,global_state, state);
                    return source_result;
                }
                else {
                    auto source_result = source.parse_impl(stream, global_state, state);
                    if (source_result == std::nullopt) {
                        this->error_handle_recovery(stream);
                        return std::optional<SourceType>();
                    }
                    action(*source_result, state);
                    return source_result;
                }
            }
        }


        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return source.peek_impl(stream);
        }

        void reset_impl() const {
            source.reset_impl();
        }

        void no_error_impl() {
            source.NoError_Internal();
        }
    private:
        child_type source;
        action_t action;
    };


    template<typename child_type,typename FF>
    class parser_where : public base_parser<typename std::decay_t<child_type>::token_t, parser_where<child_type,FF>> {
    public:
        using Predicate = FF;

        constexpr parser_where(const child_type &  parser, Predicate pred)
                : child_parser(parser), predicate(pred) {}

        template<typename Stream,typename GlobalState, typename State>
        auto parse_impl(Stream& stream,GlobalState& global_state , State& state) const {
            using ReturnType = parser_result_t<child_type,GlobalState,State>;

            auto result = child_parser.parse_impl(stream,global_state,state);

            if constexpr (std::is_same_v<nullptr_t,State>) {
                if constexpr (!std::is_same_v<GlobalState, nullptr_t>) {
                    if (!result || !predicate(*result,global_state)) {
                        this->error_handle_recovery(stream);
                        return std::optional<ReturnType>();
                    }
                }
                else {
                    if (!result || !predicate(*result)) {
                        this->error_handle_recovery(stream);
                        return std::optional<ReturnType>();
                    }
                }
            }
            else {


                if constexpr (!std::is_same_v<GlobalState, nullptr_t>) {
                    if (!result || !predicate(*result, global_state, state)) {
                        this->error_handle_recovery(stream);
                        return std::optional<ReturnType>();
                    }
                }
                else {
                    if (!result || !predicate(*result, state)) {
                        this->error_handle_recovery(stream);
                        return std::optional<ReturnType>();
                    }
                }
            }
            return result;
        }



        template<typename Stream>
        bool peek_impl(Stream & stream) const {
            return child_parser.peek_impl(stream);
        }

        void reset_impl() const {
            child_parser.reset_impl();
        }

        void no_error_impl() {
            child_parser.NoError_Internal();
        }
    private:
        child_type child_parser;
        Predicate predicate;
    };


    template<typename Parser, typename State>
    class state_parser : public base_parser<typename Parser::token_t, state_parser<Parser, State>> {
    public:
        constexpr explicit state_parser(Parser&& parser)
                : parser(std::forward<Parser>(parser)) {}

        template<typename Stream, typename GlobalState, typename LastState>
        auto parse_impl(Stream& stream,GlobalState& global_state, LastState&) const {
            return parser.parse_impl(stream,global_state, factory());
        }

        template<typename Stream>
        bool peek_impl(Stream& stream) const {
            return parser.peek_impl(stream);
        }

        State& factory(bool init = false)const {
            static State newState;
            if(init)
                newState = State();
            return newState;
        }

        void reset_impl() const {
            parser.reset_impl();
            factory(true);
        }

        void no_error_impl() {
            parser.NoError_Internal();
        }
    private:
        Parser parser;
    };

    template<typename Parser, typename OnError, typename Recovery>
    class try_catch_parser : public base_parser<typename Parser::token_t, try_catch_parser<Parser, OnError, Recovery>> {
    public:
        constexpr try_catch_parser(Parser&& parser, OnError&& on_error, Recovery&& recovery)
                : parser(std::forward<Parser>(parser)),
                  on_error(std::forward<OnError>(on_error)),
                  recovery(std::forward<Recovery>(recovery)) {}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState& global_state, State& state) const {
            if (!parser.peek_impl(stream)) {
                return recovery.parse_impl(stream, global_state, state);
            }

            try {
                return parser.parse_impl(stream, global_state, state);
            } catch (const parser_exception& ex) {

                on_error(ex, global_state);
                return recovery.parse_impl(stream, global_state, state);
            }
        }

        template<typename Stream>
        bool peek_impl(Stream& stream) const {
            return parser.peek_impl(stream);
        }

        void reset_impl() const {
            parser.reset_impl();
            recovery.reset_impl();
        }
        void no_error_impl() {
            parser.NoError_Internal();
            recovery.NoError_Internal();
        }
    private:
        Parser parser;
        OnError on_error;
        Recovery recovery;
    };

    template <typename token_type,bool is_after_this,typename FF,typename return_type = nullptr_t>
    class sync_point_recovery_parser : public base_parser<token_type,sync_point_recovery_parser<token_type,is_after_this,FF,return_type>> {
    public:
        constexpr sync_point_recovery_parser(FF && _sync_func) : sync_func(_sync_func) {}

        template<typename Stream, typename GlobalState, typename State>
        auto parse_impl(Stream& stream, GlobalState&, State&) const {
            while(!sync_func(stream.Peek()) && !stream.Eof())
                stream.Seek(1);

            if constexpr (is_after_this)
                stream.Seek(1);

            if (std::is_same_v<return_type,nullptr_t>)
                return std::make_optional(nullptr);
            else
                return std::optional<return_type>(return_type());
        }

        template<typename Stream>
        bool peek_impl(Stream&) const {
            return true;
        }

    private:
        FF sync_func;
    };
}

namespace pkuyo::parsers {

    template<typename token_type, typename cmp_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    constexpr auto Check(cmp_type &&cmp) {
        return parser_check<token_type, cmp_type>(std::forward<cmp_type>(cmp));
    }

    template<typename token_type, typename FF>
    requires (!std::__weakly_equality_comparable_with<token_type, FF>)
    constexpr auto Check(FF &&cmp_func) {
        return parser_check_with_func<token_type, FF>(std::forward<FF>(cmp_func));
    }

    template<typename token_type, typename cmp_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    constexpr auto Until(cmp_type &&cmp) {
        return parser_until<token_type, cmp_type>(std::forward<cmp_type>(cmp));
    }

    template<typename token_type, typename FF>
    constexpr auto Until(FF &&cmp_func) {
        return parser_until_with_func<token_type, FF>(std::forward<FF>(cmp_func));
    }

    template<typename token_type,size_t size>
    constexpr auto Str(const token_type (&str)[size]) {
        return parser_str<token_type,size>(str);
    }

    template<typename token_type, typename cmp_type = token_type,typename return_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    constexpr auto SingleValue(cmp_type && cmp_value) {
        auto constructor = [](const token_type& s) { return return_type(s); };
        return parser_single<token_type,return_type,cmp_type, decltype(constructor)>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    constexpr auto SinglePtr(cmp_type && cmp_value) {
        auto constructor = [](const token_type& s) { return std::make_unique<return_type>(s); };
        return parser_single<token_type,std::unique_ptr<return_type>,cmp_type, decltype(constructor)>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type,typename FF>
    constexpr auto SingleValue(cmp_type && cmp_value, FF constructor) {
        return parser_single<token_type,return_type,cmp_type, FF>(std::forward<cmp_type>(cmp_value),constructor);
    }

    template<typename token_type, typename cmp_type = token_type, typename return_type = token_type,typename FF>
    constexpr auto SinglePtr(cmp_type && cmp_value,FF constructor) {
        return parser_single<token_type,std::unique_ptr<return_type>,cmp_type,FF>(std::forward<cmp_type>(cmp_value),constructor);
    }


    template<typename token_type,typename return_type = token_type, typename FF>
    requires (!std::__weakly_equality_comparable_with<token_type, FF>)
    constexpr auto SingleValue(FF compare_func) {
        return parser_with_func<token_type, return_type,FF>(compare_func);
    }

    template<typename token_type,typename return_type, typename FF>
    requires (!std::__weakly_equality_comparable_with<token_type, FF>)
    constexpr auto SinglePtr(FF compare_func) {
        return parser_with_func<token_type,std::unique_ptr<return_type>,FF>(compare_func);
    }

    template<typename token_type, typename cmp_type, size_t size>
    constexpr auto SeqCheck(const cmp_type (&s)[size]) {
        return parser_buff_seq_check<token_type, cmp_type,size>(s);
    }

    template<typename token_type,typename return_type, typename cmp_type, size_t size>
    constexpr auto SeqValue(const cmp_type (&cmp)[size]) {
        constexpr auto constructor = [](const cmp_type (&s)[size]) { return return_type(s); };
        return parser_buff_seq<token_type,return_type, cmp_type,size, decltype(constructor)>(cmp,constructor);
    }

    template<typename token_type,typename return_type, typename cmp_type, size_t size>
    constexpr auto SeqPtr(const cmp_type (&cmp)[size]) {
        constexpr auto constructor = [](const cmp_type (&s)[size]) { return std::make_unique<return_type>(s); };
        return parser_buff_seq<token_type,std::unique_ptr<return_type>, cmp_type,size, decltype(constructor)>(cmp,constructor);
    }

    template<typename token_type,typename return_type,typename FF, typename cmp_type, size_t size>
    constexpr auto SeqValue(const cmp_type (&cmp)[size],FF ff) {
        return parser_buff_seq<token_type,return_type, cmp_type,size, FF>(cmp,ff);
    }

    template<typename token_type,typename return_type,typename FF, typename cmp_type, size_t size>
    constexpr auto SeqPtr(const cmp_type (&cmp)[size], FF ff) {
        return parser_buff_seq<token_type,std::unique_ptr<return_type>, cmp_type,size, FF>(cmp,ff);
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
    constexpr auto Action(child_type && child, FF && action) {
        return parser_action<std::remove_reference_t<child_type>, std::remove_reference_t<FF>>
        (std::forward<child_type>(child),std::forward<FF>(action));
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
        return parser_or<std::remove_reference_t<l>,std::remove_reference_t<r>,false>(std::forward<l>(left),std::forward<r>(right));
    }
    template<typename l,typename r>
    requires std::is_same_v<typename std::remove_reference_t<l>::token_t,typename std::remove_reference_t<r>::token_t> && is_parser<l> && is_parser<r>
    constexpr auto Or_BackTrack(l&& left, r&& right) {
        return parser_or<std::remove_reference_t<l>,std::remove_reference_t<r>,true>(std::forward<l>(left.NoError()),std::forward<r>(right.NoError()));
    }
    template<typename l,typename r,typename ...args>
    constexpr auto Or_BackTrack(l&& left,r&& right, args&&... arg) {
        return Or_BackTrack(Or_BackTrack(left,right),arg...);
    }

    template<typename l,typename r>
    requires std::is_same_v<typename std::remove_reference_t<l>::token_t,typename std::remove_reference_t<r>::token_t> && is_parser<l> && is_parser<r>
    constexpr auto Then(l&& left, r&& right) {
        return parser_then<std::remove_reference_t<l>,std::remove_reference_t<r>>(std::forward<l>(left),std::forward<r>(right));
    }

    template<typename child_type>
    requires is_parser<child_type>
    constexpr auto Ignore(child_type && child) {
        return parser_ignore<std::remove_reference_t<child_type>>(std::forward<child_type>(child));
    }

    template<typename State,typename Parser>
    constexpr auto WithState(Parser&& parser) {
        return state_parser<Parser, State>(std::forward<Parser>(parser));
    }


    template<typename Parser, typename Recovery>
    constexpr auto TryCatch(Parser&& parser, Recovery&& recovery) {
        auto on_error = [](auto &&,auto &&) {};
        return try_catch_parser<Parser, decltype(on_error), Recovery>(
                std::forward<Parser>(parser),
                std::move(on_error),
                std::forward<Recovery>(recovery)
        );
    }

    template<typename Parser, typename OnError, typename Recovery>
    constexpr auto TryCatch(Parser&& parser, Recovery&& recovery,OnError&& on_error) {
        return try_catch_parser<Parser, OnError, Recovery>(
                std::forward<Parser>(parser),
                std::forward<OnError>(on_error),
                std::forward<Recovery>(recovery)
        );
    }

    template<typename token_type,typename FF,bool is_after_this = false, typename return_type = nullptr_t>
    requires (!std::__weakly_equality_comparable_with<token_type, FF>)
    constexpr auto Sync(FF&& sync_func) {
        return sync_point_recovery_parser<token_type,is_after_this,FF,return_type>(std::forward<FF>(sync_func));
    }

    template<typename token_type,bool is_after_this = false,typename cmp_type = token_type, typename return_type = nullptr_t>
    requires (std::__weakly_equality_comparable_with<token_type, cmp_type>)
    constexpr auto Sync(token_type&& sync_point) {
        auto ff = [=](const token_type& token) {return token == sync_point;};
        return sync_point_recovery_parser<token_type,is_after_this, decltype(ff),return_type>(std::move(ff));
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
        return Map(std::forward<parser_type>(parser),std::forward<Mapper>(mapper));
    }


    // Overloads '<<=' for semantic action injection, retaining the original return value.
    template <typename parser_type,typename action>
    requires is_parser<parser_type>
    constexpr auto operator<<=(parser_type && parser,action&& func) {
        return Action(
                std::forward<parser_type>(parser),
                std::forward<action>(func)
        );
    }


    //Creates parser_where using *this and pred function.
    template <typename parser_type,typename Action>
    requires is_parser<parser_type>
    constexpr auto operator&&(const parser_type & parser,Action&& pred) {
        return Where(parser,pred);
    }
}
#endif //LIGHT_PARSER_COMPILE_TIME_PARSER_H
