//
// Created by pkuyo on 2025/2/11.
//
#include <iostream>
#include "sax.h"

class MyHandler : public xml_sax::SAXHandler {
public:
    void startElement(std::string_view name,
                      const std::map<std::string, std::string>& attrs) override {
        std::cout << "Start: " << name << "\n";
        for (auto& [k, v] : attrs) {
            std::cout << "  " << k << "=" << v << "\n";
        }
    }

    void endElement(std::string_view name) override {
        std::cout << "End: " << name << "\n";
    }

    void characters(std::string_view text) override {
        if (!text.empty()) {
            std::cout << "Text: " << text << "\n";
        }
    }

    void error(const std::string& msg) override {
        std::cerr << "Error: " << msg << std::endl;
    }
};

int main() {
    MyHandler handler;

    pkuyo::parsers::string_stream wrong(R"(
        <book id="123">
            <title>Modern C++</title>
            <empty></empty>
            <author>
                <name>John Doe</name>
                <email>john@example.com</email>
            </author>
            <<pricecurrency="USD">59.99</price>
        </book>
    )","wrong");

    pkuyo::parsers::string_stream correct(R"(
        <book id="123">
            <title>Modern C++</title>
            <empty></empty>
            <author>
                <name>John Doe</name>
                <email>john@example.com</email>
            </author>
            <price currency="USD">59.99</price>
        </book>
    )","correct");

    xml_sax::parse(wrong,handler);
    std::cout << "-----------------------" << std::endl;
    xml_sax::parse(correct,handler);
    return 0;
}