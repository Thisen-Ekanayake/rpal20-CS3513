// standardizer.cpp -- apply the standardization rules of RPAL bottom-up.
//
// Rules implemented (see RPAL textbook / lecture notes):
//   let     :  let(=(X,E), P)        -> gamma(lambda(X, P), E)
//   where   :  where(P, =(X,E))      -> gamma(lambda(X, P), E)
//   fcn_form:  fcn_form(F, V1..Vn,E) -> =(F, lambda(V1, ... lambda(Vn, E)))
//   lambda  :  lambda(V1..Vn, E)     -> lambda(V1, ... lambda(Vn, E))
//   within  :  within(=(X1,E1), =(X2,E2))
//                                     -> =(X2, gamma(lambda(X1, E2), E1))
//   and     :  and(=(X1,E1),...,=(Xn,En))
//                                     -> =(tau(X1..Xn), tau(E1..En))
//   rec     :  rec(=(X, E))          -> =(X, gamma(<Y*>, lambda(X, E)))
//   @       :  @(E1, N, E2)          -> gamma(gamma(N, E1), E2)
#include "standardizer.h"

#include <stdexcept>
#include <string>

namespace {

// Replicate a leaf id-node (used to duplicate the recursive variable name).
TreeNode* cloneLeaf(const TreeNode* src) {
    return new TreeNode(src->label);
}

void standardizeLet(TreeNode* node) {
    // node->children == [=, P]
    TreeNode* eq = node->children[0];
    TreeNode* P  = node->children[1];
    if (eq->label != "=" || eq->children.size() != 2) {
        throw std::runtime_error("Standardizer: malformed 'let'");
    }
    TreeNode* X = eq->children[0];
    TreeNode* E = eq->children[1];

    // Reuse eq as the lambda node.
    eq->label       = "lambda";
    eq->children[0] = X;
    eq->children[1] = P;

    node->label       = "gamma";
    node->children[0] = eq;
    node->children[1] = E;
}

void standardizeWhere(TreeNode* node) {
    // node->children == [P, =]
    TreeNode* P  = node->children[0];
    TreeNode* eq = node->children[1];
    if (eq->label != "=" || eq->children.size() != 2) {
        throw std::runtime_error("Standardizer: malformed 'where'");
    }
    TreeNode* X = eq->children[0];
    TreeNode* E = eq->children[1];

    eq->label       = "lambda";
    eq->children[0] = X;
    eq->children[1] = P;

    node->label       = "gamma";
    node->children[0] = eq;
    node->children[1] = E;
}

void standardizeFunctionForm(TreeNode* node) {
    // node->children == [F, V1, V2, ..., Vn, E]
    auto& ch = node->children;
    if (ch.size() < 3) {
        throw std::runtime_error("Standardizer: malformed function_form");
    }
    TreeNode* F    = ch.front();
    TreeNode* body = ch.back();
    // Build lambda chain V1 -> V2 -> ... -> Vn -> body
    TreeNode* cur = body;
    for (int i = static_cast<int>(ch.size()) - 2; i >= 1; --i) {
        auto* lam = new TreeNode("lambda");
        lam->children.push_back(ch[i]);
        lam->children.push_back(cur);
        cur = lam;
    }
    // Convert node into '=' (F, cur).
    ch.clear();
    node->label = "=";
    ch.push_back(F);
    ch.push_back(cur);
}

void standardizeLambda(TreeNode* node) {
    // multi-arg lambda: collapse to nested single-arg lambdas
    if (node->children.size() <= 2) return;
    auto& ch = node->children;
    TreeNode* body = ch.back();
    TreeNode* cur  = body;
    for (int i = static_cast<int>(ch.size()) - 2; i >= 1; --i) {
        auto* lam = new TreeNode("lambda");
        lam->children.push_back(ch[i]);
        lam->children.push_back(cur);
        cur = lam;
    }
    TreeNode* V1 = ch[0];
    ch.clear();
    ch.push_back(V1);
    ch.push_back(cur);
}

void standardizeWithin(TreeNode* node) {
    // within(=(X1,E1), =(X2,E2)) -> =(X2, gamma(lambda(X1,E2), E1))
    TreeNode* eq1 = node->children[0];
    TreeNode* eq2 = node->children[1];
    if (eq1->label != "=" || eq2->label != "=" ||
        eq1->children.size() != 2 || eq2->children.size() != 2) {
        throw std::runtime_error("Standardizer: malformed 'within'");
    }
    TreeNode* X1 = eq1->children[0];
    TreeNode* E1 = eq1->children[1];
    TreeNode* X2 = eq2->children[0];
    TreeNode* E2 = eq2->children[1];

    // Reuse eq1 as lambda(X1, E2)
    eq1->label       = "lambda";
    eq1->children[0] = X1;
    eq1->children[1] = E2;

    // Reuse eq2 as gamma(lambda(X1,E2), E1)
    eq2->label       = "gamma";
    eq2->children[0] = eq1;
    eq2->children[1] = E1;

    node->label       = "=";
    node->children[0] = X2;
    node->children[1] = eq2;
}

void standardizeAnd(TreeNode* node) {
    // and(=1, =2, ..., =N) -> =( ,(X1..XN) , tau(E1..EN) )
    // Use "," on the binding side (so the receiving lambda destructures by
    // position, matching RPAL's "(x, y)" parameter convention) and "tau" on
    // the value side (which constructs the tuple at runtime).
    auto* commaX = new TreeNode(",");
    auto* tauE   = new TreeNode("tau");
    for (TreeNode* eq : node->children) {
        if (eq->label != "=" || eq->children.size() != 2) {
            throw std::runtime_error("Standardizer: malformed 'and' child");
        }
        commaX->children.push_back(eq->children[0]);
        tauE->children.push_back(eq->children[1]);
        eq->children.clear();
        delete eq;
    }
    node->children.clear();
    node->label = "=";
    node->children.push_back(commaX);
    node->children.push_back(tauE);
}

void standardizeRec(TreeNode* node) {
    // rec(=(X, E)) -> =(X, gamma(<Y*>, lambda(X', E)))   (X' is a fresh copy)
    if (node->children.size() != 1 ||
        node->children[0]->label != "=" ||
        node->children[0]->children.size() != 2) {
        throw std::runtime_error("Standardizer: malformed 'rec'");
    }
    TreeNode* eq = node->children[0];
    TreeNode* X  = eq->children[0];
    TreeNode* E  = eq->children[1];

    TreeNode* Xclone = cloneLeaf(X);

    auto* lam   = new TreeNode("lambda");
    lam->children.push_back(Xclone);
    lam->children.push_back(E);

    auto* ystar = new TreeNode("<Y*>");
    auto* gam   = new TreeNode("gamma");
    gam->children.push_back(ystar);
    gam->children.push_back(lam);

    // Detach children of eq before freeing it.
    eq->children.clear();
    delete eq;
    node->children.clear();
    node->label = "=";
    node->children.push_back(X);
    node->children.push_back(gam);
}

void standardizeAt(TreeNode* node) {
    // @(E1, N, E2) -> gamma(gamma(N, E1), E2)
    if (node->children.size() != 3) {
        throw std::runtime_error("Standardizer: malformed '@'");
    }
    TreeNode* E1 = node->children[0];
    TreeNode* N  = node->children[1];
    TreeNode* E2 = node->children[2];

    auto* inner = new TreeNode("gamma");
    inner->children.push_back(N);
    inner->children.push_back(E1);

    node->children.clear();
    node->label = "gamma";
    node->children.push_back(inner);
    node->children.push_back(E2);
}

}  // namespace

void standardize(TreeNode* node) {
    if (!node) return;
    for (TreeNode* c : node->children) standardize(c);

    const std::string& lab = node->label;
    if      (lab == "let")           standardizeLet(node);
    else if (lab == "where")         standardizeWhere(node);
    else if (lab == "function_form") standardizeFunctionForm(node);
    else if (lab == "lambda")        standardizeLambda(node);
    else if (lab == "within")        standardizeWithin(node);
    else if (lab == "and")           standardizeAnd(node);
    else if (lab == "rec")           standardizeRec(node);
    else if (lab == "@")             standardizeAt(node);
    // All other labels: no transformation.
}
