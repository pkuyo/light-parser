//
// Created by pkuyo on 2025/2/4.
//

#include "parser.h"
#include <string>
#include <vector>
#include <variant>
#include <iostream>
#include <chrono>
#include "xml_def.h"




namespace xml {


/*
 * ---Terminal Symbol:
 *
 * space            = '\n' | '\r' | ' '
 * skip_space_must  = space+
 * skip_space       = space*
 *
 * tag_name         = [A-Za-z_:][A-Za-z0-9_:.-]*
 * quoted_str       = '"' [^"]* '"' | "'" [^']* "'"
 *
 * text             = [^<]+
 *
 *
 * ---Non-terminal Symbol:
 *
 *
 * attribute        = tag_name '=' quoted_str
 * attributes       = (skip_space_must attribute)*
 *
 * node             = text | element
 *
 * content          = skip_space node* skip_space
 *
 * element          = '<' tag_name attributes '>' content '</' tag_name '>'
 *
 * document         = skip_space ('<?xml' .*? '?>' skip_space)* skip_space element
 */

    using namespace pkuyo::parsers;
    using namespace std;


    struct open_tag_check : public base_parser<char,open_tag_check> {
        optional<nullptr_t> parse_impl(auto& stream) const {
            if(peek_impl(stream)) {
                stream.Seek(1);
                return nullptr;
            }
            return std::nullopt;
        }
        bool peek_impl(auto& stream) const {
            if(stream.Eof(1) || stream.Peek()  != '<' ||  stream.Peek(1) == '/' )
                return false;
            return true;
        }
    };

// space
    constexpr auto space = -SingleValue<char>([](auto &&c) { return c == '\n' || c == '\r' || c == ' '; });
    constexpr auto skip_space_must = +space;
    constexpr auto skip_space = *space;


    struct lazy_element;

    constexpr auto tag_name = +SingleValue<char>([](auto c) {return isalpha(c);});

    constexpr auto quoted_str = ('"' >> Until<char>('"') >> '"') | ('\'' >> Until<char>('\'') >> '\'');

    constexpr auto attributes = *(skip_space_must >> tag_name >> '=' >> quoted_str);

    constexpr auto text = Until<char>('<');

    constexpr auto node = text | Lazy<char,lazy_element>();

    constexpr auto content = skip_space >> *node >> skip_space;

    constexpr auto open_tag = open_tag_check()  >> tag_name >> attributes >> '>';

    constexpr auto close_tag = "</" >> tag_name >> '>';

    constexpr auto element = (open_tag >> content >> close_tag >> skip_space)
                   //verify tag name
                   && [](tuple<string,vector<Attr>,vector<Node>,string> &parts) { return get<3>(parts) == get<0>(parts); }
                   //build xml element
                   >>= [](tuple<string,vector<Attr>,vector<Node>,string> &&parts) {
                auto &[tag, attrs, children, close] = parts;
                return xml::Element{std::move(tag), std::move(attrs),std::move(children)};
            };

    struct lazy_element : public base_parser<char,lazy_element> {
        optional<Element> parse_impl(auto & stream) const {
            return element.Parse(stream);
        }
        bool peek_impl(auto & stream) const {
            return element.Peek(stream);
        }
    };

    constexpr auto document = skip_space >> element;
}

int main() {


    pkuyo::parsers::file_stream xml("test.txt");


    try {
        auto result = xml::document.Parse(xml);
        if (result) {
            std::cout << "XML parsed successfully!\n";
            xml::PrintElement(result.value());
        } else {
            std::cout << "Parse failed\n";
        }
    }
    catch(pkuyo::parsers::parser_exception& e) {
        std:: cout << e.what() << std::endl;
    }



    return 0;
}