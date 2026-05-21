// parser.h -- recursive-descent parser for RPAL.
#ifndef RPAL_PARSER_H
#define RPAL_PARSER_H

#include <string>
#include <vector>

#include "ast.h"
#include "token.h"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    TreeNode* parse();

private:
    std::vector<Token>      tokens_;
    std::size_t             pos_ = 0;
    std::vector<TreeNode*>  stack_;

    // ---- helpers ----
    const Token& peek(std::size_t off = 0) const;
    bool         isKeyword(const std::string& k) const;
    bool         isOperator(const std::string& op) const;
    bool         isPunctuation(char c) const;
    bool         isRnStart() const;
    bool         isVbStart() const;

    void expectOperator(const std::string& op);
    void expectKeyword(const std::string& k);
    void expectPunctuation(char c);

    void buildTree(const std::string& label, int childCount);
    void pushLeaf();              // pushes whatever the current token is
    void pushIdentifierLeaf();    // pushes <ID:name> and consumes

    // ---- grammar productions ----
    void E();
    void Ew();
    void T();
    void Ta();
    void Tc();
    void B();
    void Bt();
    void Bs();
    void Bp();
    void A();
    void At();
    void Af();
    void Ap();
    void R();
    void Rn();
    void D();
    void Da();
    void Dr();
    void Db();
    void Vb();
    void Vl();   // returns number of identifiers consumed via stack count
};

#endif
