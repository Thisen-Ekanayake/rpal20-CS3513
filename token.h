// token.h -- token types for the RPAL lexical analyzer.
#ifndef RPAL_TOKEN_H
#define RPAL_TOKEN_H

#include <string>

enum class TokenType {
    IDENTIFIER,
    INTEGER,
    STRING,
    OPERATOR,
    KEYWORD,
    PUNCTUATION,
    END_OF_FILE
};

struct Token {
    TokenType  type;
    std::string value;
    int        line;
};

#endif
