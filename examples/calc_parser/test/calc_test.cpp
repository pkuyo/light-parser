//
// Created by pkuyo on 2025/2/12.
//
#include "gtest/gtest.h"
#include "../calc_parser.h"

using namespace pkuyo::parsers;

class ArithmeticParserTest : public ::testing::Test {
protected:
    static constexpr double epsilon = 1e-6;

    double parse(const std::string& input) {
        string_stream stream(input);
        auto result = num_parser::expression.Parse(stream);
        return result.value_or(0.0);
    }
};

TEST_F(ArithmeticParserTest, BasicOperations) {
    EXPECT_NEAR(parse("2+3"), 5.0, epsilon);
    EXPECT_NEAR(parse("5-3"), 2.0, epsilon);
    EXPECT_NEAR(parse("4*3"), 12.0, epsilon);
    EXPECT_NEAR(parse("10/4"), 2.5, epsilon);
}

TEST_F(ArithmeticParserTest, OperatorPrecedence) {
    EXPECT_NEAR(parse("2+3*4"), 14.0, epsilon);
    EXPECT_NEAR(parse("(2+3)*4"), 20.0, epsilon);
    EXPECT_NEAR(parse("3*4+2"), 14.0, epsilon);
    EXPECT_NEAR(parse("3*(4+2)"), 18.0, epsilon);
}

TEST_F(ArithmeticParserTest, FloatNumbers) {
    EXPECT_NEAR(parse("3.14+2.86"), 6.0, epsilon);
    EXPECT_NEAR(parse("0.1+0.2"), 0.3, epsilon);
    EXPECT_NEAR(parse("10.5/2"), 5.25, epsilon);
    EXPECT_NEAR(parse("3.14159*2"), 6.28318, epsilon);
}

TEST_F(ArithmeticParserTest, ComplexExpressions) {
    EXPECT_NEAR(parse("3+5*2/(8-6)"), 8.0, epsilon);
    EXPECT_NEAR(parse("((3+5)*2-1)/3"), 5.0, epsilon);
    EXPECT_NEAR(parse("(4.5*(2+3))/1.5"), 15.0, epsilon);
    EXPECT_NEAR(parse("(1+2)*(3+4)/(5-2)"), 7.0, epsilon);
}


TEST_F(ArithmeticParserTest, InvalidInputs) {
    {
        string_stream stream("2+a");
        EXPECT_THROW(num_parser::expression.Parse(stream),pkuyo::parsers::parser_exception);
    }

    {
        string_stream stream("(2+3");
        EXPECT_THROW(num_parser::expression.Parse(stream),pkuyo::parsers::parser_exception);
    }

    {
        string_stream stream("");
        EXPECT_THROW(num_parser::expression.Parse(stream),pkuyo::parsers::parser_exception);
    }
}


TEST_F(ArithmeticParserTest, EdgeCases) {


    EXPECT_NEAR(parse("0.0000001 / 10"), 0.00000001, epsilon);

    string_stream stream("5/0");
    auto result = num_parser::expression.Parse(stream);
    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_TRUE(std::isinf(*result));
    }
}

// 主测试函数
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}