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





#ifdef IS_WINDOWS

#include <windows.h>

#else

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#endif

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

        basic_string_stream(const basic_string_stream&) = delete;
        basic_string_stream& operator=(const basic_string_stream&) = delete;

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


    class file_stream : public base_token_stream<char, file_stream> {
        friend class base_token_stream<char, file_stream>;

        std::ifstream file;
        std::deque<char> buffer;
        size_t position = 0;
        size_t mark_pos = 0;

        size_t column = 0;
        size_t line = 1;

        std::unique_ptr<char[]> file_buffer;

        char get_char() {

            if (position >= mark_pos + buffer.size()) {
                const size_t chunk_size = 4096;
                std::vector<char> temp(chunk_size);
                file.read(temp.data(), chunk_size);
                size_t actually_read = file.gcount();

                for (size_t i = 0; i < actually_read; ++i) {
                    char c = temp[i];
                    if (c == '\n') {
                        ++line;
                        column = 0;
                    } else {
                        ++column;
                    }
                    buffer.push_back(c);
                }
            }

            if (position < mark_pos + buffer.size()) {
                return buffer[position++ - mark_pos];
            }

            return EOF;
        }

        void fill_buffer(size_t required) {
            while (buffer.size() < required + (position - mark_pos) && !file.eof()) {
                const size_t chunk_size = 4096;
                std::vector<char> temp(chunk_size);
                file.read(temp.data(), chunk_size);
                size_t actually_read = file.gcount();

                for (size_t i = 0; i < actually_read; ++i) {
                    char c = temp[i];
                    buffer.push_back(c);
                    if (c == '\n') {
                        ++line;
                        column = 0;
                    } else {
                        ++column;
                    }
                }
            }
        }

        char get_impl() {
            if (position < mark_pos + buffer.size()) {
                return buffer[position++ - mark_pos];
            }

            char c = get_char();
            if (c != EOF) {
                mark_pos = position++;
            }
            return c;
        }

        char peek_impl(size_t lookahead) {
            fill_buffer(lookahead + 1);
            if (position + lookahead >= mark_pos + buffer.size()) {
                return EOF;
            }
            return buffer[position + lookahead - mark_pos];
        }

        bool eof_impl(size_t lookahead) {
            fill_buffer(lookahead + 1);
            return position + lookahead >= mark_pos + buffer.size();
        }

        std::string pos_impl() {
            return std::format(" [line:{} , column: {}]", line, column);
        }

        void seek_impl(size_t length) {
            position += length;
        }

        std::string value_impl() {
            return std::string(1, peek_impl(0));
        }

    public:
        explicit file_stream(const std::string &filename) : file() {
            name = filename;

            //set file buffer
            const size_t buffer_size = 32768;
            file_buffer = std::make_unique<char[]>(buffer_size);
            file.rdbuf()->pubsetbuf(file_buffer.get(), buffer_size);

            file.open(filename, std::ios::binary);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file: " + filename);
            }
        }

        file_stream(const file_stream&) = delete;
        file_stream& operator=(const file_stream&) = delete;
    };

    template<typename container_type>
    auto ContainerStream(const container_type & input) {
        return container_stream<container_type>(input);
    }

#ifdef IS_WINDOWS

    class mmap_file_stream : public base_token_stream<char, mmap_file_stream> {
        friend class base_token_stream<char, mmap_file_stream>;

        HANDLE file_handle = INVALID_HANDLE_VALUE;
        HANDLE mapping_handle = nullptr;
        char* mapped_data = nullptr;
        size_t file_size = 0;
        size_t position = 0;

        size_t column = 1;
        size_t line = 1;

        char get_impl() {
            if (position >= file_size) {
                throw std::runtime_error("Read beyond end of file");
            }

            char c = mapped_data[position++];

            if (c == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            return c;
        }

        char peek_impl(size_t lookahead) {
            if (position + lookahead >= file_size) {
                return 0;
            }
            return mapped_data[position + lookahead];
        }

        bool eof_impl(size_t lookahead) {
            return position + lookahead >= file_size;
        }

        std::string pos_impl() {
            return std::format(" [line:{} , column: {}]", line, column);
        }

        void seek_impl(size_t length) {
            position += length;
            if (position > file_size) {
                position = file_size;
            }
        }

        std::string value_impl() {
            if (position == 0 || position > file_size) {
                return "";
            }
            return std::string(1, mapped_data[position - 1]);
        }

    public:
        explicit mmap_file_stream(const std::string& filename) {
            name = filename;

            file_handle = CreateFileA(
                filename.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );

            if (file_handle == INVALID_HANDLE_VALUE) {
                throw std::runtime_error("Failed to open file: " + filename);
            }

            LARGE_INTEGER size;
            if (!GetFileSizeEx(file_handle, &size)) {
                CloseHandle(file_handle);
                throw std::runtime_error("Failed to get file size: " + filename);
            }
            file_size = static_cast<size_t>(size.QuadPart);

            if (file_size == 0) {
                CloseHandle(file_handle);
                return;
            }

            mapping_handle = CreateFileMapping(
                file_handle,
                nullptr,
                PAGE_READONLY,
                0,
                0,
                nullptr
            );

            if (mapping_handle == nullptr) {
                CloseHandle(file_handle);
                throw std::runtime_error("Failed to create file mapping: " + filename);
            }

            mapped_data = static_cast<char*>(MapViewOfFile(
                mapping_handle,
                FILE_MAP_READ,
                0,
                0,
                file_size
            ));

            if (mapped_data == nullptr) {
                CloseHandle(mapping_handle);
                CloseHandle(file_handle);
                throw std::runtime_error("Failed to map view of file: " + filename);
            }
        }

        ~mmap_file_stream() {
            if (mapped_data != nullptr) {
                UnmapViewOfFile(mapped_data);
            }
            if (mapping_handle != nullptr) {
                CloseHandle(mapping_handle);
            }
            if (file_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(file_handle);
            }
        }

        mmap_file_stream(const mmap_file_stream&) = delete;
        mmap_file_stream& operator=(const mmap_file_stream&) = delete;
    };

#else

    class mmap_file_stream : public base_token_stream<char, mmap_file_stream> {
    friend class base_token_stream<char, mmap_file_stream>;

        int fd = -1;
        char* mapped_data = nullptr;
        size_t file_size = 0;
        size_t position = 0;

        size_t column = 1;
        size_t line = 1;

        char get_impl() {
            if (position >= file_size) {
                throw std::runtime_error("Read beyond end of file");
            }

            char c = mapped_data[position++];

            if (c == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            return c;
        }

        char peek_impl(size_t lookahead) {
            if (position + lookahead >= file_size) {
                return 0;
            }
            return mapped_data[position + lookahead];
        }

        bool eof_impl(size_t lookahead) {
            return position + lookahead >= file_size;
        }

        std::string pos_impl() {
            return std::format(" [line:{} , column: {}]", line, column);
        }

        void seek_impl(size_t length) {
            position += length;
            if (position > file_size) {
                position = file_size;
            }
        }

        std::string value_impl() {
            if (position == 0 || position > file_size) {
                return "";
            }
            return std::string(1, mapped_data[position - 1]);
        }

    public:
        explicit mmap_file_stream(const std::string &filename) {
            name = filename;


            if ((fd = open(filename.c_str(), O_RDONLY)) == -1) {
                throw std::runtime_error("Failed to open file: " + filename);
            }


            struct stat st;
            if (fstat(fd, &st) == -1) {
                close(fd);
                throw std::runtime_error("Failed to get file size: " + filename);
            }
            file_size = st.st_size;


            if (file_size == 0) {
                close(fd);
                return;
            }


            mapped_data = static_cast<char *>(
                    mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));

            if (mapped_data == MAP_FAILED) {
                close(fd);
                throw std::runtime_error("Failed to mmap file: " + filename);
            }


            madvise(mapped_data, file_size, MADV_SEQUENTIAL);
        }

        ~mmap_file_stream() {
            if (mapped_data != nullptr && mapped_data != MAP_FAILED) {
                munmap(mapped_data, file_size);
            }
            if (fd != -1) {
                close(fd);
            }
        }

        mmap_file_stream(const mmap_file_stream &) = delete;

        mmap_file_stream &operator=(const mmap_file_stream &) = delete;
    };
#endif
}
#endif //LIGHT_PARSER_TOKEN_STREAM_H
