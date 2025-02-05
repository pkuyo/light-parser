//
// Created by pkuyo on 2025/2/4.
//

#include "parser.h"
#include <string>
#include <vector>
#include <variant>
#include <iostream>

using TokenType = char;

namespace xml {



    struct Element;

    using Node = std::variant<std::string,Element>;

    struct Element {
        std::string tag_name;
        std::vector<std::tuple<std::string, std::string>> attributes;
        std::vector<Node> children;
    };

    void PrintElement(const Element& element, int indentLevel = 0) {
        std::string indent(indentLevel * 2, ' ');

        std::cout << indent << "<" << element.tag_name;

        for (const auto& attr : element.attributes) {
            std::cout << " " << std::get<0>(attr) << "=\"" << std::get<1>(attr) << "\"";
        }
        std::cout << ">\n";

        for (const auto& node : element.children) {
            if (std::holds_alternative<std::string>(node)) {
                const auto& textNode = std::get<std::string>(node);
                std::cout << indent << "  " << textNode << "\n";
            } else if (std::holds_alternative<Element>(node)) {

                const auto& childElement = std::get<Element>(node);
                PrintElement(childElement, indentLevel + 1);
            }
        }
        std::cout << indent << "</" << element.tag_name << ">\n";
    }

}

pkuyo::parsers::parser_container<TokenType> container;
extern pkuyo::parsers::base_parser<TokenType, xml::Element> *pElement;

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


// space
auto space = container.SingleValue([](auto && c) {return c == '\n' || c == '\r' || c == ' ';});
auto skip_space_must = -+space;
auto skip_space = -*space;


auto tag_name = container.Regex("[A-Za-z_:][A-Za-z0-9_:.-]*");

auto quoted_str = ( '"' >> container.Regex(R"([^"]*)") >> '"' ) | ('\'' >> container.Regex(R"([^']*)") >> '\'');

auto attributes = *(skip_space_must >> tag_name >> '=' >> quoted_str);

auto text = container.Regex("[^<]+");

auto node =  text | container.Lazy([](){return pElement;});

auto content = skip_space >> *node >> skip_space;

auto open_tag = -container.Regex(R"(<(?!\/))") >> tag_name >> attributes >> '>';

auto close_tag =  "</" >> tag_name >> '>' ;

auto element =  (open_tag >> content>> close_tag>> skip_space)
        //verify tag name
         & [](const auto &t) { return std::get<3>(t) == std::get<0>(t); }
        //build xml element
         >>= [](auto &&parts) {
            auto &[tag, attrs, children, close] = parts;
            return xml::Element{tag, attrs, children};
        };


auto document = skip_space >> -*(container.Regex(R"(<\?xml.*?\?>)") >> skip_space) >> skip_space >> element;

pkuyo::parsers::base_parser<TokenType, xml::Element> *pElement = element.Get();


int main() {
    container.DefaultError([](auto && self,auto && current, auto && end) {
        throw pkuyo::parsers::parser_exception(current != end ? std::format("{}",std::string(current,end)) : "EOF", self.Name());
    });
    std::string xml = R"(
    <?xml version="1.0" encoding="utf-8"?>
    <root>
        <person id="123">
            <name>John</name>
            <age>30</age>
        </person>
    </root>)";

    try {
        auto result = document.Get()->Parse(xml.begin(), xml.end());
        if (result) {
            std::cout << "XML parsed successfully!\n";
            xml::PrintElement(result.value());
        } else {
            std::cout << "Parse failed\n";
        }
    }
    catch(const pkuyo::parsers::parser_exception & e) {
        std::cout << "Parse failed\n";
        std::cerr << e.what() << std::endl;
    }


    return 0;
}