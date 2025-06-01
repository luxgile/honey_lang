#pragma once

#include "ast.h"
#include "lexer.h"
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <vector>

struct Parser {
  std::vector<Token> tk_queue;

  std::vector<FnHeaderAst> fns_defined;

  std::optional<FnHeaderAst> get_function(std::string fn_name) {
    for (auto fn : fns_defined)
      if (fn.name == fn_name)
        return fn;

    return {};
  }

  bool check_tokens(std::initializer_list<TokenKind> tokens, int offset = 0) {
    if (tokens.size() + offset > tk_queue.size())
      return false;

    int i = 0;
    for (TokenKind tk : tokens) {
      if (tk != tk_queue[i + offset].kind)
        return false;
      i += 1;
    }

    return true;
  }

  std::optional<ExprAst> parse_token(Token token) {
    tk_queue.push_back(token);
    return handle_queue();
  }

  std::optional<ExprAst> handle_queue() {
    if (check_tokens({Id, Colon, Eq})) {
      auto offset = 3;
      auto header = handle_fn_header(offset);
      auto body = handle_fn_body(offset);
      if (header && body) {
        tk_queue.clear();
        return FnExprAst{{}, std::move(*header), {}};
      }
    }

    return {};
  }

  std::optional<std::unique_ptr<ExprAst>> handle_expr(int &offset) {
    if (check_tokens({Id}, offset)) {

      // Call expr
      auto fn = get_function(tk_queue[offset].value);
      if (fn) {
        offset += 1;
        std::vector<std::unique_ptr<ExprAst>> prefix_args{};

        std::vector<std::unique_ptr<ExprAst>> suffix_args{};
        int expected_arg_count = fn->suffix_args.size();
        for (int i = 0; i < expected_arg_count; i++) {
          auto expr = handle_expr(offset);
          if (!expr)
            return {};
          suffix_args.push_back(std::move(*expr));
        }
        return std::make_unique<CallExprAst>(fn->name, std::move(prefix_args),
                                             std::move(suffix_args));
      }

      // TODO: Identifiers
    }

    // TODO: Move to "primaries"
    if (check_tokens({String}, offset)) {
      return std::make_unique<StringExprAst>(tk_queue[offset].value);
    }

    return {};
  }

  std::optional<std::unique_ptr<BodyExprAst>> handle_fn_body(int &offset) {
    if (!check_tokens({LBrace}, offset))
      return {};

    std::vector<std::unique_ptr<ExprAst>> exprs{};
    offset += 1;
    while (!check_tokens({RBrace}, offset)) {
      auto expr = handle_expr(offset);
      if (!expr) return {};
      exprs.push_back(std::move(*expr));
    }

    return std::make_unique<BodyExprAst>(std::move(exprs));
  }

  std::optional<std::unique_ptr<FnHeaderAst>> handle_fn_header(int &offset) {
    std::string ret_type = "void";
    auto header_id = tk_queue[0].value;

    // Prev arguments
    auto prefix = std::vector<FieldDefAst>();
    auto arg = handle_field_def(offset);
    while (arg) {
      prefix.push_back(*arg);
      offset += 3;
      arg = handle_field_def(offset);
    }

    // Return type
    if (check_tokens({Bar}, offset)) {
      if (check_tokens({Id}, offset + 1)) {
        ret_type = tk_queue[offset + 1].value;
        offset += 1;
      } else if (!check_tokens({Bar}, offset + 1)) {
        return {};
      }
      offset += 1;
    } else {
      return {};
    }

    // Next arguments
    auto suffix = std::vector<FieldDefAst>();
    arg = handle_field_def(offset);
    while (arg) {
      suffix.push_back(*arg);
      offset += 3;
      arg = handle_field_def(offset);
    }

    return std::make_unique<FnHeaderAst>(header_id, ret_type, prefix, suffix);
  }

  std::optional<FieldDefAst> handle_field_def(int offset) {
    if (check_tokens({Id, Colon, Id}, offset)) {
      return FieldDefAst{tk_queue[offset].value, tk_queue[offset + 2].value};
    }

    return {};
  }
};
