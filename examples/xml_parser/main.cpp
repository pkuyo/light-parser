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

    struct TextNode {
        std::string content;
    };

    struct Element;

    using Node = std::variant<TextNode, Element>;

    struct Element {
        std::string tag_name;
        std::vector<std::pair<std::string, std::string>> attributes;
        std::vector<Node> children;
    };

    void PrintElement(const Element& element, int indentLevel = 0) {
        std::string indent(indentLevel * 2, ' ');

        std::cout << indent << "<" << element.tag_name;

        for (const auto& attr : element.attributes) {
            std::cout << " " << attr.first << "=\"" << attr.second << "\"";
        }
        std::cout << ">\n";

        for (const auto& node : element.children) {
            if (std::holds_alternative<TextNode>(node)) {
                const TextNode& textNode = std::get<TextNode>(node);
                std::cout << indent << "  " << textNode.content << "\n";
            } else if (std::holds_alternative<Element>(node)) {

                const Element& childElement = std::get<Element>(node);
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
auto space = container.SingleValue([](auto && c) {return c == '\n' || c == '\r' || c == ' ';}).Ignore();
auto skip_space_must = space.More().Ignore();
auto skip_space = space.Many().Ignore();


auto tag_name = container.Regex("[A-Za-z_:][A-Za-z0-9_:.-]*").Name("tag_name");


auto quoted_str = (container.Check('"') >> container.Regex(R"([^"]*)") >> container.Check('"'))
                  | (container.Check('\'') >> container.Regex(R"([^']*)") >> container.Check('\''));
auto attribute = (tag_name >> container.Check('=').Ignore() >> quoted_str)
        .Map([](auto &&pair) {
            return std::make_pair(std::get<0>(pair), std::get<1>(pair));
        })
        .Name("attribute");

auto attributes = (skip_space_must >> attribute).Many()
        .Name("attributes");


auto text = container.Regex("[^<]+").Map([](auto && str) { return xml::TextNode{str}; }).Name("text");

// sub-node ( text or element )
auto node = text | container.Lazy([](){return pElement;}) ;
auto content = skip_space >> node.Many().Name("content") >> skip_space;


auto element =
        (
                (container.Regex(R"(<(?!\/))").Ignore() >> tag_name >> attributes >> container.Check('>')).Name("open_tag")
                >> content
                >> (container.SeqCheck("</") >> tag_name >> container.Check('>')).Name("close_tag") >> skip_space
        )
        //verify tag name
        .Where([](const auto &t) { return std::get<3>(t) == std::get<0>(t); })


        //build xml element
        .Map([](auto &&parts) {
            auto &[tag, attrs, children, close_tag] = parts;
            return xml::Element{tag, attrs, children};
        }).Name("element");


auto document = (skip_space >> (container.Regex(R"(<\?xml.*?\?>)") >> skip_space).Many().Ignore() >> skip_space >> element).Name("document");

pkuyo::parsers::base_parser<TokenType, xml::Element> *pElement = element.Get();


int main() {
    container.DefaultError([](auto && self,auto && token) {
        auto re = pkuyo::parsers::parser_exception(token ? std::format("{}",token.value()) : "EOF", self.Name());
        std::cerr << re.what() << std::endl;

    });
    std::string xml = R"(
    <?xml version="1.0" encoding="utf-8"?>
    <root>
        <person id="123">
            <name>John</name>
            <age>30</age>
        </person>
    </root>)";

    auto result = document.Get()->Parse(xml.begin(),xml.end());
    if (result) {
        std::cout << "XML parsed successfully!\n";
        xml::PrintElement(result.value());
    } else {
        std::cout << "Parse failed\n";
    }


    return 0;
}