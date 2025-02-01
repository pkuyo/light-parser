//
// Created by pkuyo on 2025/2/1.
//

#include "parser.h"

#include "lexer.h"
#include "ast_node.h"

/*
 *   JSON       = Value ;
 *   Value      = Object | Array | STRING | NUMBER | TRUE | FALSE | NULL ;
 *   Object     = LBRACE [ Members ] RBRACE ;
 *   Members    = Pair { COMMA Pair } ;
 *   Pair       = STRING COLON Value ;
 *   Array      = LBRACKET [ Elements ] RBRACKET ;
 *   Elements   = Value { COMMA Value } ;
 *
*/

int main() {



    auto container = pkuyo::parsers::parser_container<Token>();


    //Terminal Symbol

    auto number = container.SinglePtr<NumberNode>(TokenType::NUMBER);

    auto string = container.SinglePtr<StringNode>(TokenType::STRING);

    //bool
    auto true_false = container.SinglePtr<BoolNode>(TokenType::FALSE) | container.SinglePtr<BoolNode>(TokenType::TRUE);

    auto null = container.SinglePtr<NullNode>(TokenType::NULL_);

    auto comma = container.Check(TokenType::COMMA);

    //Non-terminal Symbol

    pkuyo::parsers::parser_wrapper<Token, std::unique_ptr<AstNode>> *pValue = nullptr;

    auto lazy_value = container.Lazy([&]() { return pValue->parser; });

    auto elements = (lazy_value >> (comma >> lazy_value).Many()).Map([](auto &&t) {
        t.second.emplace_back(std::move(t.first));
        return std::move(t.second);
    });


    auto array = (container.Check(TokenType::LBRACKET)[elements] >> container.Check(TokenType::RBRACKET)).Map(
            [](auto &&t) {
                if (!t) return std::make_unique<ArrayNode>(std::vector<std::unique_ptr<AstNode>>());
                return std::make_unique<ArrayNode>(std::move(t.value()));
            });

    auto pair = (string >> container.Check(TokenType::COLON) >> lazy_value).Map([](auto &&t) {
        return std::make_unique<PairNode>(std::move(t.first), std::move(t.second));
    });

    auto members = (pair >> (comma >> pair).Many()).Map([](auto &&t) {
        t.second.emplace_back(std::move(t.first));
        return std::move(t.second);
    });

    auto object = (container.Check(TokenType::LBRACE)[members] >> container.Check(TokenType::RBRACE)).Map([](auto &&t) {
        if (!t) return std::make_unique<ObjectNode>(std::vector<std::unique_ptr<PairNode>>());
        return std::make_unique<ObjectNode>(std::move(t.value()));
    });

    auto value = null.Map([](auto &&t) { return std::unique_ptr<AstNode>(std::move(t)); })
                 | object | array | string | number | true_false;

    pValue = &value;

    auto & parser = value;

    std::string input =R"(
    {
        "name": "John Doe",
        "age": 30,
        "is_student": false,
        "skills": ["C++", "Python", "JavaScript"],
        "address": {
            "city": "New York",
            "zip": "10001"
        }
    }
    )";

    auto lexer = JSONLexer(input);
    auto tokens = lexer.tokenize();

    auto result = parser->Parse(tokens.cbegin(), tokens.cend());

    Visitor visitor{};

    (result.value())->visit(visitor);
    return 0;
}