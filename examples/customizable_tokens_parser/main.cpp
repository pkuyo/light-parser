//
// Created by pkuyo on 2025/2/1.
//
// Parse customizable token example
//

#include "parser.h"

#include "lexer.h"
#include "ast_node.h"
#include <string>
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

template<typename T>
auto JsonNode(TokenType & type) { return pkuyo::parsers::SinglePtr<TokenType,Token,T>(type);}

namespace json {
    using namespace pkuyo::parsers;

    using namespace std;


    //Terminal Symbol

    auto number = SinglePtr<Token,TokenType,NumberNode>(TokenType::NUMBER).Name("Name");

    auto str = SinglePtr<Token,TokenType,StringNode>(TokenType::STRING).Name("String");

    //bool
    auto true_false = (SinglePtr<Token,TokenType,BoolNode>(TokenType::FALSE) |SinglePtr<Token,TokenType,BoolNode>(TokenType::TRUE)).Name("Bool");

    auto null = SinglePtr<Token,TokenType,NullNode>(TokenType::NULL_).Name("Null") >>= [](unique_ptr<NullNode> &&t) { return unique_ptr<AstNode>(std::move(t)); };

    auto comma = Check<Token,TokenType>(TokenType::COMMA).Name(",");

    //Non-terminal Symbol

    struct lazy_value;

    auto l_value = Lazy<Token,lazy_value>().Name("Value");

    auto elements = ( (l_value >> *(comma >> l_value)) >>= [](tuple<unique_ptr<AstNode>,vector<unique_ptr<AstNode>>> &&t) {
        auto & [first,values] = t;
        values.emplace_back(std::move(first));
        return std::move(values);
    }).Name("Elements");


    auto array = ( (TokenType::LBRACKET >> ~elements >>TokenType::RBRACKET) >>=
                           [](optional<vector<unique_ptr<AstNode>>> &&t) {
                               if (!t) return make_unique<ArrayNode>(vector<unique_ptr<AstNode>>());
                               return make_unique<ArrayNode>(std::move(t.value()));
                           }).Name("Array");


    auto pair =( (str >> TokenType::COLON >> l_value) >>= [](tuple<unique_ptr<StringNode>,unique_ptr<AstNode>> &&t) {
        return std::make_unique<PairNode>(std::move(std::get<0>(t)), std::move(std::get<1>(t)));
    }).Name("Pair");

    auto members = ((pair >> *(comma >> pair)) >>= [](tuple<unique_ptr<PairNode>,vector<unique_ptr<PairNode>>> &&t) {
        auto & [first,values] = t;
        values.emplace_back(std::move(first));
        return std::move(values);
    }).Name("Members");

    auto object = ((TokenType::LBRACE >> ~members >> TokenType::RBRACE ) >>= [](optional<vector<unique_ptr<PairNode>>> &&t) -> unique_ptr<ObjectNode> {
        if (!t) return std::make_unique<ObjectNode>(std::vector<std::unique_ptr<PairNode>>());
        return std::make_unique<ObjectNode>(std::move(t.value()));
    }).Name("Object");

    auto value = ( null | object | array | str | number | true_false).Name("Value");


    struct lazy_value : public base_parser<char,lazy_value> {
        std::optional<unique_ptr<AstNode>> parse_impl(auto& stream) const {
            return value.Parse(stream);
        }
        bool peek_impl(auto & stream) const {
            return value.Peek(stream);
        }
    };
    auto & parser = value;
}

int main() {


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
    pkuyo::parsers::container_stream<std::vector<Token>> token_stream(lexer.tokenize());

    auto result = json::parser.Parse(token_stream);

    Visitor visitor{};

    (result.value())->visit(visitor);
    return 0;
}