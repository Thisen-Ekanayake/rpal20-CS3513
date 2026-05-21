// standardizer.h -- transform an AST into a Standardized Tree (ST).
//
// The ST consists only of: gamma, lambda, ->, tau, aug, leaves
// (<ID:..>, <INT:..>, <STR:..>, <true>, <false>, <nil>, <dummy>, <Y*>),
// and binary/unary operator nodes that the CSE machine handles natively.
#ifndef RPAL_STANDARDIZER_H
#define RPAL_STANDARDIZER_H

#include "ast.h"

// Standardize the tree rooted at `node` IN PLACE (post-order).
void standardize(TreeNode* node);

#endif
