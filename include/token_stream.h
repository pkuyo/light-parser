/**
 * @file token_stream.h
 * @brief Token stream abstractions and implementations, providing input handling for parser operations.
 * @author pkuyo
 * @date 2025-02-08
 * @copyright Copyright (c) 2025 pkuyo. All rights reserved.
 *
 * Includes:
 * - Base token stream template class base_token_stream
 * - String-based stream implementations (basic_string_stream)
 * - Generic container input stream (container_stream)
 * - File input stream with buffering and position tracking (file_stream)
 */

#ifndef LIGHT_PARSER_TOKEN_STREAM_H
#define LIGHT_PARSER_TOKEN_STREAM_H

#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>
#include <fstream>
#include <any>

namespace pkuyo::parsers {

    template<typename token_type, typename derived_type>
    class base_token_stream {
    public:



        token_type Get() {
            return derived().get_impl();
        }

        token_type Peek(size_t lookahead = 0) {
            return derived().peek_impl(lookahead);
        }

        void Seek(size_t length) {
            derived().seek_impl(length);
        }

        bool Eof(size_t lookahead = 0) {
            return derived().eof_impl(lookahead);
        }


        std::string Value() {
            if(Eof())
                return "EOF";
            return derived().value_impl();

        }

        std::string Pos() {
            return derived().pos_impl();
        }
        std::string_view Name() {
            return name;
        };


    protected:
        std::string value_impl() {
            return Peek(0);
        }


    protected:

        std::string name;

        derived_type &derived() { return static_cast<derived_type &>(*this); }

        const derived_type &derived() const { return static_cast<const derived_type &>(*this); }
    };


    template<typename char_type>
    class basic_string_stream : public base_token_stream<char_type,basic_string_stream<char_type>> {
        friend class base_token_stream<char_type,basic_string_stream<char_type>>;

        std::basic_string<char_type> source;
        size_t position = 0;

        char_type get_impl() {
            return source[position++];
        }

        char_type peek_impl(size_t lookahead) {
            return source[position + lookahead] ;
        }


        bool eof_impl(size_t lookahead) const {
            return position + lookahead >= source.size();
        }

        std::string pos_impl() {
            return std::format("index: {}",position);
        }

        void seek_impl(size_t length) {
            position += length;
        }

        std::string value_impl() const {
            return std::string(1,source[position]);
        }

    public:
        explicit basic_string_stream(const std::basic_string<char_type> &str) : source(str) {}
        explicit basic_string_stream(const std::basic_string<char_type> &str,std::string_view _name) : source(str) {
            this->name = _name;
        }

    };

    using string_stream = basic_string_stream<char>;
    using wstring_stream = basic_string_stream<wchar_t>;


    template<typename container_type>
    class container_stream : public base_token_stream<typename container_type::value_type,container_stream<container_type>> {
        friend class base_token_stream<typename container_type::value_type,container_stream<container_type>>;

        using value_type = container_type::value_type;

        container_type source;
        size_t position = 0;

        value_type get_impl() {
            return position < source.size() ? source[position++] : value_type{};
        }

        value_type peek_impl(size_t lookahead) {
            return (position + lookahead) < source.size() ?
                   source[position + lookahead] : value_type{};
        }

        bool eof_impl(size_t lookahead) const {
            return position + lookahead >= source.size();
        }


        std::string pos_impl() {
            return std::format("index: {}",position);
        }

        void seek_impl(size_t length) {
            position += length;
        }

        std::string value_impl() {
            return value_func(source[position]);
        }



    public:
        container_stream(const container_type &vec) : source(vec), value_func([](const value_type &value) -> std::string {
            if constexpr (std::is_convertible_v<value_type, std::string>) {
                return (std::string) value;
            } else {
                return "TOK";
            }
        }) {}

        container_stream(const container_type &vec,  std::string(*_value_func)(const value_type &)) : source(vec), value_func(_value_func){}

        container_stream(const container_type &vec, std::string_view _name) : container_stream(vec) {
            this->name = _name;
        }

        container_stream(const container_type &vec, std::string(*_value_func)(const value_type &),
                         std::string_view _name) : container_stream(vec, _value_func) {
            this->name = _name;
        }

        std::string (*value_func)(const value_type &);
    };


    class file_stream : public base_token_stream<char,file_stream> {
        friend class base_token_stream<char,file_stream>;

        std::ifstream file;
        std::vector<char> buffer;
        size_t position = 0;
        size_t mark_pos = 0;

        size_t column = 0;
        size_t line = 1;

        char get_char() {
            column++;
            char c= file.get();
            if(c == '\n') {
                column = 0;
                line++;
            }
            return c;
        }

        void fill_buffer(size_t required) {
            while (buffer.size() <= position+required- mark_pos && !file.eof()) {
                buffer.push_back(get_char());
            }
        }

        char get_impl() {
            if(position - mark_pos < buffer.size())
                return buffer[position++];


            mark_pos = position++;

            if(!buffer.empty())
                buffer.clear();

            return get_char();

        }
        char peek_impl(size_t lookahead) {
            fill_buffer(lookahead);
            return buffer[lookahead + position - mark_pos];
        }

        bool eof_impl(size_t lookahead) {
            if(lookahead+position-mark_pos >= buffer.size())
                fill_buffer(lookahead);
            return lookahead+position-mark_pos >= buffer.size();
        }

        std::string pos_impl() {
            return std::format(" [line:{} , column: {}]",line,column);
        }

        void seek_impl(size_t length) {
            position += length;
        }

        std::string value_impl() {
            return std::string(1, peek_impl(0));
        }


    public:
        explicit file_stream(const std::string &filename)
                : file(filename, std::ios::binary) {
            name = filename;
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file: " + filename);
            }
        }
    };

    template<typename container_type>
    auto ContainerStream(const container_type & input) {
        return container_stream<container_type>(input);
    }
}
#endif //LIGHT_PARSER_TOKEN_STREAM_H
