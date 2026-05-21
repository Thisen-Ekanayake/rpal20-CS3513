// ast.cpp
#include "ast.h"

#include <cstdio>

static void printHelper(const TreeNode* n, int depth) {
    for (int i = 0; i < depth; ++i) std::fputc('.', stdout);
    std::fputs(n->label.c_str(), stdout);
    std::fputc('\n', stdout);
    for (const auto* c : n->children) printHelper(c, depth + 1);
}

void printTree(const TreeNode* root) {
    printHelper(root, 0);
}
