//
// Created by pkuyo on 2025/1/31.
//
#include "gtest/gtest.h"
#include "pkuyo/parser.h"
#include <cctype>

using namespace pkuyo::parsers;

struct TestToken {
    std::string value;

    TestToken(std::string&& str) : value(std::move(str)) {}
    TestToken(){}

    operator std::string() const {
        return value;
    }

    friend bool operator==(const TestToken &left, const TestToken &right) {
        return left.value == right.value;
    }



};

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {}

};


TEST_F(ParserTest, CheckParser) {
    constexpr auto parser = Check<char>('A');
    constexpr auto parser2 = Check<char>(&isupper);

    string_stream stream("AB");
    EXPECT_TRUE(parser.Parse(stream).has_value());

    string_stream stream1("C");
    EXPECT_THROW(parser.Parse(stream1),parser_exception);

    string_stream stream2("A");
    EXPECT_TRUE(parser2.Parse(stream2).has_value());

    string_stream stream3("a");
    EXPECT_THROW(parser2.Parse(stream3),parser_exception);

}

TEST_F(ParserTest, SingleValueParser) {
    constexpr auto parser = SingleValue<char>('c');

    string_stream stream("cd");
    string_stream stream2("e");

    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 'c');
    EXPECT_EQ(stream.Peek(), 'd');
    EXPECT_THROW(parser.Parse(stream2),parser_exception);

}



TEST_F(ParserTest, SinglePtrParser) {
    auto parser = SinglePtr<char>('1');

    string_stream stream("1+");
    string_stream stream2("2");

    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result.value()),'1');
    EXPECT_EQ(stream.Peek(), '+');
    EXPECT_THROW(parser.Parse(stream2),parser_exception);
}

TEST_F(ParserTest, SingleValueParserWithFunc) {
    constexpr auto parser = SingleValue<char>(&isdigit);

    string_stream stream("1+");
    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, '1');
    EXPECT_EQ(stream.Peek(), '+');

    string_stream stream2("a+");
    EXPECT_THROW(parser.Parse(stream2),parser_exception);

}

TEST_F(ParserTest, SinglePtrParserWithFunc) {
    constexpr auto parser = SinglePtr<char>(&isdigit);

    string_stream stream("1+");
    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result.value(), '1');
    EXPECT_EQ(stream.Peek(), '+');

    string_stream stream2("a+");
    EXPECT_THROW(parser.Parse(stream2),parser_exception);
}
TEST_F(ParserTest, SeqParser) {
    std::vector<TestToken> test_swq= {{"number"},{"+"}};

    std::vector<TestToken> tokens = {{"number"},
                                     {"+"},
                                     {"number"}};

    auto parser = SeqCheck<TestToken,std::vector<TestToken>>(test_swq);

    auto stream = ContainerStream(tokens);
    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(stream.Peek().value, "number");
}

TEST_F(ParserTest, ThenCombination) {

    constexpr auto parser = SingleValue<char>('A') >> 'B';

    string_stream valid("AB");

    EXPECT_TRUE(parser.Parse(valid).has_value());
    EXPECT_TRUE(valid.Eof());

    string_stream invalid("AC");

    EXPECT_THROW(parser.Parse(invalid), parser_exception);
}

TEST_F(ParserTest, OrCombination) {
    constexpr auto parser = SingleValue<char>('A') | SingleValue<char>('B');

    string_stream stream1("AB");
    EXPECT_TRUE(parser.Parse(stream1).has_value());

    string_stream stream2("B");
    EXPECT_TRUE(parser.Parse(stream2).has_value());
}

TEST_F(ParserTest, UntilParser) {

    constexpr auto parser = Check<char>('<') >> Until<char>('>') >> ">";
    string_stream tokens("<token>");
    auto result = parser.Parse(tokens);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "token");
}

TEST_F(ParserTest, ManyParser) {
    constexpr auto parser = *Check<char>('a');

    string_stream empty_stream("");
    EXPECT_TRUE(parser.Parse(empty_stream).has_value());

    string_stream stream("aaa");
    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());

}

TEST_F(ParserTest, SemanticActionParser) {
    std::string output;
    auto parser = SingleValue<TestToken>(TestToken("num"))
            <<= [&output] (const TestToken & t) { output = t.value;};

    std::vector<TestToken> input = {{"num"}};
    auto stream = ContainerStream(input);
    auto result = parser.Parse(stream);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(output,"num");
}

TEST_F(ParserTest, MoreParser) {
    constexpr auto parser = +Check<char>('a');


    string_stream stream("aaa");
    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());

    string_stream empty_stream("");
    EXPECT_THROW(parser.Parse(empty_stream), parser_exception);
}

TEST_F(ParserTest, RepeatParser) {
    constexpr auto parser = Check<char>('a') % 3;


    string_stream stream("aaa");
    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());

    string_stream stream2("aa");
    EXPECT_THROW(parser.Parse(stream2), parser_exception);
}

TEST_F(ParserTest, OptionalCheckParser) {
    auto expr = ~Check<TestToken>(TestToken("optional"));

    std::vector<TestToken> tokens = {{"any"}};
    auto stream = ContainerStream(tokens);
    EXPECT_TRUE(expr.Parse(stream).has_value());
    EXPECT_EQ(stream.Peek().value, "any");

    tokens = {{"optional"},
              {"any"}};
    stream = ContainerStream(tokens);
    EXPECT_TRUE(expr.Parse(stream).has_value());
    EXPECT_EQ(stream.Peek().value, "any");
}

TEST_F(ParserTest, OptionalSingleParser) {
    auto expr = ~SingleValue<TestToken>(TestToken("optional"));

    std::vector<TestToken> tokens = {{"any"}};
    auto stream = ContainerStream(tokens);
    EXPECT_TRUE(expr.Parse(stream).has_value());
    EXPECT_EQ(stream.Peek().value, "any");

    tokens = {{"optional"},
              {"any"}};
    stream = ContainerStream(tokens);
    EXPECT_TRUE(expr.Parse(stream).has_value());
    EXPECT_EQ(stream.Peek().value, "any");
}

TEST_F(ParserTest, WhereParser) {
    constexpr auto num = +SingleValue<char>(&isdigit) >>= [](auto && t) {return std::stoi(t);};
    constexpr auto parser = num && [](auto && re) {return re == 10;};


    string_stream stream("10");
    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());

    string_stream stream2("11");
    EXPECT_THROW(parser.Parse(stream2), parser_exception);

}

TEST_F(ParserTest, MapParser) {
    constexpr auto num = +SingleValue<char>(&isdigit) >>= [](auto && t) {return std::stoi(t);};
    constexpr auto parser = num >>= [](auto t) {return t*1.5;};

    string_stream stream("10");
    auto result = parser.Parse(stream);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 15);
}


TEST_F(ParserTest, NotParser) {
    constexpr auto parser = !Str<char>("No") >> -~Str<char>("No") >> Str<char>("Yes");
    string_stream tokens("Yes");
    string_stream no_tokens("NoYes");

    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "Yes");

    EXPECT_THROW(parser.Parse(no_tokens), parser_exception);
}

TEST_F(ParserTest, PredTest) {
    constexpr auto parser = &SeqCheck<char>("Yes") >> Str<char>("Yes");
    string_stream tokens("Yes");
    string_stream no_tokens("NoYes");

    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "Yes");
    EXPECT_THROW(parser.Parse(no_tokens), parser_exception);

}

TEST_F(ParserTest, StateParser) {
    constexpr auto parser = WithState<int>(*(SeqCheck<char>("Yes")
            <<= [](auto&&,auto& g_state,auto & state) {state++;g_state+=state;}));

    string_stream tokens("YesYesYes");
    int global_state = 0;
    auto result = parser.Parse(tokens,global_state);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(global_state, 6);
}

struct LazyTest;
constexpr auto lazy_parser =
        (SingleValue<char>(&isdigit) >>=[](auto && t){return t - '0';}) >> Lazy<char, LazyTest>()
            >>= [](auto && t) {
                return std::get<0>(t) + std::get<1>(t);
            };

struct LazyTest : base_parser<char,LazyTest> {
    std::optional<int> parse_impl(auto& stream,auto g_ctx,auto ctx) const {
         if(peek_impl(stream))
            return lazy_parser.Parse(stream,g_ctx,ctx);
         return std::make_optional(0);
    }
    bool peek_impl(auto& stream) const {
        return lazy_parser.Peek(stream);
    }
};

TEST_F(ParserTest, LazyParser) {

    string_stream tokens("123");
    auto result = lazy_parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 6);


}
#if !defined(__GNUC__)  || defined(__clang__)

TEST_F(ParserTest, SelfLazyParser) {
    static constexpr auto parser = Lazy<char,int>([](auto && self) {
        return ((SingleValue<char>(&isdigit) >>=[](auto && t){return t - '0';}) >> ~self) >>= [](auto && t) {
            if(std::get<1>(t))
                return std::get<0>(t) + *std::get<1>(t);
            return std::get<0>(t);
        };
    });
    string_stream tokens("123");
    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 6);
}
#endif
TEST_F(ParserTest, StringParser) {

    constexpr auto parser = (Str<char>("key") | Str<char>("word")) >> "::" >>  Until<char>(';');
    string_stream tokens("key::end;");

    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());

    auto & [first, end] = *result;
    EXPECT_EQ(first, "key");
    EXPECT_EQ(end, "end");
}

TEST_F(ParserTest, TryCatchParser) {

    auto parser = TryCatch(Str("com-Try"),Str("com-Recovery"));
    string_stream tokens("com-Recovery");

    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(*result, "com-Recovery");
}

TEST_F(ParserTest, BackTrack) {
    auto parser = Or_BackTrack(Str("abcdd"),Str("abcce"), Str("abccd"));
    string_stream tokens("abccd");

    auto result = parser.Parse(tokens);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "abccd");
}
TEST_F(ParserTest, ErrorRecovery) {
    auto parser = TryCatch(Check<TestToken>(TestToken("expected")),
            Sync(TestToken(";"))).Name("ErrorTest");
    std::vector<TestToken> tokens = {{"unexpected"},
                                     {";"},
                                     {"valid"}};

    auto stream = ContainerStream(tokens);
    parser.Parse(stream);
    EXPECT_EQ(stream.Peek().value, ";");
}

TEST_F(ParserTest, ArithmeticExpression) {
    constexpr auto number = (+SingleValue<char>(&isdigit))
            >>= [](auto && t) {return std::stoi(t);};
    constexpr auto op = SingleValue<char>('+') | SingleValue<char>('-');
    constexpr auto expr = number >> op >> number;

    string_stream stream("123+456");
    auto result = expr.Parse(stream);

    ASSERT_TRUE(result.has_value());

    auto& [n1, re_op, n2] = *result;

    EXPECT_EQ(n1, 123);
    EXPECT_EQ(re_op, '+');
    EXPECT_EQ(n2, 456);
}

TEST_F(ParserTest, JsonStringParser) {
    constexpr auto json_string = Check<char>('"') >>
                                        Until<char>('"') >>
                                        Check<char>('"');

    string_stream stream(R"("hello world")");
    auto result = json_string.Parse(stream);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result,"hello world");
}

TEST_F(ParserTest, FileStreamParsing) {
    std::ofstream tmp("test.tmp");
    tmp << "abccd";
    tmp.close();

    file_stream stream("test.tmp");
    auto parser = Or_BackTrack(Str("abcdd"),Str("abcce"), Str("abccd"));

    auto result = parser.Parse(stream);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "abccd");
    std::remove("test.tmp");
    EXPECT_THROW(file_stream("test1.tmp"),std::runtime_error);
}


TEST_F(ParserTest, MMAPStreamParsing) {
    std::ofstream tmp("test.tmp");
    tmp << "abccd";
    tmp.close();

    mmap_file_stream stream("test.tmp");
    auto parser = Or_BackTrack(Str("abcdd"),Str("abcce"), Str("abccd"));

    auto result = parser.Parse(stream);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "abccd");
    std::remove("test.tmp");
    EXPECT_THROW(file_stream("test1.tmp"),std::runtime_error);

}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
