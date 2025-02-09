//
// Created by pkuyo on 2025/2/7.
//

#ifndef LIGHT_PARSER_XML_DEF_H
#define LIGHT_PARSER_XML_DEF_H
#include <string>
#include <vector>
#include <variant>
#include <iostream>
namespace xml {

    struct Element;

    using Node = std::variant<std::string,std::shared_ptr<Element>>;
    using Attr = std::tuple<std::string, std::string>;

    struct Element {
        std::string tag_name;
        std::vector<Attr> attributes;
        std::vector<Node> children;

        Element(std::string_view name) :tag_name(name) {}
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
            } else if (std::holds_alternative<std::shared_ptr<Element>>(node)) {

                const auto& childElement = *std::get<std::shared_ptr<Element>>(node);
                PrintElement(childElement, indentLevel + 1);
            }
        }
        std::cout << indent << "</" << element.tag_name << ">\n";
    }

}

#endif //LIGHT_PARSER_XML_DEF_H
