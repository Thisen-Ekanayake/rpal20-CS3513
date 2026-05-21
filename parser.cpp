// parser.cpp -- recursive-descent parser implementing the grammar in
// RPAL_Grammar.pdf.  Builds the AST via a stack-based "buildTree" helper.
#include "parser.h"

#include <sstream>
#include <stdexcept>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek(std::size_t off) const {
    return tokens_[pos_ + off];
}

bool Parser::isKeyword(const std::string& k) const {
    return peek().type == TokenType::KEYWORD && peek().value == k;
}

bool Parser::isOperator(const std::string& op) const {
    return peek().type == TokenType::OPERATOR && peek().value == op;
}

bool Parser::isPunctuation(char c) const {
    return peek().type == TokenType::PUNCTUATION
           && peek().value.size() == 1 && peek().value[0] == c;
}

bool Parser::isRnStart() const {
    const Token& t = peek();
    if (t.type == TokenType::IDENTIFIER) return true;
    if (t.type == TokenType::INTEGER)    return true;
    if (t.type == TokenType::STRING)     return true;
    if (t.type == TokenType::KEYWORD &&
        (t.value == "true" || t.value == "false" ||
         t.value == "nil"  || t.value == "dummy")) return true;
    if (isPunctuation('(')) return true;
    return false;
}

bool Parser::isVbStart() const {
    return peek().type == TokenType::IDENTIFIER || isPunctuation('(');
}

void Parser::expectOperator(const std::string& op) {
    if (!isOperator(op)) {
        std::ostringstream oss;
        oss << "Parse error: expected operator '" << op
            << "' but got '" << peek().value << "' at line " << peek().line;
        throw std::runtime_error(oss.str());
    }
    ++pos_;
}

void Parser::expectKeyword(const std::string& k) {
    if (!isKeyword(k)) {
        std::ostringstream oss;
        oss << "Parse error: expected keyword '" << k
            << "' but got '" << peek().value << "' at line " << peek().line;
        throw std::runtime_error(oss.str());
    }
    ++pos_;
}

void Parser::expectPunctuation(char c) {
    if (!isPunctuation(c)) {
        std::ostringstream oss;
        oss << "Parse error: expected '" << c
            << "' but got '" << peek().value << "' at line " << peek().line;
        throw std::runtime_error(oss.str());
    }
    ++pos_;
}

void Parser::buildTree(const std::string& label, int childCount) {
    if (static_cast<int>(stack_.size()) < childCount) {
        throw std::runtime_error("Internal parser error: stack underflow for label " + label);
    }
    auto* node = new TreeNode(label);
    int start = static_cast<int>(stack_.size()) - childCount;
    node->children.reserve(childCount);
    for (int i = start; i < static_cast<int>(stack_.size()); ++i) {
        node->children.push_back(stack_[i]);
    }
    stack_.resize(start);
    stack_.push_back(node);
}

void Parser::pushIdentifierLeaf() {
    if (peek().type != TokenType::IDENTIFIER) {
        std::ostringstream oss;
        oss << "Parse error: expected identifier but got '"
            << peek().value << "' at line " << peek().line;
        throw std::runtime_error(oss.str());
    }
    stack_.push_back(new TreeNode("<ID:" + peek().value + ">"));
    ++pos_;
}

TreeNode* Parser::parse() {
    E();
    if (peek().type != TokenType::END_OF_FILE) {
        std::ostringstream oss;
        oss << "Parse error: unexpected token '"
            << peek().value << "' at line " << peek().line;
        throw std::runtime_error(oss.str());
    }
    if (stack_.size() != 1) {
        throw std::runtime_error("Parse error: stack size mismatch at end");
    }
    return stack_[0];
}

// ============================================================
//  Expressions
// ============================================================

// E -> 'let' D 'in' E             => 'let'
//   -> 'fn' Vb+ '.' E             => 'lambda'
//   -> Ew
void Parser::E() {
    if (isKeyword("let")) {
        ++pos_;
        D();
        expectKeyword("in");
        E();
        buildTree("let", 2);
    } else if (isKeyword("fn")) {
        ++pos_;
        int n = 0;
        while (isVbStart()) {
            Vb();
            ++n;
        }
        if (n == 0) {
            throw std::runtime_error("Parse error: 'fn' requires at least one Vb");
        }
        expectOperator(".");
        E();
        buildTree("lambda", n + 1);
    } else {
        Ew();
    }
}

// Ew -> T 'where' Dr => 'where'
//    -> T
void Parser::Ew() {
    T();
    if (isKeyword("where")) {
        ++pos_;
        Dr();
        buildTree("where", 2);
    }
}

// T -> Ta ( ',' Ta )+ => 'tau'
//   -> Ta
void Parser::T() {
    Ta();
    int n = 1;
    while (isPunctuation(',')) {
        ++pos_;
        Ta();
        ++n;
    }
    if (n > 1) buildTree("tau", n);
}

// Ta -> Ta 'aug' Tc => 'aug'   (left-recursive: rewritten iteratively)
//    -> Tc
void Parser::Ta() {
    Tc();
    while (isKeyword("aug")) {
        ++pos_;
        Tc();
        buildTree("aug", 2);
    }
}

// Tc -> B '->' Tc '|' Tc => '->'
//    -> B
void Parser::Tc() {
    B();
    if (isOperator("->")) {
        ++pos_;
        Tc();
        expectOperator("|");
        Tc();
        buildTree("->", 3);
    }
}

// ============================================================
//  Boolean expressions
// ============================================================

// B -> B 'or' Bt => 'or' (left-recursive)
//   -> Bt
void Parser::B() {
    Bt();
    while (isKeyword("or")) {
        ++pos_;
        Bt();
        buildTree("or", 2);
    }
}

// Bt -> Bt '&' Bs => '&' (left-recursive)
//    -> Bs
void Parser::Bt() {
    Bs();
    while (isOperator("&")) {
        ++pos_;
        Bs();
        buildTree("&", 2);
    }
}

// Bs -> 'not' Bp => 'not'
//    -> Bp
void Parser::Bs() {
    if (isKeyword("not")) {
        ++pos_;
        Bp();
        buildTree("not", 1);
    } else {
        Bp();
    }
}

// Bp -> A ('gr'|'>') A => 'gr' | ... | A
void Parser::Bp() {
    A();
    if (isKeyword("gr") || isOperator(">")) {
        ++pos_; A(); buildTree("gr", 2);
    } else if (isKeyword("ge") || isOperator(">=")) {
        ++pos_; A(); buildTree("ge", 2);
    } else if (isKeyword("ls") || isOperator("<")) {
        ++pos_; A(); buildTree("ls", 2);
    } else if (isKeyword("le") || isOperator("<=")) {
        ++pos_; A(); buildTree("le", 2);
    } else if (isKeyword("eq")) {
        ++pos_; A(); buildTree("eq", 2);
    } else if (isKeyword("ne")) {
        ++pos_; A(); buildTree("ne", 2);
    }
}

// ============================================================
//  Arithmetic expressions
// ============================================================

// A -> '+' At
//   -> '-' At => 'neg'
//   -> A '+' At => '+' (left-recursive)
//   -> A '-' At => '-'
//   -> At
void Parser::A() {
    if (isOperator("+")) {
        ++pos_;
        At();
    } else if (isOperator("-")) {
        ++pos_;
        At();
        buildTree("neg", 1);
    } else {
        At();
    }
    while (isOperator("+") || isOperator("-")) {
        std::string op = peek().value;
        ++pos_;
        At();
        buildTree(op, 2);
    }
}

// At -> At '*' Af => '*'  (left-recursive)
//    -> At '/' Af => '/'
//    -> Af
void Parser::At() {
    Af();
    while (isOperator("*") || isOperator("/")) {
        std::string op = peek().value;
        ++pos_;
        Af();
        buildTree(op, 2);
    }
}

// Af -> Ap '**' Af => '**'  (right-recursive)
//    -> Ap
void Parser::Af() {
    Ap();
    if (isOperator("**")) {
        ++pos_;
        Af();
        buildTree("**", 2);
    }
}

// Ap -> Ap '@' '<IDENTIFIER>' R => '@'  (left-recursive)
//    -> R
void Parser::Ap() {
    R();
    while (isOperator("@")) {
        ++pos_;
        pushIdentifierLeaf();
        R();
        buildTree("@", 3);
    }
}

// ============================================================
//  Rators and Rands
// ============================================================

// R -> R Rn => 'gamma' (left-recursive: function application)
//   -> Rn
void Parser::R() {
    Rn();
    while (isRnStart()) {
        Rn();
        buildTree("gamma", 2);
    }
}

// Rn -> '<IDENTIFIER>' | '<INTEGER>' | '<STRING>' |
//       'true' | 'false' | 'nil' | 'dummy' | '(' E ')'
void Parser::Rn() {
    const Token& t = peek();
    switch (t.type) {
        case TokenType::IDENTIFIER:
            stack_.push_back(new TreeNode("<ID:" + t.value + ">"));
            ++pos_;
            break;
        case TokenType::INTEGER:
            stack_.push_back(new TreeNode("<INT:" + t.value + ">"));
            ++pos_;
            break;
        case TokenType::STRING:
            // Recover the original quoted source form for AST printing only;
            // the runtime stores the decoded value (handled later by the
            // standardizer/CSE machine).
            stack_.push_back(new TreeNode("<STR:'" + t.value + "'>"));
            ++pos_;
            break;
        case TokenType::KEYWORD:
            if (t.value == "true" || t.value == "false" ||
                t.value == "nil"  || t.value == "dummy") {
                stack_.push_back(new TreeNode("<" + t.value + ">"));
                ++pos_;
            } else {
                std::ostringstream oss;
                oss << "Parse error: unexpected keyword '" << t.value
                    << "' at line " << t.line;
                throw std::runtime_error(oss.str());
            }
            break;
        case TokenType::PUNCTUATION:
            if (t.value == "(") {
                ++pos_;
                E();
                expectPunctuation(')');
            } else {
                std::ostringstream oss;
                oss << "Parse error: unexpected punctuation '" << t.value
                    << "' at line " << t.line;
                throw std::runtime_error(oss.str());
            }
            break;
        default: {
            std::ostringstream oss;
            oss << "Parse error: unexpected token '" << t.value
                << "' at line " << t.line;
            throw std::runtime_error(oss.str());
        }
    }
}

// ============================================================
//  Definitions
// ============================================================

// D -> Da 'within' D => 'within'
//   -> Da
void Parser::D() {
    Da();
    if (isKeyword("within")) {
        ++pos_;
        D();
        buildTree("within", 2);
    }
}

// Da -> Dr ( 'and' Dr )+ => 'and'
//    -> Dr
void Parser::Da() {
    Dr();
    int n = 1;
    while (isKeyword("and")) {
        ++pos_;
        Dr();
        ++n;
    }
    if (n > 1) buildTree("and", n);
}

// Dr -> 'rec' Db => 'rec'
//    -> Db
void Parser::Dr() {
    if (isKeyword("rec")) {
        ++pos_;
        Db();
        buildTree("rec", 1);
    } else {
        Db();
    }
}

// Db -> Vl '=' E                          => '='
//    -> '<IDENTIFIER>' Vb+ '=' E          => 'function_form'
//    -> '(' D ')'
//
// We need lookahead to distinguish Vl '=' E (Vl is a single ID followed by
// '=' or a comma-list) from a function form (ID followed by an ID or '(').
void Parser::Db() {
    if (isPunctuation('(')) {
        ++pos_;
        D();
        expectPunctuation(')');
        return;
    }
    if (peek().type != TokenType::IDENTIFIER) {
        std::ostringstream oss;
        oss << "Parse error: expected identifier or '(' in definition at line "
            << peek().line;
        throw std::runtime_error(oss.str());
    }

    // Decide between Vl '=' E and function_form by looking ahead.
    const Token& next = peek(1);
    bool isFunctionForm = false;
    if (next.type == TokenType::IDENTIFIER) {
        isFunctionForm = true;
    } else if (next.type == TokenType::PUNCTUATION && next.value == "(") {
        isFunctionForm = true;
    }

    if (isFunctionForm) {
        // ID Vb+ '=' E
        pushIdentifierLeaf();           // function name
        int vbCount = 0;
        while (isVbStart()) {
            Vb();
            ++vbCount;
        }
        if (vbCount == 0) {
            throw std::runtime_error("Parse error: function form requires at least one Vb");
        }
        expectOperator("=");
        E();
        buildTree("function_form", vbCount + 2);  // name + Vbs + body
    } else {
        // Vl '=' E
        Vl();
        expectOperator("=");
        E();
        buildTree("=", 2);
    }
}

// ============================================================
//  Variables
// ============================================================

// Vb -> '<IDENTIFIER>'
//    -> '(' Vl ')'
//    -> '(' ')'  => '()'
void Parser::Vb() {
    if (peek().type == TokenType::IDENTIFIER) {
        pushIdentifierLeaf();
    } else if (isPunctuation('(')) {
        ++pos_;
        if (isPunctuation(')')) {
            ++pos_;
            stack_.push_back(new TreeNode("()"));
        } else {
            Vl();
            expectPunctuation(')');
        }
    } else {
        std::ostringstream oss;
        oss << "Parse error: expected Vb at line " << peek().line;
        throw std::runtime_error(oss.str());
    }
}

// Vl -> '<IDENTIFIER>' ( ',' '<IDENTIFIER>' )* => ','?
//   (build comma-node only when more than one identifier)
void Parser::Vl() {
    pushIdentifierLeaf();
    int n = 1;
    while (isPunctuation(',')) {
        ++pos_;
        pushIdentifierLeaf();
        ++n;
    }
    if (n > 1) buildTree(",", n);
}
