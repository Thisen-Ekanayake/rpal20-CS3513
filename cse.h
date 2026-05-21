// cse.h -- Control-Stack-Environment machine for RPAL.
//
// Driver for the Standardized Tree.  The tree is first flattened into a
// table of "control structures" (one for the program plus one per lambda /
// conditional branch).  The machine then steps through control items,
// using a value stack and an environment chain.
#ifndef RPAL_CSE_H
#define RPAL_CSE_H

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"

struct Environment;

struct Value {
    enum Kind {
        INT_, STR_, BOOL_, DUMMY_, TUPLE_,
        LAMBDA_CL, ETA, YSTAR_,
        ENV_MARKER, BUILTIN
    };
    Kind                       kind = INT_;
    long                       intVal  = 0;
    std::string                strVal;
    bool                       boolVal = false;
    std::vector<Value>         tupleVal;
    int                        ctrlIdx = 0;
    std::vector<std::string>   bindVars;
    bool                       isCommaBind = false;
    bool                       isEmptyBind = false;
    Environment*               env  = nullptr;
    int                        envId = 0;
    std::string                builtinName;
    // For partially-applied builtins (currently only Conc) we stash the first
    // argument here.  shared_ptr keeps the Value cheap to copy.
    std::shared_ptr<Value>     partialArg;
};

struct CtrlItem {
    enum Kind {
        NAME,
        INT_, STR_, TRUE_, FALSE_, NIL_, DUMMY_, YSTAR_,
        LAMBDA,
        GAMMA,
        TAU, AUG,
        BETA,
        ENV_EXIT,
        OP_BIN, OP_UN,
        LITERAL_VALUE
    };
    Kind                      kind = NAME;
    std::string               sval;
    long                      ival  = 0;
    long                      ival2 = 0;
    std::vector<std::string>  bindVars;
    bool                      isCommaBind = false;
    bool                      isEmptyBind = false;
    Value                     lit;            // for LITERAL_VALUE
};

struct Environment {
    int                                          id = 0;
    Environment*                                 parent = nullptr;
    std::unordered_map<std::string, Value>       bindings;
};

class CSEMachine {
public:
    void run(TreeNode* st);

private:
    std::vector<std::vector<CtrlItem>>  ctrlStructs_;
    std::deque<CtrlItem>                control_;
    std::vector<Value>                  stack_;
    std::vector<Environment*>           envStack_;
    std::vector<std::unique_ptr<Environment>> envOwn_;
    int                                 nextEnvId_ = 0;

    // ---- helpers ----
    void   flattenInto(TreeNode* node, int structIdx);
    void   step();
    void   applyGamma();
    void   applyBuiltin(const Value& fn, const Value& arg);
    void   applyBinaryOp(const std::string& op);
    void   applyUnaryOp(const std::string& op);
    Value  lookupName(const std::string& name);
    Environment* newEnv(Environment* parent);
    void   pushBuiltin(Environment* env, const std::string& name);
    void   printValue(const Value& v, bool inTuple);
    static bool isBinaryOpLabel(const std::string& l);
    static bool isUnaryOpLabel(const std::string& l);
};

#endif
