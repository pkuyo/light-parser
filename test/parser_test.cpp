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
        parser_error_handler<TestToken>::DefaultRecovery(panic_mode_recovery<TestToken>([](const TestToken &t) {
            return t.value == ";"; // set sync pos
        }));
    }

};

TEST_F(ParserTest, ConstexprParser) {

    constexpr auto parser = *Check<char>(' ') >> SeqValue<char,std::string>("hello") >> "::" >> SeqValue<char,std::string>("end");
    std::string tokens("  hello::end;");

    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());

    auto & [first, end] = *result;
    EXPECT_EQ(first, "hello");
    EXPECT_EQ(end, "end");
}

TEST_F(ParserTest, UntilParser) {

    constexpr auto parser = Check<char>('<') >> Until<char>('>') >> ">";
    std::string tokens("<token>");
    auto result = parser.Parse(tokens);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "token");
}

TEST_F(ParserTest, CheckParser) {
    auto parser = Check<TestToken>("keyword").Name("CheckTest");
    std::vector<TestToken> tokens = {{"keyword"},
                                     {"other"}};

    auto it = tokens.cbegin();
    EXPECT_TRUE(parser.Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "other");
}

TEST_F(ParserTest, SingleValueParser) {
    auto parser = SingleValue<TestToken,std::string,std::string>("number", [](const TestToken &token) {
        return token.value;
    });
    std::vector<TestToken> tokens = {{"number"},
                                     {"+"}};

    auto it = tokens.cbegin();
    auto result = parser.Parse(it, tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "number");
    EXPECT_EQ(it->value, "+");
}



TEST_F(ParserTest, SinglePtrParser) {
    auto parser = SinglePtr<TestToken,std::string,std::string>("number", [](const TestToken &token) {
        return std::make_unique<std::string>(token.value);
    });
    std::vector<TestToken> tokens = {{"number"},
                                     {"+"}};

    auto it = tokens.cbegin();
    auto result = parser.Parse(it, tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result.value()), "number");
    EXPECT_EQ(it->value, "+");
}

TEST_F(ParserTest, SingleValueParserWithFunc) {
    auto parser = SingleValue<TestToken>([](const TestToken & token) {return *(token.value.begin()) == 'a';})
            >>= ([](const TestToken & token) {return token.value;});
    std::vector<TestToken> tokens = {{"a_token"},
                                     {"+"}};

    auto it = tokens.cbegin();
    auto result = parser.Parse(it, tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "a_token");
    EXPECT_EQ(it->value, "+");
}

TEST_F(ParserTest, SeqParser) {
    std::vector<TestToken> test_swq= {{"number"},{"+"}};

    std::vector<TestToken> tokens = {{"number"},
                                     {"+"},
                                     {"number"}};

    auto parser = SeqCheck<TestToken,std::vector<TestToken>>(test_swq);

    auto it = tokens.cbegin();
    auto result = parser.Parse(it, tokens.cend());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(it->value, "number");
}

TEST_F(ParserTest, ThenCombination) {

    auto parser = Check<TestToken>("A") >> "B";

    std::vector<TestToken> valid = {{"A"},
                                    {"B"}};
    auto valid_it = valid.cbegin();
    EXPECT_TRUE(parser.Parse(valid_it, valid.cend()).has_value());
    EXPECT_EQ(valid_it, valid.end());

    std::vector<TestToken> invalid = {{"A"},
                                      {"C"}};
    EXPECT_THROW(parser.Parse(invalid), parser_exception);
}

TEST_F(ParserTest, OrCombination) {
    auto parser = Check<TestToken>("A") | "B";

    std::vector<TestToken> tokens1 = {{"A"}};
    EXPECT_TRUE(parser.Parse(tokens1).has_value());

    std::vector<TestToken> tokens2 = {{"B"}};
    EXPECT_TRUE(parser.Parse(tokens2).has_value());
}

TEST_F(ParserTest, ManyParser) {
    auto parser = *Check<TestToken>("num");

    std::vector<TestToken> empty = {};
    EXPECT_TRUE(parser.Parse(empty).has_value());

    std::vector<TestToken> three_nums = {{"num"},
                                         {"num"},
                                         {"num"}};
    auto result = parser.Parse(three_nums);
    ASSERT_TRUE(result.has_value());

}

TEST_F(ParserTest, SemanticActionParser) {
    std::string output;
    auto parser = SingleValue<TestToken>("num") <<= [&output] (const TestToken & t) { output = t.value;};

    std::vector<TestToken> input = {{"num"}};
    auto result = parser.Parse(input);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(output,"num");
}

TEST_F(ParserTest, MoreParser) {
    auto parser = +Check<TestToken>("num");


    std::vector<TestToken> three_nums = {{"num"},
                                         {"num"},
                                         {"num"}};
    auto result = parser.Parse(three_nums);
    ASSERT_TRUE(result.has_value());

    std::vector<TestToken> invalid = {{"invalid"}};
    EXPECT_THROW(parser.Parse(invalid), parser_exception);
}

TEST_F(ParserTest, ErrorRecovery) {
    auto parser = Check<TestToken>("expected").Name("ErrorTest");
    std::vector<TestToken> tokens = {{"unexpected"},
                                     {";"},
                                     {"valid"}};

    auto it = tokens.cbegin();
    EXPECT_THROW(parser.Parse(it, tokens.cend()), parser_exception);
    EXPECT_EQ(it->value, ";");
}

//TEST_F(ParserTest, LazyParser) {
//    auto expr = builder.Lazy([this]() {
//        return builder.Empty().Get();
//    }).Name("LazyTest");
//
//
//    std::vector<TestToken> tokens = {{"any"}};
//    auto it = tokens.cbegin();
//    EXPECT_TRUE(expr->Parse(it, tokens.cend()).has_value());
//}

TEST_F(ParserTest, OptionalCheckParser) {
    auto expr = ~Check<TestToken>("optional");

    std::vector<TestToken> tokens = {{"any"}};
    auto it = tokens.cbegin();
    EXPECT_TRUE(expr.Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "any");

    tokens = {{"optional"},
              {"any"}};
    it = tokens.cbegin();
    EXPECT_TRUE(expr.Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "any");
}

TEST_F(ParserTest, OptionalSingleParser) {
    auto expr = ~SingleValue<TestToken>("optional");

    std::vector<TestToken> tokens = {{"any"}};
    auto it = tokens.cbegin();
    EXPECT_TRUE(expr.Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "any");

    tokens = {{"optional"},
              {"any"}};
    it = tokens.cbegin();
    EXPECT_TRUE(expr.Parse(it, tokens.cend()).has_value());
    EXPECT_EQ(it->value, "any");
}

TEST_F(ParserTest, MapParser) {
    auto num = SingleValue<TestToken,std::string,int>("num", [](const TestToken &t) {
        return 1;
    });
    auto parser = num >>= ([](int v) { return v * 1.5; });

    std::vector<TestToken> tokens = {{"num"}};
    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1.5);
}



TEST_F(ParserTest, NotParser) {

    auto parser = !Str<char>("No") >> -~Str<char>("No") >> Str<char>("Yes");
    std::string tokens("Yes");
    std::string no_tokens("NoYes");

    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "Yes");

    EXPECT_THROW(parser.Parse(no_tokens), parser_exception);


}


TEST_F(ParserTest, StringParser) {

    auto parser = (Str<char>("key") | Str<char>("word")) >> "::" >>  Regex<char>(R"([^;]*;)");
    std::string tokens("key::end;");

    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());

    auto & [first, end] = *result;
    EXPECT_EQ(first, "key");
    EXPECT_EQ(end, "end;");
}

TEST_F(ParserTest, ComplexExpression) {
    auto num = SingleValue<TestToken,std::string,int>("num", [](const TestToken &t) {
        return 12;
    });
    auto add = Check<TestToken>("+");
    auto mul = Check<TestToken>("*");

    auto factor = num | (Check<TestToken>("(") >> num >> Check<TestToken>(")"));

    auto term = factor >> *(mul >> factor);
    auto expr = term >> *(add >> term);

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
    auto result = expr.Parse(it, tokens.cend());
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(it, tokens.cend());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}