#pragma once

#include "ast.h"
#include "helpers.h"
#include "lexer.h"
#include "program_ctx.h"
#include "types.h"
#include "v_expr_type.h"
#include <algorithm>
#include <cstdio>
#include <expected>
#include <format>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

struct ParserError {
  FilePos pos;
  std::string msg;

  static void print_error(const ParserError &error, std::string source);

  static ParserError type_not_found(Token curr, std::string type_name) {
    return ParserError{curr.position,
                       std::format("use of undeclared type '{}'", type_name)};
  }

  static ParserError
  unexpected_token(Token curr, std::initializer_list<TokenKind> expected) {
    std::string msg;
    for (auto kind : expected) {
      msg += token_kind_to_string(kind) + ",";
    }
    msg.pop_back();
    msg += std::format(" was expected, but {} was found instead",
                       token_kind_to_string(curr.kind));
    return ParserError{curr.position, msg};
  }

  static ParserError undefined_identifier(Token curr, std::string id) {
    return ParserError{curr.position, std::format("{} is not defined", id)};
  }

  static ParserError unexpected_token(Token curr, std::string expected) {
    return ParserError{curr.position,
                       std::format("{} was expected, but {} was found instead",
                                   expected, token_kind_to_string(curr.kind))};
  }

  static ParserError unexpected_token(Token curr, TokenKind expected) {
    return ParserError{curr.position,
                       std::format("{} was expected, but {} was found instead",
                                   token_kind_to_string(expected),
                                   token_kind_to_string(curr.kind))};
  }

  static ParserError undefined_enum_member(Token curr, std::string _enum,
                                           std::string member) {
    return ParserError{
        curr.position,
        std::format("enum '{}' does not contain member '{}'", _enum, member)};
  }

  static ParserError unsupported_enum_variant(Token curr) {
    return ParserError{curr.position,
                       "unexpected statement found while parsing an enum "
                       "variant \nonly structs and enums are supported"};
  }

  static ParserError non_struct_enum_variant(Token curr) {
    return ParserError{curr.position,
                       "statement found while parsing an enum is not a struct\n"
                       "only structs and enums are supported"};
  }

  static ParserError fn_missing_body(Token curr) {
    return ParserError{curr.position, "function is missing a body definition"};
  }

  static ParserError argument_expected(Token curr) {
    return ParserError{
        curr.position,
        std::format("an argument was expected, but {} was found instead",
                    token_kind_to_string(curr.kind))};
  }

  static ParserError expected_type_enum(Token curr, const AstType *type) {
    return ParserError{
        curr.position,
        std::format("an enum type was expected, but {} was found instead",
                    type->get_name())};
  }

  static ParserError undefined_meta_fn(Token curr, std::string name) {
    return ParserError{curr.position,
                       std::format("'{}' is not a valid meta function", name)};
  }

  static ParserError undefined_call(Token curr, std::string name) {
    return ParserError{curr.position,
                       std::format("'{}' is not a valid function", name)};
  }

  static ParserError no_overload_call_matched(Token curr, AstTypeDb *type_db,
                                              std::vector<AstTypeId> fns) {
    auto fn_ty = type_db->get_type(fns[0]).value();
    std::string msg =
        std::format("no overload found for function '{}' with "
                    "the given arguments \navailable overloads:\n",
                    fn_ty->get_name());

    for (auto &fn : fns) {
      msg += "\t (";
      fn_ty = type_db->get_type(fn).value();
      for (auto &arg : fn_ty->get_pre_args()) {
        auto arg_type = msg +=
            std::format("{}: {}", arg.name,
                        type_db->get_type(arg.type).value()->get_name());
      }
      if (fn_ty->get_pre_args().size() > 0)
        msg += " ";
      msg += "|";
      if (fn_ty->get_su_args().size() > 0)
        msg += " ";
      for (auto &arg : fn_ty->get_su_args()) {
        auto arg_type = msg +=
            std::format("{}: {}", arg.name,
                        type_db->get_type(arg.type).value()->get_name());
      }
      msg += ")\n";
    }
    return ParserError{curr.position, msg};
  }

  static ParserError undefined_struct(Token curr, std::string name) {
    return ParserError{curr.position,
                       std::format("'{}' is not a valid struct", name)};
  }

  static ParserError expression_expected(Token curr) {
    return ParserError{curr.position, "an expression was expected here"};
  }

  static ParserError undefined_statement(Token curr) {
    return ParserError{curr.position, "undefined statement found"};
  }

  static ParserError eof_found(Token curr) {
    return ParserError{curr.position, "unexpectedly reached end of file"};
  }

  static ParserError multityped_array(Token curr, const AstType *array_type,
                                      const AstType *unexpected_type) {
    return ParserError{curr.position,
                       std::format("array with type '{}' cannot contain an "
                                   "expression of a different type '{}'",
                                   array_type->get_name(),
                                   unexpected_type->get_name())};
  }
  static ParserError member_not_found(Token curr, const AstType *type,
                                      std::string member) {
    return ParserError{curr.position,
                       std::format("no member '{}' found for type '{}'", member,
                                   type->get_name())};
  }
};

template <class T> class ParserResult {
  std::optional<T> result;
  std::optional<ParserError> error;
  bool is_canceled;

public:
  template <class U> friend class ParserResult;

  ParserResult(ParserError error)
      : result(std::nullopt), error(error), is_canceled(false) {}
  ParserResult(T result)
      : result(std::move(result)), error({}), is_canceled(false) {}
  ParserResult() : result({}), error({}), is_canceled(true) {}

  bool is_ok() { return result.has_value(); }
  bool is_err() { return error.has_value(); }
  bool is_cancel() { return is_canceled; }

  T &get_res() { return result.value(); }
  ParserError &get_err() { return error.value(); }

  explicit operator bool() { return is_ok(); }

  template <class U>
  ParserResult(ParserResult<U> &&other) noexcept(
      std::is_nothrow_constructible_v<T, U &&> &&
      std::is_nothrow_constructible_v<ParserError,
                                      ParserError &&>) // Propagate noexcept
      : is_canceled(other.is_cancel()) {
    if (other.is_ok()) {
      result.emplace(std::move(other.get_res()));
      other.result.reset();
    } else if (other.is_err()) {
      error.emplace(std::move(other.error.value()));
      other.error.reset();
    }
  }

  template <class U>
  ParserResult(const ParserResult<U> &other) : is_canceled(other.is_canceled) {
    if (other.is_ok()) {
      result.emplace(other.result.value());
    } else if (other.is_err()) {
      error.emplace(other.error.value());
    }
  }
};

struct Parser {
private:
  std::vector<ParserError> errors;

public:
  bool debug_scan = false;
  bool debug_checks = false;
  ProgramCtx *ctx;
  Lexer lexer;
  AstExprTypeVisitor type_visitor;

  std::vector<Token> tk_queue;

  /// Holds all expressions not used in a line. Used for prefix arguments.
  std::vector<AstExpression> line_expressions;

  /// Meta tags fn defined and not used
  std::vector<uptr<MetaDefExprAst>> line_meta_def;

  /// Stack of types for types declared inside other types.
  std::vector<AstTypeId> asttype_stack;

  Parser(ProgramCtx *ctx) : ctx(ctx) { type_visitor.ctx = ctx; }

#define LOG(msg)                                                               \
  if (debug_scan)                                                              \
  std::println(msg)

  Token get_tk(int offset) {
    if (offset >= (int)tk_queue.size())
      throw ParserError::eof_found(tk_queue[tk_queue.size() - 1]);
    return tk_queue[offset];
  }

  /// Try to read 'steps' more tokens, returns if it was possible or not.
  /* bool read_tokens(int steps) { */
  /*   for (int i = 0; i < steps; i++) { */
  /*     auto tk = lexer.get_token(); */
  /*     if (tk.kind == EoF) */
  /*       return false; */
  /*     tk_queue.push_back(tk); */
  /*   } */
  /*   return true; */
  /* } */

  bool has_parsing_errors();
  void print_parsing_errors() {
    for (auto err : errors) {
      ParserError::print_error(err, lexer.source);
    }
  }

  bool check_any_tokens(std::initializer_list<TokenKind> tokens, int offset);

  bool check_tokens(std::initializer_list<TokenKind> tokens, int offset);

  void skip_until(std::initializer_list<TokenKind> kinds, int &offset);

  void skip_until(TokenKind kind, int &offset);

  void skip_all(std::initializer_list<TokenKind> kinds, int &offset);

  uptr<FileStmtAst> parse_file(std::string source, bool print_tokens);

  ParserResult<AstStatement> parse_file_statement(int &offset);

  ParserResult<uptr<StructDefAst>> parse_struct_def(int &offset);

  ParserResult<uptr<MetaDefExprAst>> parse_meta_expr(int &offset);

  ParserResult<uptr<FnDefAst>>
  parse_fn_def(std::optional<AstTypeId> parent_struct, int &offset);

  ParserResult<AstTypeId> parse_type(int &offset);

  ParserResult<uptr<EnumDefAst>> parse_enum_def(int &offset);

  /// Will read all expressions in a line and merge unused ones into prefix
  /// arguments.
  ParserResult<AstExpression>
  consume_expressions(int &offset,
                      std::initializer_list<TokenKind> halt_tokens);

  ParserResult<uptr<VarAssignStmtAst>>
  parse_var_assign(int &offset, std::initializer_list<TokenKind> halt_tokens);

  ParserResult<uptr<VarDefStmtAst>> parse_var_decl(int &offset);

  ParserResult<uptr<ReturnStmtAst>> parse_return(int &offset);

  ParserResult<AstStatement> parse_statement(int &offset);

  ParserResult<AstExpression> parse_expr(int &offset);

  ParserResult<AstExpression> parse_inner_expr(int &offset);

  ParserResult<uptr<EnumExprAst>> parse_enum_expr(int &offset);

  ParserResult<uptr<MemberAccesorExprAst>>
  parser_member_access_expr(int &offset, AstExpression &base_expr);

  ParserResult<uptr<SingleMatchExprAst>> parse_single_match_expr(int &offset);

  ParserResult<uptr<IfExprAst>> parse_if_expr(int &offset);

  ParserResult<uptr<ForExprAst>> parse_loop_expr(int &offset);

  ParserResult<std::unique_ptr<CallExprAst>>
  parse_call_expr(int &offset, std::optional<AstTypeId> parent);

  ParserResult<std::unique_ptr<StructExprAst>> parse_struct_expr(int &offset);

  ParserResult<std::unique_ptr<IndexExprAst>>
  parse_index_expr(int &offset, AstExpression &base);

  ParserResult<std::unique_ptr<ArrayExprAst>> parse_array_expr(int &offset);

  ParserResult<std::vector<uptr<StructExprAst::StructFieldAssign>>>
  parse_struct_assigments(int &offset);

  ParserResult<std::unique_ptr<BodyExprAst>> parse_fn_body(int &offset);

  ParserResult<std::unique_ptr<FnHeaderAst>>
  parse_fn_header(std::string header_id, std::optional<AstTypeId> parent_struct,
                  int &offset);

  ParserResult<std::vector<uptr<ArgDefAst>>>
  parse_fn_args(std::optional<AstTypeId> parent_struct, int &offset);

  ParserResult<uptr<ArgDefAst>> parse_arg_def(int &offset);
};
