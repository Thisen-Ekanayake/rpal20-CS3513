// lexer.cpp -- implementation of the RPAL lexer following RPAL_Lex.pdf.
#include "lexer.h"

#include <cctype>
#include <stdexcept>
#include <unordered_set>

namespace {

const std::unordered_set<std::string> kKeywords = {
    "let", "in", "fn", "where", "aug", "or", "not",
    "gr", "ge", "ls", "le", "eq", "ne",
    "true", "false", "nil", "dummy",
    "within", "and", "rec"
};

bool isOperatorChar(char c) {
    // Operator_symbol per RPAL_Lex.pdf.
    static const std::string kOps =
        "+-*<>&.@/:=~|$!#%^_[]{}\"`?";
    return kOps.find(c) != std::string::npos;
}

bool isLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

}  // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

bool Lexer::eof() const { return pos_ >= source_.size(); }

char Lexer::peek(std::size_t off) const {
    return (pos_ + off < source_.size()) ? source_[pos_ + off] : '\0';
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') ++line_;
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (!eof()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Line comment: consume until end-of-line.
            while (!eof() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::scanIdentifierOrKeyword() {
    int  startLine = line_;
    std::string lex;
    while (!eof() && (isLetter(peek()) || isDigit(peek()) || peek() == '_')) {
        lex += advance();
    }
    if (kKeywords.count(lex)) {
        return {TokenType::KEYWORD, lex, startLine};
    }
    return {TokenType::IDENTIFIER, lex, startLine};
}

Token Lexer::scanInteger() {
    int  startLine = line_;
    std::string lex;
    while (!eof() && isDigit(peek())) lex += advance();
    return {TokenType::INTEGER, lex, startLine};
}

Token Lexer::scanOperator() {
    int  startLine = line_;
    std::string lex;
    while (!eof() && isOperatorChar(peek())) lex += advance();
    return {TokenType::OPERATOR, lex, startLine};
}

Token Lexer::scanString() {
    int  startLine = line_;
    advance();  // consume opening single quote
    std::string lex;
    while (!eof() && peek() != '\'') {
        char c = peek();
        if (c == '\\' && pos_ + 1 < source_.size()) {
            char esc = source_[pos_ + 1];
            switch (esc) {
                case 't':  lex += '\t';  pos_ += 2; break;
                case 'n':  lex += '\n';  pos_ += 2; break;
                case '\\': lex += '\\';  pos_ += 2; break;
                case '\'': lex += '\'';  pos_ += 2; break;
                default:   lex += esc;   pos_ += 2; break;
            }
        } else {
            lex += advance();
        }
    }
    if (eof()) {
        throw std::runtime_error(
            "Lexer error: unterminated string starting at line "
            + std::to_string(startLine));
    }
    advance();  // consume closing quote
    return {TokenType::STRING, lex, startLine};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    while (true) {
        skipWhitespaceAndComments();
        if (eof()) {
            out.push_back({TokenType::END_OF_FILE, "", line_});
            break;
        }
        char c = peek();
        if (isLetter(c)) {
            out.push_back(scanIdentifierOrKeyword());
        } else if (isDigit(c)) {
            out.push_back(scanInteger());
        } else if (c == '\'') {
            out.push_back(scanString());
        } else if (c == '(' || c == ')' || c == ';' || c == ',') {
            int curLine = line_;
            advance();
            out.push_back({TokenType::PUNCTUATION, std::string(1, c), curLine});
        } else if (isOperatorChar(c)) {
            out.push_back(scanOperator());
        } else {
            throw std::runtime_error(
                std::string("Lexer error: unexpected character '") + c
                + "' at line " + std::to_string(line_));
        }
    }
    return out;
}
