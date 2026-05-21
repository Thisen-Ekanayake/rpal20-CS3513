// lexer.h -- RPAL lexical analyzer.
#ifndef RPAL_LEXER_H
#define RPAL_LEXER_H

#include <string>
#include <vector>
#include "token.h"

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string source_;
    std::size_t pos_  = 0;
    int         line_ = 1;

    bool   eof() const;
    char   peek(std::size_t off = 0) const;
    char   advance();
    void   skipWhitespaceAndComments();
    Token  scanIdentifierOrKeyword();
    Token  scanInteger();
    Token  scanOperator();
    Token  scanString();
};

#endif
