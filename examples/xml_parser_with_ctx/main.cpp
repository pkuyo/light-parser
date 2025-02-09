//
// Created by pkuyo on 2025/2/4.
//

#include "parser.h"
#include <string>
#include <vector>
#include <variant>
#include <iostream>
#include <stack>
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
    using XmlStack = std::stack<std::shared_ptr<xml::Element>>;


    struct open_tag_check : public base_parser<char,open_tag_check> {
        optional<nullptr_t> parse_impl(auto& stream,auto&,auto&) const {
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
    constexpr auto skip_space = *space;


    struct lazy_element;

    constexpr auto tag_name = +SingleValue<char>([](auto c) {return isalpha(c);});

    constexpr auto quoted_str = ('"' >> Until<char>('"') >> '"') | ('\'' >> Until<char>('\'') >> '\'');

    constexpr auto attributes = * ( (tag_name >> '=' >> quoted_str >> skip_space)
            >>= [] (tuple<std::string, std::string> && part, XmlStack & ctx) {
                ctx.top()->attributes.emplace_back(std::move(part));
                return nullptr;
    });

    constexpr auto text = Until<char>('<');

    constexpr auto node =
            (text >>= [](const string &str, XmlStack &ctx) {
                    ctx.top()->children.emplace_back(str);
                return nullptr;
            }) | Lazy<char, lazy_element>();

    constexpr auto content = skip_space >> *node >> skip_space;

    constexpr auto open_tag = open_tag_check()
            >> (tag_name >>= [](const string & str, XmlStack & ctx){

                auto new_element = make_shared<Element>(str);
                if(!ctx.empty())
                    ctx.top()->children.emplace_back(new_element);
                ctx.push(new_element);
                return nullptr;
            })
            >> skip_space >> attributes;

    constexpr auto close_tag = "</"
            >> -(tag_name && [](const string & str, XmlStack & ctx) {
                auto result = ctx.top()->tag_name == str;
                if(ctx.size() != 1)
                    ctx.pop();
                return result;
            })
            >> '>';

    constexpr auto element = open_tag >>
            (   (SeqCheck<char>("/>" )  <<= [](nullptr_t,XmlStack & ctx) { ctx.pop();})
                | '>' >> content >> close_tag)
            >> skip_space;

    struct lazy_element : public base_parser<char,lazy_element> {
        optional<nullptr_t> parse_impl(auto & stream,auto& g_ctx,auto& ctx) const {
            return element.Parse(stream,g_ctx,ctx);
        }
        bool peek_impl(auto & stream) const {
            return element.Peek(stream);
        }
    };

    constexpr auto document = skip_space >> element;
}

int main() {

    xml::XmlStack element_stack;
    pkuyo::parsers::file_stream xml("test.txt");


    try {
        auto result = xml::document.Parse(xml,element_stack);
        if (result) {
            std::cout << "XML parsed successfully!\n";
            xml::PrintElement(*element_stack.top());
        } else {
            std::cout << "Parse failed\n";
        }
    }
    catch(pkuyo::parsers::parser_exception& e) {
        std:: cout << e.what() << std::endl;
    }



    return 0;
}