// ast.h -- abstract syntax tree node + pretty printer.
#ifndef RPAL_AST_H
#define RPAL_AST_H

#include <string>
#include <vector>

struct TreeNode {
    std::string             label;
    std::vector<TreeNode*>  children;

    explicit TreeNode(std::string l) : label(std::move(l)) {}
    ~TreeNode() {
        for (auto* c : children) delete c;
    }

    // No copy semantics; nodes own their children.
    TreeNode(const TreeNode&)            = delete;
    TreeNode& operator=(const TreeNode&) = delete;
};

// Print tree in the dot-indent format used by `rpal.exe -ast` / `-st`.
void printTree(const TreeNode* root);

#endif
