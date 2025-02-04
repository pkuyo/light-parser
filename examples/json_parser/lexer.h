//
// Created by pkuyo on 2025/2/1.
//

#ifndef LIGHT_PARSER_LEXER_H
#define LIGHT_PARSER_LEXER_H

#include <utility>
#include <regex>

enum class TokenType {
    LBRACE,    // {
    RBRACE,    // }
    LBRACKET,  // [
    RBRACKET,  // ]
    COLON,     // :
    COMMA,     // ,
    STRING,    // string
    NUMBER,    // number
    TRUE,      // true
    FALSE,     // false
    NULL_,     // null
    WHITESPACE // white space
};

// token
struct Token {
    TokenType type;
    std::string value;

    friend bool operator==(const Token &left, TokenType right) {
        return left.type == right;
    }

    friend bool operator!=(const Token &left, TokenType right) {
        return left.type != right;
    }

    friend bool operator==(TokenType left, const Token &right) {
        return left == right.type;
    }

    friend bool operator!=(TokenType left, const Token &right) {
        return left != right.type;
    }
};

template<>
class std::formatter<Token> {
public:
    constexpr auto parse(auto &context) {
        return context.begin();
    }
    constexpr auto format(const Token &v, auto &context) const
    {
        return std::format_to(context.out(), "{}", v.value);
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
                    if (pattern.first != TokenType::WHITESPACE) {
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

    const std::vector<std::pair<TokenType, std::string>> tokenPatterns = {
            {TokenType::LBRACE,     R"(\{)"},          // {
            {TokenType::RBRACE,     R"(\})"},          // }
            {TokenType::LBRACKET,   R"(\[)"},          // [
            {TokenType::RBRACKET,   R"(\])"},          // ]
            {TokenType::COLON,      R"(:)"},           // :
            {TokenType::COMMA,      R"(,)"},           // ,
            {TokenType::STRING,     R"("(?:\\.|[^"\\])*")"}, // string
            {TokenType::NUMBER,     R"(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)"}, // number
            {TokenType::TRUE,       R"(true)"},        // true
            {TokenType::FALSE,      R"(false)"},       // false
            {TokenType::NULL_,      R"(null)"},        // null
            {TokenType::WHITESPACE, R"(\s+)"}         // 空白字符
    };
};

#endif //LIGHT_PARSER_LEXER_H
