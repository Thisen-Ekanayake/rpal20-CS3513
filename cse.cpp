// cse.cpp -- CSE machine implementation.
#include "cse.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

// ---------------- static helpers ----------------

bool CSEMachine::isBinaryOpLabel(const std::string& l) {
    return l == "+" || l == "-" || l == "*" || l == "/" || l == "**" ||
           l == "eq" || l == "ne" || l == "gr" || l == "ge" ||
           l == "ls" || l == "le" || l == "or" || l == "&";
}

bool CSEMachine::isUnaryOpLabel(const std::string& l) {
    return l == "neg" || l == "not";
}

static std::string extractInside(const std::string& label, std::size_t prefix) {
    // strips `<prefix...>` -> contents between prefix and last '>'.
    return label.substr(prefix, label.size() - prefix - 1);
}

// ---------------- environment ----------------

Environment* CSEMachine::newEnv(Environment* parent) {
    auto up = std::make_unique<Environment>();
    up->id = nextEnvId_++;
    up->parent = parent;
    auto* p = up.get();
    envOwn_.push_back(std::move(up));
    return p;
}

void CSEMachine::pushBuiltin(Environment* env, const std::string& name) {
    Value v;
    v.kind = Value::BUILTIN;
    v.builtinName = name;
    env->bindings[name] = v;
}

Value CSEMachine::lookupName(const std::string& name) {
    for (Environment* e = envStack_.back(); e != nullptr; e = e->parent) {
        auto it = e->bindings.find(name);
        if (it != e->bindings.end()) return it->second;
    }
    throw std::runtime_error("Undefined name: " + name);
}

// ---------------- flattening (ST -> control structures) ----------------

void CSEMachine::flattenInto(TreeNode* node, int structIdx) {
    if (!node) throw std::runtime_error("null node in flatten");
    const std::string& lab = node->label;

    if (lab == "lambda") {
        // children: [binding, body]
        if (node->children.size() != 2) {
            throw std::runtime_error("Malformed lambda after standardization");
        }
        int bodyIdx = static_cast<int>(ctrlStructs_.size());
        ctrlStructs_.emplace_back();
        flattenInto(node->children[1], bodyIdx);

        CtrlItem item;
        item.kind = CtrlItem::LAMBDA;
        item.ival = bodyIdx;
        TreeNode* bind = node->children[0];
        if (bind->label == "()") {
            item.isEmptyBind = true;
        } else if (bind->label == ",") {
            item.isCommaBind = true;
            for (TreeNode* c : bind->children) {
                if (c->label.rfind("<ID:", 0) != 0) {
                    throw std::runtime_error("Lambda comma binding contains non-id");
                }
                item.bindVars.push_back(extractInside(c->label, 4));
            }
        } else if (bind->label.rfind("<ID:", 0) == 0) {
            item.bindVars.push_back(extractInside(bind->label, 4));
        } else {
            throw std::runtime_error("Unexpected lambda binding: " + bind->label);
        }
        ctrlStructs_[structIdx].push_back(item);
        return;
    }

    if (lab == "gamma") {
        flattenInto(node->children[0], structIdx);
        flattenInto(node->children[1], structIdx);
        CtrlItem g;  g.kind = CtrlItem::GAMMA;
        ctrlStructs_[structIdx].push_back(g);
        return;
    }

    if (lab == "->") {
        // children: [cond, then, else]
        int thenIdx = static_cast<int>(ctrlStructs_.size());
        ctrlStructs_.emplace_back();
        flattenInto(node->children[1], thenIdx);

        int elseIdx = static_cast<int>(ctrlStructs_.size());
        ctrlStructs_.emplace_back();
        flattenInto(node->children[2], elseIdx);

        flattenInto(node->children[0], structIdx);  // condition

        CtrlItem b;
        b.kind  = CtrlItem::BETA;
        b.ival  = thenIdx;
        b.ival2 = elseIdx;
        ctrlStructs_[structIdx].push_back(b);
        return;
    }

    if (lab == "tau") {
        for (TreeNode* c : node->children) flattenInto(c, structIdx);
        CtrlItem t;
        t.kind = CtrlItem::TAU;
        t.ival = static_cast<long>(node->children.size());
        ctrlStructs_[structIdx].push_back(t);
        return;
    }

    if (lab == "aug") {
        flattenInto(node->children[0], structIdx);
        flattenInto(node->children[1], structIdx);
        CtrlItem a;  a.kind = CtrlItem::AUG;
        ctrlStructs_[structIdx].push_back(a);
        return;
    }

    if (lab == "<Y*>") {
        CtrlItem y;  y.kind = CtrlItem::YSTAR_;
        ctrlStructs_[structIdx].push_back(y);
        return;
    }
    if (lab == "<true>") {
        CtrlItem t;  t.kind = CtrlItem::TRUE_;
        ctrlStructs_[structIdx].push_back(t);
        return;
    }
    if (lab == "<false>") {
        CtrlItem t;  t.kind = CtrlItem::FALSE_;
        ctrlStructs_[structIdx].push_back(t);
        return;
    }
    if (lab == "<nil>") {
        CtrlItem n;  n.kind = CtrlItem::NIL_;
        ctrlStructs_[structIdx].push_back(n);
        return;
    }
    if (lab == "<dummy>") {
        CtrlItem d;  d.kind = CtrlItem::DUMMY_;
        ctrlStructs_[structIdx].push_back(d);
        return;
    }

    if (lab.rfind("<ID:", 0) == 0) {
        CtrlItem item;
        item.kind = CtrlItem::NAME;
        item.sval = extractInside(lab, 4);
        ctrlStructs_[structIdx].push_back(item);
        return;
    }
    if (lab.rfind("<INT:", 0) == 0) {
        CtrlItem item;
        item.kind = CtrlItem::INT_;
        item.ival = std::stol(extractInside(lab, 5));
        ctrlStructs_[structIdx].push_back(item);
        return;
    }
    if (lab.rfind("<STR:'", 0) == 0) {
        CtrlItem item;
        item.kind = CtrlItem::STR_;
        // strip <STR:'...'>  -> 6 chars on the left, 2 chars on the right ('>)
        item.sval = lab.substr(6, lab.size() - 8);
        ctrlStructs_[structIdx].push_back(item);
        return;
    }

    if (isBinaryOpLabel(lab)) {
        flattenInto(node->children[0], structIdx);
        flattenInto(node->children[1], structIdx);
        CtrlItem op;
        op.kind = CtrlItem::OP_BIN;
        op.sval = lab;
        ctrlStructs_[structIdx].push_back(op);
        return;
    }
    if (isUnaryOpLabel(lab)) {
        flattenInto(node->children[0], structIdx);
        CtrlItem op;
        op.kind = CtrlItem::OP_UN;
        op.sval = lab;
        ctrlStructs_[structIdx].push_back(op);
        return;
    }

    throw std::runtime_error("Unknown ST node label: " + lab);
}

// ---------------- machine driver ----------------

void CSEMachine::run(TreeNode* st) {
    // Build the primitive environment.
    Environment* env0 = newEnv(nullptr);
    for (const char* nm : {
            "Print", "print", "Conc", "Stem", "Stern", "Order", "ItoS",
            "Null", "Isstring", "Isinteger", "Istruthvalue", "Istuple",
            "Isfunction", "Isdummy"
        }) {
        pushBuiltin(env0, nm);
    }
    {
        Value nilV;
        nilV.kind = Value::TUPLE_;  // nil is an empty tuple
        env0->bindings["nil"] = nilV;
    }
    envStack_.push_back(env0);

    // Build all control structures (top-level is index 0).
    ctrlStructs_.emplace_back();
    flattenInto(st, 0);

    // Initialize control with the top-level structure.
    const auto& top = ctrlStructs_[0];
    control_.insert(control_.end(), top.begin(), top.end());

    while (!control_.empty()) step();

    // At the end of the program emit a trailing newline (rpal.exe convention).
    std::fputc('\n', stdout);
}

// ---------------- step ----------------

void CSEMachine::step() {
    CtrlItem item = control_.front();
    control_.pop_front();

    switch (item.kind) {
        case CtrlItem::NAME: {
            stack_.push_back(lookupName(item.sval));
            break;
        }
        case CtrlItem::INT_: {
            Value v;  v.kind = Value::INT_;  v.intVal = item.ival;
            stack_.push_back(v);
            break;
        }
        case CtrlItem::STR_: {
            Value v;  v.kind = Value::STR_;  v.strVal = item.sval;
            stack_.push_back(v);
            break;
        }
        case CtrlItem::TRUE_: {
            Value v;  v.kind = Value::BOOL_;  v.boolVal = true;
            stack_.push_back(v);
            break;
        }
        case CtrlItem::FALSE_: {
            Value v;  v.kind = Value::BOOL_;  v.boolVal = false;
            stack_.push_back(v);
            break;
        }
        case CtrlItem::NIL_: {
            Value v;  v.kind = Value::TUPLE_;
            stack_.push_back(v);
            break;
        }
        case CtrlItem::DUMMY_: {
            Value v;  v.kind = Value::DUMMY_;
            stack_.push_back(v);
            break;
        }
        case CtrlItem::YSTAR_: {
            Value v;  v.kind = Value::YSTAR_;
            stack_.push_back(v);
            break;
        }
        case CtrlItem::LAMBDA: {
            Value v;
            v.kind         = Value::LAMBDA_CL;
            v.ctrlIdx      = static_cast<int>(item.ival);
            v.bindVars     = item.bindVars;
            v.isCommaBind  = item.isCommaBind;
            v.isEmptyBind  = item.isEmptyBind;
            v.env          = envStack_.back();
            stack_.push_back(v);
            break;
        }
        case CtrlItem::GAMMA: {
            applyGamma();
            break;
        }
        case CtrlItem::TAU: {
            long n = item.ival;
            if (static_cast<long>(stack_.size()) < n) {
                throw std::runtime_error("tau: stack underflow");
            }
            Value t;
            t.kind = Value::TUPLE_;
            t.tupleVal.resize(n);
            for (long i = n - 1; i >= 0; --i) {
                t.tupleVal[i] = stack_.back();
                stack_.pop_back();
            }
            stack_.push_back(t);
            break;
        }
        case CtrlItem::AUG: {
            Value e = stack_.back();  stack_.pop_back();
            Value t = stack_.back();  stack_.pop_back();
            if (t.kind != Value::TUPLE_) {
                throw std::runtime_error("aug: left operand must be a tuple/nil");
            }
            t.tupleVal.push_back(e);
            stack_.push_back(t);
            break;
        }
        case CtrlItem::BETA: {
            Value c = stack_.back();  stack_.pop_back();
            if (c.kind != Value::BOOL_) {
                throw std::runtime_error("conditional: predicate is not boolean");
            }
            int chosen = c.boolVal ? static_cast<int>(item.ival)
                                   : static_cast<int>(item.ival2);
            const auto& body = ctrlStructs_[chosen];
            control_.insert(control_.begin(), body.begin(), body.end());
            break;
        }
        case CtrlItem::ENV_EXIT: {
            Value result = stack_.back();  stack_.pop_back();
            if (stack_.empty() || stack_.back().kind != Value::ENV_MARKER) {
                throw std::runtime_error("ENV_EXIT: missing env marker on stack");
            }
            stack_.pop_back();
            envStack_.pop_back();
            stack_.push_back(result);
            break;
        }
        case CtrlItem::OP_BIN: {
            applyBinaryOp(item.sval);
            break;
        }
        case CtrlItem::OP_UN: {
            applyUnaryOp(item.sval);
            break;
        }
        case CtrlItem::LITERAL_VALUE: {
            stack_.push_back(item.lit);
            break;
        }
    }
}

// ---------------- gamma ----------------

void CSEMachine::applyGamma() {
    if (stack_.size() < 2) {
        throw std::runtime_error("gamma: stack underflow");
    }
    Value rand  = stack_.back();  stack_.pop_back();
    Value rator = stack_.back();  stack_.pop_back();

    switch (rator.kind) {
        case Value::LAMBDA_CL: {
            Environment* parent = rator.env;
            Environment* e = newEnv(parent);
            if (rator.isEmptyBind) {
                // Nothing to bind.  The argument is simply discarded; this
                // form mirrors rpal.exe's behaviour for `fn (). E`.
                (void)rand;
            } else if (rator.isCommaBind) {
                if (rand.kind != Value::TUPLE_ ||
                    rand.tupleVal.size() != rator.bindVars.size()) {
                    throw std::runtime_error(
                        "lambda destructure: tuple shape mismatch");
                }
                for (std::size_t i = 0; i < rator.bindVars.size(); ++i) {
                    e->bindings[rator.bindVars[i]] = rand.tupleVal[i];
                }
            } else {
                e->bindings[rator.bindVars[0]] = rand;
            }
            envStack_.push_back(e);

            Value mark;
            mark.kind  = Value::ENV_MARKER;
            mark.env   = e;
            mark.envId = e->id;
            stack_.push_back(mark);

            // Splice the lambda body's control items + ENV_EXIT at the front.
            CtrlItem exitItem;
            exitItem.kind = CtrlItem::ENV_EXIT;
            exitItem.ival = e->id;
            control_.insert(control_.begin(), exitItem);
            const auto& body = ctrlStructs_[rator.ctrlIdx];
            control_.insert(control_.begin(), body.begin(), body.end());
            break;
        }
        case Value::YSTAR_: {
            // Y* applied to a lambda -> eta closure.
            if (rand.kind != Value::LAMBDA_CL) {
                throw std::runtime_error("Y*: expected a lambda argument");
            }
            Value eta = rand;
            eta.kind  = Value::ETA;
            stack_.push_back(eta);
            break;
        }
        case Value::ETA: {
            // Apply eta to arg: rewrite as two gammas with a stashed
            // PUSH(arg) between them so the recursive lambda's body sees
            //    arg = X,  X = eta.
            //
            //   Stack:  ... CL eta          (rator unfolded; arg saved)
            //   Ctrl:   GAMMA  PUSH(arg)  GAMMA  ...rest...
            Value cl = rator;
            cl.kind  = Value::LAMBDA_CL;

            stack_.push_back(cl);
            stack_.push_back(rator);  // eta back on top

            CtrlItem gam2; gam2.kind = CtrlItem::GAMMA;
            CtrlItem pushArg;  pushArg.kind = CtrlItem::LITERAL_VALUE;
            pushArg.lit = rand;
            CtrlItem gam1; gam1.kind = CtrlItem::GAMMA;

            control_.insert(control_.begin(), gam2);
            control_.insert(control_.begin(), pushArg);
            control_.insert(control_.begin(), gam1);
            break;
        }
        case Value::TUPLE_: {
            // Tuple indexing: rand is an integer (1-based) per RPAL.
            if (rand.kind != Value::INT_) {
                throw std::runtime_error("Tuple index: integer expected");
            }
            long i = rand.intVal;
            if (i < 1 || static_cast<std::size_t>(i) > rator.tupleVal.size()) {
                throw std::runtime_error("Tuple index out of range");
            }
            stack_.push_back(rator.tupleVal[i - 1]);
            break;
        }
        case Value::BUILTIN: {
            applyBuiltin(rator, rand);
            break;
        }
        default:
            throw std::runtime_error("gamma: rator is not applicable");
    }
}

// ---------------- builtin operators ----------------

void CSEMachine::applyBinaryOp(const std::string& op) {
    if (stack_.size() < 2) {
        throw std::runtime_error("binary op: stack underflow (" + op + ")");
    }
    Value rhs = stack_.back();  stack_.pop_back();
    Value lhs = stack_.back();  stack_.pop_back();

    Value out;
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "**") {
        if (lhs.kind != Value::INT_ || rhs.kind != Value::INT_) {
            throw std::runtime_error("Illegal operands for '" + op + "'");
        }
        long a = lhs.intVal, b = rhs.intVal, r = 0;
        if      (op == "+") r = a + b;
        else if (op == "-") r = a - b;
        else if (op == "*") r = a * b;
        else if (op == "/") {
            if (b == 0) throw std::runtime_error("Division by zero");
            r = a / b;
        } else { // **
            r = 1;
            long base = a;
            long exp  = b;
            if (exp < 0) throw std::runtime_error("Negative exponent");
            for (long i = 0; i < exp; ++i) r *= base;
        }
        out.kind = Value::INT_;
        out.intVal = r;
    }
    else if (op == "gr" || op == "ge" || op == "ls" || op == "le") {
        if (lhs.kind != Value::INT_ || rhs.kind != Value::INT_) {
            throw std::runtime_error("Illegal operands for '" + op + "'");
        }
        bool b = false;
        if      (op == "gr") b = lhs.intVal >  rhs.intVal;
        else if (op == "ge") b = lhs.intVal >= rhs.intVal;
        else if (op == "ls") b = lhs.intVal <  rhs.intVal;
        else                 b = lhs.intVal <= rhs.intVal;
        out.kind = Value::BOOL_;
        out.boolVal = b;
    }
    else if (op == "eq" || op == "ne") {
        bool eq;
        if (lhs.kind == Value::INT_ && rhs.kind == Value::INT_) {
            eq = (lhs.intVal == rhs.intVal);
        } else if (lhs.kind == Value::STR_ && rhs.kind == Value::STR_) {
            eq = (lhs.strVal == rhs.strVal);
        } else if (lhs.kind == Value::BOOL_ && rhs.kind == Value::BOOL_) {
            eq = (lhs.boolVal == rhs.boolVal);
        } else {
            throw std::runtime_error("Illegal operands for '" + op + "'");
        }
        out.kind = Value::BOOL_;
        out.boolVal = (op == "eq") ? eq : !eq;
    }
    else if (op == "or" || op == "&") {
        if (lhs.kind != Value::BOOL_ || rhs.kind != Value::BOOL_) {
            throw std::runtime_error("Illegal operands for '" + op + "'");
        }
        out.kind = Value::BOOL_;
        out.boolVal = (op == "or") ? (lhs.boolVal || rhs.boolVal)
                                   : (lhs.boolVal && rhs.boolVal);
    }
    else {
        throw std::runtime_error("Unknown binary op: " + op);
    }
    stack_.push_back(out);
}

void CSEMachine::applyUnaryOp(const std::string& op) {
    if (stack_.empty()) {
        throw std::runtime_error("unary op: stack underflow");
    }
    Value v = stack_.back();  stack_.pop_back();
    Value out;
    if (op == "neg") {
        if (v.kind != Value::INT_) {
            throw std::runtime_error("Illegal operand for 'neg'");
        }
        out.kind = Value::INT_;
        out.intVal = -v.intVal;
    } else if (op == "not") {
        if (v.kind != Value::BOOL_) {
            throw std::runtime_error("Illegal operand for 'not'");
        }
        out.kind = Value::BOOL_;
        out.boolVal = !v.boolVal;
    } else {
        throw std::runtime_error("Unknown unary op: " + op);
    }
    stack_.push_back(out);
}

// ---------------- builtin functions ----------------

void CSEMachine::applyBuiltin(const Value& fn, const Value& arg) {
    const std::string& name = fn.builtinName;
    Value out;

    if (name == "Print" || name == "print") {
        printValue(arg, false);
        out.kind = Value::DUMMY_;
    }
    else if (name == "Conc") {
        if (!fn.partialArg) {
            // First (partial) application.
            if (arg.kind != Value::STR_) {
                throw std::runtime_error("Conc: argument must be a string");
            }
            out = fn;
            out.partialArg = std::make_shared<Value>(arg);
            // Conc is curried (Conc x y).  When the first gamma fires
            // the second argument still needs another gamma to be consumed,
            // so we do NOT eat an extra control item here.  The flattened
            // form is `gamma(gamma(Conc, x), y)`.
        } else {
            if (arg.kind != Value::STR_) {
                throw std::runtime_error("Conc: second argument must be a string");
            }
            out.kind = Value::STR_;
            out.strVal = fn.partialArg->strVal + arg.strVal;
        }
    }
    else if (name == "Stem") {
        if (arg.kind != Value::STR_) {
            throw std::runtime_error("Stem: argument must be a string");
        }
        out.kind = Value::STR_;
        if (!arg.strVal.empty()) out.strVal = std::string(1, arg.strVal[0]);
    }
    else if (name == "Stern") {
        if (arg.kind != Value::STR_) {
            throw std::runtime_error("Stern: argument must be a string");
        }
        out.kind = Value::STR_;
        if (arg.strVal.size() > 1) out.strVal = arg.strVal.substr(1);
    }
    else if (name == "Order") {
        if (arg.kind != Value::TUPLE_) {
            throw std::runtime_error("Order: argument must be a tuple");
        }
        out.kind = Value::INT_;
        out.intVal = static_cast<long>(arg.tupleVal.size());
    }
    else if (name == "ItoS") {
        if (arg.kind != Value::INT_) {
            throw std::runtime_error("ItoS: argument must be an integer");
        }
        out.kind = Value::STR_;
        out.strVal = std::to_string(arg.intVal);
    }
    else if (name == "Null") {
        out.kind = Value::BOOL_;
        out.boolVal = (arg.kind == Value::TUPLE_ && arg.tupleVal.empty());
    }
    else if (name == "Isstring") {
        out.kind = Value::BOOL_;  out.boolVal = (arg.kind == Value::STR_);
    }
    else if (name == "Isinteger") {
        out.kind = Value::BOOL_;  out.boolVal = (arg.kind == Value::INT_);
    }
    else if (name == "Istruthvalue") {
        out.kind = Value::BOOL_;  out.boolVal = (arg.kind == Value::BOOL_);
    }
    else if (name == "Istuple") {
        out.kind = Value::BOOL_;
        // rpal.exe considers nil a tuple.  Non-empty tuples are tuples too.
        out.boolVal = (arg.kind == Value::TUPLE_);
    }
    else if (name == "Isfunction") {
        out.kind = Value::BOOL_;
        out.boolVal = (arg.kind == Value::LAMBDA_CL || arg.kind == Value::ETA);
    }
    else if (name == "Isdummy") {
        out.kind = Value::BOOL_;
        out.boolVal = (arg.kind == Value::DUMMY_);
    }
    else {
        throw std::runtime_error("Unknown builtin: " + name);
    }
    stack_.push_back(out);
}

// ---------------- printing ----------------

void CSEMachine::printValue(const Value& v, bool inTuple) {
    switch (v.kind) {
        case Value::INT_:
            std::printf("%ld", v.intVal);
            break;
        case Value::STR_:
            // Inside Print the escapes have already been interpreted at lex
            // time, so we just emit the raw bytes.
            std::fwrite(v.strVal.data(), 1, v.strVal.size(), stdout);
            break;
        case Value::BOOL_:
            std::printf("%s", v.boolVal ? "true" : "false");
            break;
        case Value::DUMMY_:
            std::printf("dummy");
            break;
        case Value::TUPLE_:
            if (v.tupleVal.empty()) {
                std::printf("nil");
            } else {
                std::printf("(");
                for (std::size_t i = 0; i < v.tupleVal.size(); ++i) {
                    if (i) std::printf(", ");
                    printValue(v.tupleVal[i], true);
                }
                std::printf(")");
            }
            break;
        case Value::LAMBDA_CL: {
            const char* bv = "(null)";
            if (!v.isEmptyBind && !v.isCommaBind && !v.bindVars.empty()) {
                bv = v.bindVars.front().c_str();
            }
            std::printf("[lambda closure: %s: %d]", bv, v.ctrlIdx);
            break;
        }
        case Value::ETA: {
            const char* bv = "(null)";
            if (!v.isEmptyBind && !v.isCommaBind && !v.bindVars.empty()) {
                bv = v.bindVars.front().c_str();
            }
            std::printf("[eta closure: %s: %d]", bv, v.ctrlIdx);
            break;
        }
        case Value::YSTAR_:
            std::printf("<Y*>");
            break;
        case Value::ENV_MARKER:
            std::printf("<env:%d>", v.envId);
            break;
        case Value::BUILTIN:
            std::printf("%s", v.builtinName.c_str());
            break;
    }
    (void)inTuple;
}
