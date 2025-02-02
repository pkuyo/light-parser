//
// Created by pkuyo on 2025/1/31.
//
#include "gtest/gtest.h"
#include "parser.h"

using namespace pkuyo::parsers;

struct TestToken {
    std::string value;

    bool operator==(const TestToken &other) const {
        return value == other.value;
    }


    friend bool operator==(const TestToken &self, const std::string_view &other) {
        return self.value == other;
    }


    friend bool operator!=(const TestToken &self, const std::string_view &other) {
        return self.value != other;
    }

};

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        builder.DefaultRecovery([](const TestToken &t) {
            return t.value == ";"; // set sync pos
        });
    }

    parser_container<TestToken> builder;
};

TEST_F(ParserTest, CheckParser) {
    auto parser = builder.Check("keyword").Name("CheckTest");
    std::vector<TestToken> tokens = {{"keyword"},
                                     {"other"}};

    auto it = tokens.cbegin();
    EXPECT_TRUE(parser->Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "other");
}

TEST_F(ParserTest, SingleValueParser) {
    auto parser = builder.SingleValue<std::string>("number", [](const TestToken &token) {
        return token.value;
    });
    std::vector<TestToken> tokens = {{"number"},
                                     {"+"}};

    auto it = tokens.cbegin();
    auto result = parser->Parse(it, tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "number");
    EXPECT_EQ(it->value, "+");
}

TEST_F(ParserTest, SinglePtrParser) {
    auto parser = builder.SinglePtr<std::string>("number", [](const TestToken &token) {
        return std::make_unique<std::string>(token.value);
    });
    std::vector<TestToken> tokens = {{"number"},
                                     {"+"}};

    auto it = tokens.cbegin();
    auto result = parser->Parse(it, tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result.value()), "number");
    EXPECT_EQ(it->value, "+");
}

TEST_F(ParserTest, SingleValueParserWithFunc) {
    auto parser = builder
            .SingleValue([](const auto & token) {return *(token.value.begin()) == 'a';})
            .Map([](const auto & token) {return token.value;});
    std::vector<TestToken> tokens = {{"a_token"},
                                     {"+"}};

    auto it = tokens.cbegin();
    auto result = parser->Parse(it, tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "a_token");
    EXPECT_EQ(it->value, "+");
}


TEST_F(ParserTest, ThenCombination) {
    auto a = builder.Check("A");
    auto b = builder.Check("B");
    auto parser = a >> b;

    std::vector<TestToken> valid = {{"A"},
                                    {"B"}};
    auto valid_it = valid.cbegin();
    EXPECT_TRUE(parser->Parse(valid_it, valid.cend()).has_value());
    EXPECT_EQ(valid_it, valid.end());

    std::vector<TestToken> invalid = {{"A"},
                                      {"C"}};
    EXPECT_THROW(parser->Parse(invalid.cbegin(), invalid.cend()), parser_exception);
}

TEST_F(ParserTest, OrCombination) {
    auto a = builder.Check("A");
    auto b = builder.Check("B");
    auto parser = a | b;

    std::vector<TestToken> tokens1 = {{"A"}};
    EXPECT_TRUE(parser->Parse(tokens1.cbegin(), tokens1.cend()).has_value());

    std::vector<TestToken> tokens2 = {{"B"}};
    EXPECT_TRUE(parser->Parse(tokens2.cbegin(), tokens2.cend()).has_value());
}

TEST_F(ParserTest, ManyParser) {
    auto num = builder.Check("num");
    auto parser = num.Many();

    std::vector<TestToken> empty = {};
    EXPECT_TRUE(parser->Parse(empty.cbegin(), empty.cend()).has_value());

    std::vector<TestToken> three_nums = {{"num"},
                                         {"num"},
                                         {"num"}};
    auto result = parser->Parse(three_nums.cbegin(), three_nums.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 3);

}

TEST_F(ParserTest, SemanticActionParser) {
    std::string output;
    auto parser = builder.SingleValue("num") >>= [&output] (auto && t) { output = t.value;};

    std::vector<TestToken> input = {{"num"}};
    auto result = parser->Parse(input.cbegin(), input.cend());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(output,"num");
}

TEST_F(ParserTest, MoreParser) {
    auto num = builder.Check("num");
    auto parser = num.More();


    std::vector<TestToken> three_nums = {{"num"},
                                         {"num"},
                                         {"num"}};
    auto result = parser->Parse(three_nums.cbegin(), three_nums.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 3);

    std::vector<TestToken> invalid = {{"invalid"}};
    EXPECT_THROW(parser->Parse(invalid.cbegin(), invalid.cend()), parser_exception);
}

TEST_F(ParserTest, ErrorRecovery) {
    auto parser = builder.Check("expected").Name("ErrorTest");
    std::vector<TestToken> tokens = {{"unexpected"},
                                     {";"},
                                     {"valid"}};

    auto it = tokens.cbegin();
    EXPECT_THROW(parser->Parse(it, tokens.cend()), parser_exception);
    EXPECT_EQ(it->value, ";");
}

TEST_F(ParserTest, LazyParser) {
    auto expr = builder.Lazy([this]() {
        return builder.Empty().Get();
    }).Name("LazyTest");

    std::vector<TestToken> tokens = {{"any"}};
    auto it = tokens.cbegin();
    EXPECT_TRUE(expr->Parse(it, tokens.cend()).has_value());
}

TEST_F(ParserTest, OptionalCheckParser) {
    auto expr = builder.Check("optional").Optional();

    std::vector<TestToken> tokens = {{"any"}};
    auto it = tokens.cbegin();
    EXPECT_TRUE(expr->Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "any");

    tokens = {{"optional"},
              {"any"}};
    it = tokens.cbegin();
    EXPECT_TRUE(expr->Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "any");
}

TEST_F(ParserTest, OptionalSingleParser) {
    auto expr = builder.SingleValue("optional").Optional();

    std::vector<TestToken> tokens = {{"any"}};
    auto it = tokens.cbegin();
    EXPECT_TRUE(expr->Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "any");

    tokens = {{"optional"},
              {"any"}};
    it = tokens.cbegin();
    EXPECT_TRUE(expr->Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "any");
}

TEST_F(ParserTest, MapParser) {
    auto num = builder.SingleValue<int>("num", [](const TestToken &t) {
        return 1;
    });
    auto parser = num.Map([](int v) { return v * 1.5; });

    std::vector<TestToken> tokens = {{"num"}};
    auto result = parser->Parse(tokens.cbegin(), tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1.5);
}

TEST_F(ParserTest, StringParser) {
    auto str_builder = pkuyo::parsers::parser_container<char>();
    auto parser = (str_builder.Str("key") | str_builder.Str("word")) >> str_builder.Str("end");
    std::string tokens("keyend");

    auto result = parser->Parse(tokens.cbegin(), tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "key");
    EXPECT_EQ(result->second, "end");
}

TEST_F(ParserTest, ComplexExpression) {
    auto num = builder.SingleValue<int>("num", [](const TestToken &t) {
        return std::atoi(t.value.c_str());
    });
    auto add = builder.Check("+");
    auto mul = builder.Check("*");

    auto factor = num | (builder.Check("(") >> num >> builder.Check(")"));

    auto term = factor >> (mul >> factor).Many();
    auto expr = term >> (add >> term).Many();

// (5)*3+2
    std::vector<TestToken> tokens = {
            {"("},
            {"num"},
            {")"},
            {"*"},
            {"num"},
            {"+"},
            {"num"}
    };
    auto it = tokens.cbegin();
    auto result = expr->Parse(it, tokens.cend());
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(it, tokens.cend());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}