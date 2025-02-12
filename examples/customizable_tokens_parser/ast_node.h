//
// Created by pkuyo on 2025/2/1.
//

#ifndef LIGHT_PARSER_AST_NODE_H
#define LIGHT_PARSER_AST_NODE_H
#include "lexer.h"
#include <iostream>

struct Visitor;

struct AstNode {
    virtual void visit(Visitor & visitor) = 0;
    virtual ~AstNode() = default;
};


struct NumberNode : AstNode {
    explicit NumberNode(const Token &token) : value(std::atof(token.value.c_str())) {}
    void visit(Visitor & visitor) override;

    double value;
};

void NumberNode::visit(Visitor &visitor) {

}

struct StringNode : AstNode {
    explicit StringNode(const Token &token) : value(token.value) {}
    void visit(Visitor & visitor) override;

    std::string value;
};


struct BoolNode : AstNode {
    explicit BoolNode(const Token &token) : value(token.type == token_type::TRUE_) {}
    void visit(Visitor & visitor) override;

    bool value;
};

struct NullNode : AstNode {
    explicit NullNode(const Token &) {}
    void visit(Visitor & visitor) override;

};

struct PairNode : AstNode {
    PairNode(std::unique_ptr<StringNode> &&name, std::unique_ptr<AstNode> &&value) :
            name(std::move(name)), value(std::move(value)) {}
    void visit(Visitor & visitor) override;

    std::unique_ptr<StringNode> name;
    std::unique_ptr<AstNode> value;
};

struct ObjectNode : AstNode {
    explicit  ObjectNode(std::vector<std::unique_ptr<PairNode>> &&node) : elements(std::move(node)) {}
    std::vector<std::unique_ptr<PairNode>> elements;
    void visit(Visitor & visitor) override;

};



struct ArrayNode : AstNode {
    explicit ArrayNode(std::vector<std::unique_ptr<AstNode>> &&node) : elements(std::move(node)) {}
    std::vector<std::unique_ptr<AstNode>> elements;
    void visit(Visitor & visitor) override;

};



struct Visitor {
    int header = 0;

    void printSpaces(int n) {
        std::cout << std::string(n, ' ');
    }
    template<typename T>
    void print(const char* c, T&& type) {
        printSpaces(header);
        std::cout << std::format("{} - {}", c, type) << std::endl;
    }

    void accept(NumberNode& node) {
        print("NumberNode",node.value);
    }

    void accept(BoolNode& node) {
        print("BoolNode",node.value);
    }
    void accept(StringNode& node) {
        print("StringNode",std::format("{}",node.value));
    }

    void accept(NullNode& node) {
        print("NullNode","null");
    }

    void accept(ArrayNode& node) {
        print("ArrayNode", std::format("count:{}",node.elements.size()));
        header+=2;
        for(auto & child : node.elements)
            child->visit(*this);
        header-=2;
    }
    void accept(PairNode& node) {
        print("PairNode",std::format("name:{}",node.name->value));
        header+=2;
        node.value->visit(*this);
        header-=2;
    }
    void accept(ObjectNode& node) {
        print("ObjectNode", std::format("count:{}",node.elements.size()));
        header+=2;
        for(auto & child : node.elements)
            child->visit(*this);
        header-=2;
    }
};
void NullNode::visit(Visitor &visitor) {
    visitor.accept(*this);
}

void BoolNode::visit(Visitor &visitor) {
    visitor.accept(*this);
}
void StringNode::visit(Visitor &visitor) {
    visitor.accept(*this);
}
void ArrayNode::visit(Visitor &visitor) {
    visitor.accept(*this);
}

void PairNode::visit(Visitor &visitor) {
    visitor.accept(*this);
}

void ObjectNode::visit(Visitor &visitor) {
    visitor.accept(*this);
}
#endif //LIGHT_PARSER_AST_NODE_H
