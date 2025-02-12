//
// Created by pkuyo on 2025/2/1.
//

#ifndef LIGHT_PARSER_LEXER_H
#define LIGHT_PARSER_LEXER_H

#include <utility>
#include <regex>


enum class token_type {
    LBRACE,    // {
    RBRACE,    // }
    LBRACKET,  // [
    RBRACKET,  // ]
    COLON,     // :
    COMMA,     // ,
    STRING,    // string
    NUMBER,    // number
    TRUE_,      // true
    FALSE_,     // false
    NULL_,     // null
    WHITESPACE // white space
};



// token
struct Token {
    token_type type;
    std::string value;

    explicit operator std::string() const {
        return value;
    }

    friend bool operator==(const Token &left, token_type right) {
        return left.type == right;
    }

    friend bool operator!=(const Token &left, token_type right) {
        return left.type != right;
    }

    friend bool operator==(token_type left, const Token &right) {
        return left == right.type;
    }

    friend bool operator!=(token_type left, const Token &right) {
        return left != right.type;
    }
};




// simple lexer
class JSONLexer {
public:
    explicit JSONLexer(std::string input) : input(std::move(input)), position(0) {}

    // 词法分析函数
    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (position < input.length()) {
            bool matched = false;
            for (const auto &pattern: tokenPatterns) {
                std::regex regex(pattern.second);
                std::smatch match;
                if (std::regex_search(input.cbegin() + position, input.cend(), match, regex,
                                      std::regex_constants::match_continuous)) {
                    std::string value = match.str(0);
                    if (pattern.first != token_type::WHITESPACE) {
                        tokens.push_back({pattern.first, value});
                    }
                    position += value.length();
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                throw std::runtime_error("Invalid character at position " + std::to_string(position));
            }
        }
        return tokens;
    }

private:
    std::string input;
    size_t position;

    const std::vector<std::pair<token_type, std::string>> tokenPatterns = {
            {token_type::LBRACE,     R"(\{)"},          // {
            {token_type::RBRACE,     R"(\})"},          // }
            {token_type::LBRACKET,   R"(\[)"},          // [
            {token_type::RBRACKET,   R"(\])"},          // ]
            {token_type::COLON,      R"(:)"},           // :
            {token_type::COMMA,      R"(,)"},           // ,
            {token_type::STRING,     R"("(?:\\.|[^"\\])*")"}, // string
            {token_type::NUMBER,     R"(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)"}, // number
            {token_type::TRUE_,       R"(true)"},        // true
            {token_type::FALSE_,      R"(false)"},       // false
            {token_type::NULL_,      R"(null)"},        // null
            {token_type::WHITESPACE, R"(\s+)"}         // 空白字符
    };
};

#endif //LIGHT_PARSER_LEXER_H
