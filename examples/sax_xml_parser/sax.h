//
// Created by pkuyo on 2025/2/11.
//

#ifndef LIGHT_PARSER_SAX_H
#define LIGHT_PARSER_SAX_H

#include "compile_time_parser.h"
#include "token_stream.h"
#include <map>

namespace xml_sax {

    using namespace pkuyo::parsers;
    using namespace std;


    class SAXHandler {
    public:
        virtual ~SAXHandler() = default;
        virtual void startElement(string_view name, const map<string, string>& attrs) = 0;
        virtual void endElement(string_view name) = 0;
        virtual void characters(string_view text) = 0;
        virtual void error(const string& msg) = 0;
    };


    struct lazy_element;

    struct tag_state {
        string name;
    };

    struct open_tag_check : public base_parser<char,open_tag_check> {
        optional<nullptr_t> parse_impl(auto& stream,auto&,auto&) const {
            if(peek_impl(stream)) {
                stream.Seek(1);
                return nullptr;
            }
            return nullopt;
        }
        bool peek_impl(auto& stream) const {
            if(stream.Eof(1) || stream.Peek()  != '<' ||  stream.Peek(1) == '/' )
                return false;
            return true;
        }
    };

    constexpr auto name = +SingleValue<char>([](char c) {return isalpha(c) || c == '_' || c == ':';})
            .Name("name");

    constexpr auto whitespace  = *Check<char>([](char c){return isspace(c) ;}).Name("whitespace");

    constexpr auto attr_value = '"' >> Until<char>('\"').Name("attr_value") >> '"';

    constexpr auto attribute = (name >> whitespace >> '=' >> whitespace >> attr_value).Name("attribute");


    constexpr auto attributes = *(attribute >> whitespace)
            >>= [](auto&& attrs,auto&, auto&) {
                    map<string, string> map;
                    for (auto& [k, v] : attrs) map[k] = v;
                    return map;
                };

    constexpr auto self_close = SeqCheck<char>("/>").Name("self_close")
            >>= [](auto , auto& handler, auto & self_close_state)  {
                    handler.endElement(self_close_state.name);
                    return nullptr;
                };


    constexpr auto open_tag = ((open_tag_check() >> name >> whitespace >> attributes).Name("open_tag")
            >>= [](auto&& tuple,auto& handler,auto & self_close_state) {
                    auto& [name, attrs] = tuple;
                    self_close_state.name = name;
                    handler.startElement(name, attrs);
                    return nullptr;
                });


    constexpr auto close_tag = ("</" >> name >> ">").Name("close_tag")
            >>= [](auto&& name,auto& handler,auto &) {
                    handler.endElement(name);
                    return nullptr;
                };



    constexpr auto content = Until<char>('<').Name("content")
            >>= [](auto&& content,auto& handler, auto state) {
                    handler.characters(content);
                    return nullptr;
                };

    constexpr auto node = TryCatch(
              Lazy<char,lazy_element>() | content,
            Sync<char>('<'),
            [](auto&& ex, auto && handler) {handler.error(ex.what());}
            ) >> whitespace;

    constexpr auto element = WithState<tag_state>(open_tag >> ('>' >> whitespace >> *node >> close_tag | self_close));

    constexpr auto root = whitespace >> *("<?xml" >> -Until<char>('?') >> "?>" >> whitespace) >> element;


    struct lazy_element : public base_parser<char,lazy_element> {
        optional<nullptr_t> parse_impl(auto & stream,auto& g_ctx,auto& ctx) const {
            return element.Parse(stream,g_ctx,ctx);
        }
        bool peek_impl(auto & stream) const {
            return element.Peek(stream);
        }
    };
    template<typename Stream,typename Handler>
    requires is_base_of_v<SAXHandler,Handler>
    void parse(Stream& stream, Handler& handler) {

        if (!root.Parse(stream, handler).has_value()) {
            handler.error("Invalid XML document");
        }
    }


} // namespace xml_sax
#endif //LIGHT_PARSER_SAX_H
