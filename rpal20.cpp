// rpal20.cpp -- entry point for the RPAL interpreter.
//
// Usage:   ./rpal20 [flags] <source-file>
// Flags:
//   -ast     print only the AST (no evaluation)
//   -st      print only the Standardized Tree (no evaluation)
//   -noout   skip CSE evaluation entirely (useful with -ast / -st)
//
// In its default mode (no flags) the program executes the source program
// through the CSE machine; output matches `rpal.exe`.
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ast.h"
#include "cse.h"
#include "lexer.h"
#include "parser.h"
#include "standardizer.h"

static void usage() {
    std::cerr << "Usage: ./rpal20 [-ast] [-st] [-noout] <file>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(); return 1; }

    bool showAst = false;
    bool showSt  = false;
    bool noEval  = false;
    std::string filename;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "-ast")   { showAst = true; noEval = true; }
        else if (a == "-st")    { showSt  = true; noEval = true; }
        else if (a == "-noout") { noEval  = true; }
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "Unknown flag: " << a << "\n";
            usage();
            return 1;
        }
        else { filename = a; }
    }

    if (filename.empty()) { usage(); return 1; }

    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Cannot open file: " << filename << "\n";
        return 1;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    TreeNode* tree = nullptr;
    try {
        Lexer lex(source);
        auto tokens = lex.tokenize();

        Parser parser(std::move(tokens));
        tree = parser.parse();

        if (showAst) {
            printTree(tree);
            delete tree;
            return 0;
        }

        standardize(tree);

        if (showSt) {
            printTree(tree);
            delete tree;
            return 0;
        }

        if (!noEval) {
            CSEMachine cse;
            cse.run(tree);
        }
        delete tree;
    } catch (const std::exception& e) {
        delete tree;
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
