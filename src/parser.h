#pragma once

#include "ast.h"
#include "lexer.h"
#include "program_ctx.h"
#include "v_expr_type.h"
#include <cstdio>
#include <expected>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <variant>
#include <vector>

/// Not sure if this is context for llvm ir gen or parsing...

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
        std::print("> [x] check [+{}]: ", offset);
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
          std::print("> [x] check [+{}]: ", offset);
          for (auto tk : tokens) {
            std::print("{} ", token_kind_to_string(tk));
          }
          std::print("\n");
        }
        return false;
      }
      i += 1;
    }

    if (debug_checks) {
      std::print("> [O] check [+{}]: ", offset);
      for (auto tk : tokens) {
        std::print("{} ", token_kind_to_string(tk));
      }
      std::print("\n");
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

    if (auto _struct = handle_struct_def()) {
      return _struct;
    }

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

  std::optional<uptr<StructDefAst>> handle_struct_def() {
    LOG("parsing struct");
    int offset = 0;

    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    if (check_tokens({Id, Colon, Id, Struct, LBrace}, offset) &&
        tk_queue[offset + 2].value == "=") {
      LOG("struct def found");
      auto struct_name = get_tk(offset).value().value;
      offset += 5;

      // Skip new lines
      while (check_tokens({NewLine}, offset))
        offset += 1;

      // Get struct fields
      std::vector<uptr<ArgDefAst>> fields;
      while (!check_tokens({RBrace}, offset)) {

        auto field = handle_arg_def(offset);
        if (!field)
          return {};

        if (!check_tokens({Comma}, offset))
          return {};
        offset += 1;

        // Skip new lines
        while (check_tokens({NewLine}, offset))
          offset += 1;

        fields.push_back(std::move(*field));
      }
      offset += 1;

      LOG("struct parsing successfull");
      tk_queue.clear();
      return std::make_unique<StructDefAst>(struct_name, std::move(fields));
    }

    LOG("struct parsing failed");
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

    if (check_tokens({Id, Colon, Id}, offset) &&
        tk_queue[offset + 2].value == "=") {
      LOG("fn decl found");
      auto fn_id = *get_tk(offset);
      offset += 3;
      auto header = handle_fn_header(fn_id.value, offset);
      if (!header)
        return {};

      header->get()->is_external = is_external;

      std::optional<uptr<BodyExprAst>> body = {};

      for (auto &prefix : header.value()->prefix_args) {
        ctx.defined_vars[prefix->name] =
            new VarDefStmtAst{prefix->name, prefix->type, std::nullopt};
      }
      for (auto &suffix : header.value()->suffix_args) {
        ctx.defined_vars[suffix->name] =
            new VarDefStmtAst{suffix->name, suffix->type, std::nullopt};
      }

      if (!is_external)
        body = handle_fn_body(offset);

      if (body || is_external) {
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
  std::optional<AstExpression> consume_expressions(int &offset,
                                                   TokenKind halt_token) {
    while (!check_tokens({halt_token}, offset)) {
      auto expr = handle_expr(offset);
      if (!expr) {
        line_expressions.clear();
        return {};
      }
      line_expressions.push_back(std::move(*expr));
    }
    offset += 1; // To account for the halt token.

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
    if (check_tokens({Id, Colon, Id}, offset) &&
        tk_queue[offset + 2].value == "=") {
      auto id = get_tk(offset)->value;
      offset += 3;
      auto expr = consume_expressions(offset, NewLine);
      LOG("var definition expr found");
      auto def_var =
          std::make_unique<VarDefStmtAst>(id, std::nullopt, std::move(expr));
      ctx.defined_vars[id] = def_var.get();
      return def_var;
    }

    // Var assigment
    if (check_tokens({Id, Id}, offset) && tk_queue[offset + 1].value == "=") {
      auto id = get_tk(offset)->value;
      offset += 2;
      auto expr = consume_expressions(offset, NewLine);
      if (!expr)
        return {};

      LOG("var assignment found");
      auto def_var = std::make_unique<VarAssignStmtAst>(id, std::move(*expr));
      return def_var;
    }

    // If nothing else found, try to find a expression like a call function.
    // This handles the line expressions in case prefixed arguments are found.
    auto last_expr = consume_expressions(offset, NewLine);
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

      // If the identifier is a fn but the previous call failed because not all
      // args are parsed yet, avoid creating a variable below
      if (auto overloads = ctx.get_overloads(identifier))
        return {};

      // Variable
      auto var = std::make_unique<VarExprAst>(identifier);
      LOG("var expression found");
      offset += 1;
      return var;
    }

    if (auto body = handle_fn_body(offset))
      return body;

    if (check_tokens({LPar}, offset)) {
      offset += 1;
      auto expr = consume_expressions(offset, TokenKind::RPar);
      if (!expr)
        return {};
      return std::make_unique<GroupExprAst>(std::move(*expr));
    }

    if (auto if_expr = handle_if_expr(offset))
      return if_expr;

    if (auto for_expr = handle_for_expr(offset))
      return for_expr;

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

    if (check_tokens({Bool}, offset)) {
      LOG("bool found");
      std::string i = tk_queue[offset].value;
      offset += 1;
      return std::make_unique<BoolExprAst>(i == "true" ? true : false);
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

  std::optional<uptr<IfExprAst>> handle_if_expr(int &offset) {
    if (check_tokens({If}, offset)) {
      LOG("parsing if expression");
      offset += 1;

      auto condition = consume_expressions(offset, LBrace);
      if (!condition)
        return {};

      offset -= 1;

      auto then_expr = handle_expr(offset);
      if (!then_expr)
        return {};

      std::optional<AstExpression> else_expr = std::nullopt;
      if (check_tokens({Else}, offset)) {
        offset += 1;
        else_expr = handle_expr(offset);
      }

      LOG("if expression found");
      return std::make_unique<IfExprAst>(
          std::move(*condition), std::move(*then_expr), std::move(else_expr));
    }
    return {};
  }

  std::optional<uptr<ForExprAst>> handle_for_expr(int &offset) {
    if (check_tokens({Loop}, offset)) {
      LOG("parsing for expression");
      offset += 1;

      auto for_cond_expr = consume_expressions(offset, LBrace);
      if (!for_cond_expr)
        return {};

      offset -= 1;

      auto for_body = handle_expr(offset);
      if (!for_body)
        return {};

      LOG("for expression found");
      return std::make_unique<ForExprAst>(std::move(*for_cond_expr),
                                          std::move(*for_body));
    }

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
    // Skip RBrace
    offset += 1;
    std::println("Clearing line expressions with a size of {}",
                 line_expressions.size());
    line_expressions.clear();

    LOG("got fn body");
    return std::make_unique<BodyExprAst>(std::move(stmts));
  }

  std::optional<std::unique_ptr<FnHeaderAst>>
  handle_fn_header(std::string header_id, int &offset) {
    LOG("parsing fn header");
    auto ret_type = VOID_TYPE;

    // Prev arguments
    LOG("prefix args:");
    auto prefix = handle_fn_args(offset);
    if (!prefix)
      return {};

    // Return type
    if (check_tokens({Bar}, offset)) {
      if (check_tokens({Id}, offset + 1)) {
        LOG("getting fn return type");
        ret_type = tk_queue[offset + 1].value;
        offset += 1;
      } else {
        LOG("no return type for fn");
      }

      if (!check_tokens({Bar}, offset + 1)) {
        return {};
      }

      offset += 2;
    } else {
      return {};
    }

    // Next arguments
    LOG("suffix args:");
    auto suffix = handle_fn_args(offset);
    if (!suffix)
      return {};

    LOG("found fn header");
    return std::make_unique<FnHeaderAst>(
        header_id, ret_type, false, std::move(*prefix), std::move(*suffix));
  }

  std::optional<std::vector<uptr<ArgDefAst>>> handle_fn_args(int &offset) {
    LOG("parsing fn arguments");
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
          check_tokens({NewLine}, offset) || check_tokens({EoF}, offset)) {
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
          std::make_unique<ArgDefAst>(tk_queue[offset].value, VOID_TYPE, true);
      offset += 5;
      return field;
    }

    return {};
  }
};
