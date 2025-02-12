//
// Created by pkuyo on 2025/2/12.
//
#include <gtest/gtest.h>
#include "../sax.h"

using namespace std;
using namespace pkuyo::parsers;
using namespace xml_sax;

// Mock SAX处理器实现
class MockHandler : public SAXHandler {
public:
    struct StartElementCall {
        string name;
        map<string, string> attrs;
    };

    vector<StartElementCall> startElements;
    vector<string> endElements;
    vector<string> texts;
    vector<string> errors;

    void startElement(string_view name, const map<string, string>& attrs) override {
        startElements.push_back({string(name), attrs});
    }

    void endElement(string_view name) override {
        endElements.push_back(string(name));
    }

    void characters(string_view text) override {
        texts.push_back(string(text));
    }

    void error(const string& msg) override {
        errors.push_back(msg);
    }

    bool errorsOccurred() const { return !errors.empty(); }
};

class XMLParserTest : public ::testing::Test {
protected:
    void TearDown() override {
        if (handler.errorsOccurred()) {
            for (const auto& err : handler.errors) {
                cout << "Parser error: " << err << endl;
            }
        }
    }

    MockHandler handler;
};

TEST_F(XMLParserTest, SelfClosingTag) {
    string xml = "<root/>";
    string_stream stream(xml);
    parse(stream, handler);

    ASSERT_EQ(handler.startElements.size(), 1);
    EXPECT_EQ(handler.startElements[0].name, "root");
    EXPECT_TRUE(handler.startElements[0].attrs.empty());
    ASSERT_EQ(handler.endElements.size(), 1);
    EXPECT_EQ(handler.endElements[0], "root");
    EXPECT_FALSE(handler.errorsOccurred());
}

TEST_F(XMLParserTest, AttributesParsing) {
    string xml = R"(<root attr1="value1" attr2="value2"/>)";
    string_stream stream(xml);
    parse(stream, handler);

    ASSERT_EQ(handler.startElements.size(), 1);
    const auto& attrs = handler.startElements[0].attrs;
    EXPECT_EQ(attrs.size(), 2);
    EXPECT_EQ(attrs.at("attr1"), "value1");
    EXPECT_EQ(attrs.at("attr2"), "value2");
    EXPECT_FALSE(handler.errorsOccurred());
}

TEST_F(XMLParserTest, NestedElements) {
    string xml = R"(
        <parent>
            <child/>
            <child name="test"/>
        </parent>
    )";
    string_stream stream(xml);
    parse(stream, handler);

    ASSERT_EQ(handler.startElements.size(), 3);
    EXPECT_EQ(handler.startElements[0].name, "parent");
    EXPECT_EQ(handler.startElements[1].name, "child");
    EXPECT_EQ(handler.startElements[2].name, "child");

    ASSERT_EQ(handler.endElements.size(), 3);
    EXPECT_EQ(handler.endElements[0], "child");
    EXPECT_EQ(handler.endElements[1], "child");
    EXPECT_EQ(handler.endElements[2], "parent");
    EXPECT_FALSE(handler.errorsOccurred());
}

TEST_F(XMLParserTest, TextContent) {
    string xml = "<msg>Hello&lt;World&gt;!</msg>";
    string_stream stream(xml);
    parse(stream, handler);

    ASSERT_EQ(handler.texts.size(), 1);
    EXPECT_EQ(handler.texts[0], "Hello&lt;World&gt;!");
    EXPECT_FALSE(handler.errorsOccurred());
}

TEST_F(XMLParserTest, XmlDeclaration) {
    string xml = R"(<?xml version="1.0" encoding="UTF-8"?><root/>)";
    string_stream stream(xml);
    parse(stream, handler);

    ASSERT_EQ(handler.startElements.size(), 1);
    EXPECT_EQ(handler.startElements[0].name, "root");
    EXPECT_FALSE(handler.errorsOccurred());
}

TEST_F(XMLParserTest, ErrorHandling) {
    string xml = "<root><child></root>";
    string_stream stream(xml);
    parse(stream, handler);

    EXPECT_TRUE(handler.errorsOccurred());
    EXPECT_FALSE(handler.errors.empty());
}

TEST_F(XMLParserTest, ComplexStructure) {
    string xml = R"(
        <?xml version="1.0"?>
        <catalog>
            <book id="bk101">
                <author>Gambardella, Matthew</author>
                <title>XML Developer's Guide</title>
                <price>44.95</price>
            </book>
            <book id="bk102">
                <author>Ralls, Kim</author>
                <title>Midnight Rain</title>
                <price>5.95</price>
            </book>
        </catalog>
    )";

    string_stream stream(xml);
    parse(stream, handler);

    EXPECT_EQ(handler.startElements.size(), 9);
    EXPECT_EQ(handler.endElements.size(), 9);
    EXPECT_EQ(handler.texts.size(), 6);

    EXPECT_EQ(handler.startElements[1].attrs["id"], "bk101");
    EXPECT_EQ(handler.startElements[5].attrs["id"], "bk102");

    EXPECT_EQ(handler.texts[0], "Gambardella, Matthew");
    EXPECT_EQ(handler.texts[1], "XML Developer's Guide");
    EXPECT_FALSE(handler.errorsOccurred());
}

TEST_F(XMLParserTest, EmptyDocument) {
    string xml = "";
    string_stream stream(xml);
    parse(stream, handler);

    EXPECT_EQ(handler.startElements.size(),0);
}

TEST_F(XMLParserTest, MalformedAttribute) {
    string xml = "<root attr=value/>";
    string_stream stream(xml);
    parse(stream, handler);

    EXPECT_TRUE(handler.errorsOccurred());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}