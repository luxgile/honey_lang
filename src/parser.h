#pragma once

#include "ast.h"
#include "lexer.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

/// Holds a group of functions with the same name but different definitions
struct OverloadFnGroup {
  std::vector<FnHeaderAst *> fns;

  // TODO: Terribly inneficient. Start using types as ids instead of strings to
  // improve checks.

  bool eq_arg_types(std::vector<uptr<ArgDefAst>> &lhs,
                    std::vector<std::string> &rhs) {
    if (lhs.size() != rhs.size())
      return false;

    if (lhs.size() != rhs.size())
      return false;

    for (int i = 0; i < (int)lhs.size(); i++) {
      if (lhs[i]->type != rhs[i])
        return false;
    }

    return true;
  }

  /// Returns the fn and the index it was found.
  std::optional<std::tuple<int, FnHeaderAst *>>
  get_fn(std::vector<std::string> pre, std::vector<std::string> suf) {
    for (int i = 0; i < (int)fns.size(); i++) {
      auto fn = fns[i];

      if (!eq_arg_types(fn->prefix_args, pre))
        continue;

      if (!eq_arg_types(fn->suffix_args, suf))
        continue;

      return std::tuple(i, fn);
    }
    return {};
  }
};

/// Not sure if this is context for llvm ir gen or parsing...
struct ProgramCtx {
  /* std::vector<FnHeaderAst *> defined_ext_fns; */
  /* std::vector<FnDefAst *> defined_fns; */

private:
  std::map<std::string, uptr<OverloadFnGroup>> fns;
  std::map<std::string, uptr<OverloadFnGroup>> ext_fns;
  std::map<std::string, llvm::Type *> types;
  std::map<std::string, uptr<VarExprAst>> variables;
  std::map<std::string, uptr<MetaFunction>> defined_meta;

public:
  std::map<std::string, VarDefStmtAst *> defined_vars;

  void define_fn(std::string name, FnHeaderAst *fn) {
    auto overloads = fns[name].get();
    if (overloads == nullptr) {
      fns[name] = std::make_unique<OverloadFnGroup>();
      overloads = fns[name].get();
    }
    overloads->fns.push_back(fn);
  }

  std::optional<std::tuple<int, FnHeaderAst *>>
  get_fn(std::string name, std::vector<std::string> pre,
         std::vector<std::string> suf) {
    auto overloads = fns[name].get();
    if (overloads == nullptr)
      return std::nullopt;
    return overloads->get_fn(pre, suf);
  }

  void define_fn_ext(std::string name, FnHeaderAst *fn) {
    auto overloads = ext_fns[name].get();
    if (overloads == nullptr) {
      fns[name] = std::make_unique<OverloadFnGroup>();
      overloads = ext_fns[name].get();
    }
    overloads->fns.push_back(fn);
  }

  std::optional<std::tuple<int, FnHeaderAst *>>
  get_fn_ext(std::string name, std::vector<std::string> pre,
             std::vector<std::string> suf) {
    auto overloads = ext_fns[name].get();
    if (overloads == nullptr)
      return std::nullopt;
    return overloads->get_fn(pre, suf);
  }

  std::optional<OverloadFnGroup *> get_overloads(std::string name) {
    auto fn = fns[name].get();
    if (fn != nullptr)
      return fn;

    auto fn_ext = ext_fns[name].get();
    if (fn_ext != nullptr)
      return fn_ext;

    return std::nullopt;
  }

  std::vector<FnHeaderAst *> get_all_ext_fn() {
    std::vector<FnHeaderAst *> fns;
    for (auto it = ext_fns.begin(); it != ext_fns.end(); it++) {
      for (auto fn : it->second.get()->fns) {
        fns.push_back(fn);
      }
    }
    return fns;
  }

  void define_type(std::string name, llvm::Type *type) { types[name] = type; }

  std::optional<llvm::Type *> get_type(std::string name) {
    auto type = types[name];
    if (type == nullptr)
      return std::nullopt;
    return type;
  }

  void define_var(std::string name, uptr<VarExprAst> &value) {
    variables[name] = std::move(value);
  }

  std::optional<VarExprAst *> get_var(std::string name) {
    auto var = &variables[name];
    if (var == nullptr)
      return std::nullopt;
    return var->get();
  }

  void define_meta(std::string name, uptr<MetaFunction> meta) {
    defined_meta[name] = std::move(meta);
  }

  std::optional<MetaFunction *> get_meta(std::string name) {
    auto meta = &defined_meta[name];
    if (meta->get() == nullptr)
      return std::nullopt;
    return meta->get();
  }
};

struct AstExprTypeVisitor {
  ProgramCtx *ctx;

  const std::string UNDEFINED = "Undefined";

  std::string operator()(uptr<IntExprAst> &node) { return "Int"; }

  std::string operator()(uptr<FloatExprAst> &node) { return "Float"; }

  std::string operator()(uptr<StringExprAst> &node) { return "RawString"; }

  std::string operator()(uptr<VarExprAst> &node) {
    auto def_var = ctx->defined_vars[node->name];
    if (!def_var)
      return UNDEFINED;

    if (def_var->type)
      return *def_var->type;

    if (def_var->assignment)
      return std::visit(*this, *def_var->assignment);

    return UNDEFINED;
  }

  std::string operator()(uptr<ArgDefAst> &node) { return node->type; }

  // Expressions
  std::string operator()(uptr<CallExprAst> &node) {
    auto overloads = ctx->get_overloads(node->fn_name);
    if (!overloads)
      return UNDEFINED;
    std::vector<std::string> prefix_types;
    for (auto &pre : node->prefix_args) {
      prefix_types.push_back(std::visit(*this, pre));
    }
    std::vector<std::string> suffix_types;
    for (auto &suf : node->suffix_args) {
      suffix_types.push_back(std::visit(*this, suf));
    }

    auto fn = overloads.value()->get_fn(prefix_types, suffix_types);
    auto ret_type = std::get<1>(*fn)->ret_type;
    return ret_type ? *ret_type : "Void";
  }

  std::string operator()(uptr<BodyExprAst> &node) { return "Void"; }

  std::string operator()(uptr<StatementExprAst> &node) {
    return std::visit(*this, node->expr);
  }

  std::string operator()(uptr<MetaDefExprAst> &node) {
    auto meta = ctx->get_meta(node->name);
    switch (meta.value()->kind) {
    case MetaFunctionKind::AddInt:
      return "Int";
    case MetaFunctionKind::AddFloat:
      return "Float";
    default:
      return UNDEFINED;
    }
  }
};

struct Parser {
  bool debug_scan = false;
  bool debug_checks = false;
  ProgramCtx ctx;
  AstExprTypeVisitor type_visitor;

  std::vector<Token> tk_queue;

  /// Holds all expressions not used in a line. Used for prefix arguments.
  std::vector<AstExpression> line_expressions;

  /// Meta tags fn defined and not used
  std::vector<uptr<MetaDefExprAst>> line_meta_def;

  Parser() { type_visitor.ctx = &ctx; }

#define LOG(msg)                                                               \
  if (debug_scan)                                                              \
  std::println(msg)

  std::optional<Token> get_tk(int offset) {
    if (offset >= (int)tk_queue.size())
      return {};
    return tk_queue[offset];
  }

  /* std::optional<std::reference_wrapper<FnHeaderAst>> */
  /* get_function(std::string &fn_name) { */
  /*   for (auto &fn : ctx.defined_ext_fns) */
  /*     if (fn->name == fn_name) */
  /*       return std::ref(*fn); */
  /**/
  /*   for (auto &fn : ctx.defined_fns) */
  /*     if (fn->fn_header->name == fn_name) */
  /*       return std::ref(*fn->fn_header); */
  /**/
  /*   return {}; */
  /* } */

  bool check_tokens(std::initializer_list<TokenKind> tokens, int offset = 0) {
    if (tokens.size() + offset > tk_queue.size()) {
      if (debug_checks) {
        std::print("> failed check [+{}]: ", offset);
        for (auto tk : tokens) {
          std::print("{} ", token_kind_to_string(tk));
        }
        std::print("\n");
      }
      return false;
    }

    int i = 0;
    for (TokenKind tk : tokens) {
      if (tk != tk_queue[i + offset].kind) {
        if (debug_checks) {
          std::print("> failed check [+{}]: ", offset);
          for (auto tk : tokens) {
            std::print("{} ", token_kind_to_string(tk));
          }
          std::print("\n");
        }
        return false;
      }
      i += 1;
    }

    return true;
  }

  std::optional<AstStatement> parse_token(Token token) {
    tk_queue.push_back(token);

    if (debug_checks) {
      std::print("[");
      int i = 0;
      for (auto tk : tk_queue) {
        std::print("{} {},", i++, token_kind_to_string(tk.kind));
      }
      std::print("]\n");
    }

    return handle_queue();
  }

  std::optional<AstStatement> handle_queue() {
    LOG("starting parsing...");

    if (auto fn = handle_fn_def()) {
      return fn;
    }

    int offset = 0;
    if (auto meta = handle_meta_def(offset)) {
      // Meta functions at file level are meant to be used for the following
      // statement.
      // TODO: This is NOT the case for @IF or similar for conditional
      // compilation.
      line_meta_def.push_back(std::move(*meta));
      LOG("meta body function found.");

      tk_queue.clear();
      return {};
    }

    return {};
  }

  std::optional<uptr<MetaDefExprAst>> handle_meta_def(int &offset) {
    LOG("parsing meta");
    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    if (check_tokens({Meta}, offset)) {
      auto meta_tk = get_tk(offset).value();
      offset += 1;

      auto meta_fn = ctx.get_meta(meta_tk.value);
      if (!meta_fn)
        return {};

      std::vector<AstExpression> args;
      for (int i = 0; i < meta_fn.value()->arg_num(); i++) {
        auto expr = handle_expr(offset);
        if (!expr)
          return {};
        args.push_back(std::move(*expr));
      }

      LOG("meta found");
      return std::make_unique<MetaDefExprAst>(meta_tk.value, std::move(args));
    }

    LOG("parsing meta failed");
    return {};
  }

  std::optional<uptr<FnDefAst>> handle_fn_def() {
    int offset = 0;

    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    bool is_external = false;
    if (check_tokens({Extern}, offset)) {
      is_external = true;
      offset += 1;
    }

    if (check_tokens({Id, Colon, Eq}, offset)) {
      LOG("fn decl found");
      auto fn_id = *get_tk(offset);
      offset += 3;
      auto header = handle_fn_header(fn_id.value, offset);
      if (header)
        header->get()->is_external = is_external;

      std::optional<uptr<BodyExprAst>> body = {};

      if (!is_external)
        body = handle_fn_body(offset);

      if (header && (body || is_external)) {
        tk_queue.clear();
        LOG("got fn expression");
        auto fn =
            std::make_unique<FnDefAst>(std::move(*header), std::move(body));
        ctx.define_fn(fn->fn_header->name, fn->fn_header.get());
        return fn;
      }
    }

    return {};
  }

  /// Will read all expressions in a line and merge unused ones into prefix
  /// arguments.
  std::optional<AstExpression> consume_expressions(int &offset) {
    while (!check_tokens({NewLine}, offset)) {
      auto expr = handle_expr(offset);
      if (!expr)
        return {};
      line_expressions.push_back(std::move(*expr));
    }
    offset += 1; // To account for the new line.

    if (line_expressions.size() == 0)
      return {};
    auto last_expr = std::move(line_expressions.back());
    line_expressions.pop_back();
    if (line_expressions.size() > 0)
      std::println("!! line expressions unnused: {}", line_expressions.size());
    line_expressions.clear();
    return std::move(last_expr);
  }

  std::optional<AstStatement> handle_statement(int &offset) {
    LOG("starting statement parsing");

    // Var declaration
    if (check_tokens({Id, Colon, Eq}, offset)) {
      auto id = get_tk(offset)->value;
      offset += 3;
      auto expr = consume_expressions(offset);
      LOG("var definition expr found");
      auto def_var =
          std::make_unique<VarDefStmtAst>(id, std::nullopt, std::move(expr));
      ctx.defined_vars[id] = def_var.get();
      return def_var;
    }

    // If nothing else found, try to find a expression like a call function.
    // This handles the line expressions in case prefixed arguments are found.
    auto last_expr = consume_expressions(offset);
    if (!last_expr)
      return {};

    LOG("statement expr found");
    auto stmt_expr = std::make_unique<StatementExprAst>(std::move(*last_expr));
    return stmt_expr;
  }

  std::optional<AstExpression> handle_expr(int &offset) {
    LOG("parsing expression");
    if (check_tokens({Id}, offset)) {
      auto identifier = get_tk(offset).value().value;

      // Call expr
      if (auto call = handle_call_expr(offset))
        return call;

      // Variable
      auto var = std::make_unique<VarExprAst>(identifier);
      LOG("var expression found");
      offset += 1;
      return var;
    }

    if (check_tokens({Meta}, offset)) {
      auto meta = handle_meta_def(offset);
      return meta;
    }

    if (check_tokens({String}, offset)) {
      LOG("string found");
      std::string str = tk_queue[offset].value;
      offset += 1;
      return std::make_unique<StringExprAst>(str);
    }

    if (check_tokens({Int}, offset)) {
      LOG("int found");
      std::string i = tk_queue[offset].value;
      offset += 1;
      return std::make_unique<IntExprAst>(std::stoi(i));
    }

    if (check_tokens({Float}, offset)) {
      LOG("float found");
      std::string f = tk_queue[offset].value;
      offset += 1;
      return std::make_unique<FloatExprAst>(std::stof(f));
    }

    LOG("no expression found");
    return {};
  }

  std::optional<std::unique_ptr<CallExprAst>> handle_call_expr(int &offset) {
    LOG("parsing call");
    auto identifier = get_tk(offset).value().value;
    auto overloads = ctx.get_overloads(identifier);
    if (!overloads) {
      LOG("parsing call failed - no overload found");
      return {};
    }

    // Consume the identifier

    auto tmp_offset = offset;
    tmp_offset += 1;

    for (auto fn : overloads.value()->fns) {
      std::vector<AstExpression> prefix_args{};
      bool matched_args = true;
      for (int i = 0;
           i < (int)line_expressions.size() && i < (int)fn->prefix_args.size();
           i++) {
        if (std::visit(type_visitor, line_expressions[i]) !=
            fn->prefix_args[i]->type) {
          matched_args = false;
          break;
        }
        std::println(" >>> adding line expression call as prefix");
        prefix_args.push_back(std::move(line_expressions[i]));
      }

      if (!matched_args)
        continue;

      matched_args = true;
      line_expressions.clear();

      std::vector<AstExpression> suffix_args{};
      if (fn->is_vararic()) {
        int i = 0;
        while (true) {
          if (check_tokens({NewLine}, tmp_offset)) {
            break;
          }

          auto expr = handle_expr(tmp_offset);
          if (!expr)
            return {};

          if (!fn->is_vararic() &&
              (i >= (int)fn->suffix_args.size() ||
               std::visit(type_visitor, *expr) != fn->suffix_args[i]->type)) {
            matched_args = false;
            break;
          }

          suffix_args.push_back(std::move(*expr));
          i += 1;
        }

        if (!matched_args)
          continue;

      } else {
        int expected_arg_count = fn->suffix_args.size();
        for (int i = 0; i < expected_arg_count; i++) {
          auto expr = handle_expr(tmp_offset);
          if (!expr)
            return {};

          if (i >= (int)fn->suffix_args.size() ||
              (!fn->suffix_args[i]->is_varadic &&
               std::visit(type_visitor, *expr) != fn->suffix_args[i]->type)) {
            matched_args = false;
            break;
          }

          suffix_args.push_back(std::move(*expr));
        }

        if (!matched_args)
          continue;
      }

      LOG("call expr found");
      offset = tmp_offset;
      return std::make_unique<CallExprAst>(fn->name, std::move(prefix_args),
                                           std::move(suffix_args));
    }

    LOG("parsing call failed");
    return {};
  }

  std::optional<std::unique_ptr<BodyExprAst>> handle_fn_body(int &offset) {
    if (!check_tokens({LBrace}, offset))
      return {};

    offset += 1;
    while (check_tokens({NewLine}, offset)) { // Ignore new line
      offset += 1;
    }

    LOG("starting fn body");
    std::vector<AstStatement> stmts{};
    // TODO: Move statement creation and line expressions to a different
    // place. So lines and whole body parsing is separated and less of a mess.
    while (!check_tokens({RBrace}, offset)) {
      auto stmt = handle_statement(offset);
      if (!stmt)
        return {};
      stmts.push_back(std::move(*stmt));

      // Skip all new lines
      while (check_tokens({NewLine}, offset))
        offset += 1;
    }
    std::println("Clearing line expressions with a size of {}",
                 line_expressions.size());
    line_expressions.clear();

    LOG("got fn body");
    return std::make_unique<BodyExprAst>(std::move(stmts));
  }

  std::optional<std::unique_ptr<FnHeaderAst>>
  handle_fn_header(std::string header_id, int &offset) {
    std::optional<std::string> ret_type = std::nullopt;

    // Prev arguments
    auto prefix = handle_fn_args(offset);
    if (!prefix)
      return {};

    // Return type
    if (check_tokens({Bar}, offset)) {
      if (check_tokens({Id}, offset + 1)) {
        ret_type = tk_queue[offset + 1].value;
        offset += 1;
      }

      if (!check_tokens({Bar}, offset + 1)) {
        return {};
      }

      offset += 2;
    } else {
      return {};
    }

    // Next arguments
    auto suffix = handle_fn_args(offset);
    if (!suffix)
      return {};

    LOG("found fn header");
    return std::make_unique<FnHeaderAst>(
        header_id, ret_type, false, std::move(*prefix), std::move(*suffix));
  }

  std::optional<std::vector<uptr<ArgDefAst>>> handle_fn_args(int &offset) {
    LOG("getting fn arguments");
    auto args = std::vector<uptr<ArgDefAst>>();

    if (check_tokens({Bar}, offset) || check_tokens({LBrace}, offset) ||
        check_tokens({NewLine}, offset)) {
      if (check_tokens({NewLine}, offset))
        offset += 1;

      return args;
    }

    while (true) {
      auto arg = handle_arg_def(offset);
      if (!arg)
        return {};

      LOG("found argument for fn header");
      args.push_back(std::move(*arg));

      if (check_tokens({Comma}, offset)) {
        offset += 1;
        continue;
      }

      if (check_tokens({Bar}, offset) || check_tokens({LBrace}, offset) ||
          check_tokens({NewLine}, offset)) {
        if (check_tokens({NewLine}, offset))
          offset += 1;

        break;
      }

      return {};
    }

    LOG("all arguments found for fn header");
    return args;
  }

  std::optional<uptr<ArgDefAst>> handle_arg_def(int &offset) {
    if (check_tokens({Id, Colon, Id}, offset)) {
      auto field = std::make_unique<ArgDefAst>(
          tk_queue[offset].value, tk_queue[offset + 2].value, false);
      offset += 3;
      return field;
    }

    // Varadic argument
    if (check_tokens({Id, Colon, Dot, Dot, Dot}, offset)) {
      auto field =
          std::make_unique<ArgDefAst>(tk_queue[offset].value, "Void", true);
      offset += 5;
      return field;
    }

    return {};
  }
};
