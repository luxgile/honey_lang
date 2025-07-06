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

  static void print_error(ParserError &error, std::string source) {
    auto pos = error.pos;
    std::vector<std::string> lines;
    std::istringstream iss(source);
    std::string line;
    while (std::getline(iss, line))
      lines.push_back(line);

    std::println("error: {}", error.msg);
    std::println("--- line: {}", error.pos.line);
    std::println("|");
    std::println("|  {}", lines[pos.line - 1]);
  }

  static ParserError type_not_found(Token curr, std::string type_name) {
    return ParserError{curr.position,
                       std::format("Use of undeclared type '{}'", type_name)};
  }

  static ParserError
  unexpected_token(Token curr, std::initializer_list<TokenKind> expected) {
    std::string msg;
    for (auto kind : expected) {
      msg += token_kind_to_string(kind) + ",";
    }
    msg.pop_back();
    msg += std::format(" was expected, but {} was found instead.",
                       token_kind_to_string(curr.kind));
    return ParserError{curr.position, msg};
  }

  static ParserError unexpected_token(Token curr, std::string expected) {
    return ParserError{curr.position,
                       std::format("{} was expected, but {} was found instead.",
                                   expected, token_kind_to_string(curr.kind))};
  }

  static ParserError unexpected_token(Token curr, TokenKind expected) {
    return ParserError{curr.position,
                       std::format("{} was expected, but {} was found instead.",
                                   token_kind_to_string(expected),
                                   token_kind_to_string(curr.kind))};
  }

  static ParserError undefined_enum_member(Token curr, std::string _enum,
                                           std::string member) {
    return ParserError{
        curr.position,
        std::format("Enum '{}' does not contain member '{}'", _enum, member)};
  }

  static ParserError unsupported_enum_variant(Token curr) {
    return ParserError{curr.position,
                       "Unexpected statement found while parsing an enum "
                       "variant. Only structs and enums are supported."};
  }

  static ParserError non_struct_enum_variant(Token curr) {
    return ParserError{curr.position,
                       "Statement found while parsing an enum is not a struct. "
                       "Only structs and enums are supported."};
  }

  static ParserError fn_missing_body(Token curr) {
    return ParserError{curr.position, "Function is missing a body definition."};
  }

  static ParserError argument_expected(Token curr) {
    return ParserError{
        curr.position,
        std::format("An argument was expected, but {} was found instead.",
                    token_kind_to_string(curr.kind))};
  }

  static ParserError expected_type_enum(Token curr, const AstType *type) {
    return ParserError{
        curr.position,
        std::format("An enum type was expected, but {} was found instead.",
                    type->get_name())};
  }

  static ParserError undefined_meta_fn(Token curr, std::string name) {
    return ParserError{curr.position,
                       std::format("'{}' is not a valid meta function.", name)};
  }

  static ParserError undefined_call(Token curr, std::string name) {
    return ParserError{curr.position,
                       std::format("'{}' is not a valid function.", name)};
  }

  static ParserError undefined_struct(Token curr, std::string name) {
    return ParserError{curr.position,
                       std::format("'{}' is not a valid struct.", name)};
  }

  static ParserError expression_expected(Token curr) {
    return ParserError{curr.position, "An expression was expected here."};
  }

  static ParserError undefined_statement(Token curr) {
    return ParserError{curr.position, "Undefined statement found."};
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

  std::optional<Token> get_tk(int offset) {
    if (offset >= (int)tk_queue.size())
      return {};
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

  bool has_parsing_errors() { return errors.size() > 0; }
  void print_parsing_errors() {
    for (auto err : errors) {
      ParserError::print_error(err, lexer.source);
    }
  }

  bool check_any_tokens(std::initializer_list<TokenKind> tokens, int offset) {
    if (offset > (int)tk_queue.size())
      return false;

    for (TokenKind tk : tokens) {
      if (tk == tk_queue[offset].kind) {
        return true;
      }
    }

    return false;
  }

  bool check_tokens(std::initializer_list<TokenKind> tokens, int offset) {
    if (tokens.size() + offset - 1 > tk_queue.size())
      return false;

    int i = 0;
    for (TokenKind tk : tokens) {
      if (tk != tk_queue[i + offset].kind) {
        return false;
      }
      i += 1;
    }

    return true;
  }

  void panic_until(std::initializer_list<TokenKind> kinds, int &offset) {
    while (!check_any_tokens(kinds, offset))
      offset += 1;
  }

  void panic_until(TokenKind kind, int &offset) {
    while (!check_tokens({kind}, offset))
      offset += 1;
  }

  uptr<FileStmtAst> parse_file(std::string source) {
    lexer.set_source(source);
    while (true) {
      auto tk = lexer.get_token();
      tk_queue.push_back(tk);
      if (tk.kind == EoF)
        break;
    }

    auto offset = 0;
    asttype_stack.clear();

    std::vector<AstStatement> statements;
    while (!check_tokens({EoF}, offset) && offset <= (int)tk_queue.size()) {
      auto statement = parse_file_statement(offset);
      if (statement.is_err()) {
        errors.push_back(statement.get_err());
        continue;
      }
      statements.push_back(std::move(statement.get_res()));
    }
    return std::make_unique<FileStmtAst>("main.hun", std::move(statements));
  }

  ParserResult<AstStatement> parse_file_statement(int &offset) {
    if (auto _struct = parse_struct_def(offset)) {
      return _struct;
    }

    if (auto _enum = parse_enum_def(offset)) {
      return _enum;
    }

    if (auto fn = parse_fn_def(offset)) {
      return fn;
    }

    if (auto meta = parse_meta_expr(offset)) {
      // Meta functions at file level are meant to be used for the following
      // statement.
      // TODO: This is NOT the case for @IF or similar for conditional
      // compilation.
      /* line_meta_def.push_back(std::move(meta.get_res())); */
      tk_queue.clear();
      return ParserResult<uptr<StatementExprAst>>{
          std::make_unique<StatementExprAst>(std::move(meta.get_res()))};
    }

    panic_until({NewLine, EoF}, offset);
    return ParserError::undefined_statement(get_tk(offset).value());
  }

  ParserResult<uptr<StructDefAst>> parse_struct_def(int &offset) {
    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    if (check_tokens({Id, Colon, Colon, Struct, LBrace}, offset)) {
      auto struct_name = get_tk(offset).value().value;
      offset += 5;

      // Skip new lines
      while (check_tokens({NewLine}, offset))
        offset += 1;

      auto s_id = ctx->type_db.new_struct(struct_name, {}, {});
      asttype_stack.push_back(s_id);

      // Get struct fields
      std::vector<uptr<ArgDefAst>> fields;
      std::vector<AstTypeField> field_types;
      while (!check_tokens({RBrace}, offset)) {
        auto field = parse_arg_def(offset);
        if (field.is_err()) {
          errors.push_back(field.get_err());
          panic_until(RBrace, offset);
          break;
        }

        if (!check_tokens({Comma}, offset)) {
          errors.push_back(
              ParserError::unexpected_token(*get_tk(offset), Comma));
          panic_until(RBrace, offset);
          break;
        }
        offset += 1;

        // Skip new lines
        while (check_tokens({NewLine}, offset))
          offset += 1;

        field_types.push_back(
            AstTypeField{field.get_res()->name, field.get_res()->type});
        fields.push_back(std::move(field.get_res()));
      }
      offset += 1;

      asttype_stack.pop_back();

      auto s_type = ctx->type_db.get_type_mut(s_id).value();
      s_type->set_fields(field_types);
      if (asttype_stack.size() > 0)
        s_type->set_parent_id(asttype_stack.back());
      auto s = std::make_unique<StructDefAst>(s_id, std::move(fields));
      ctx->define_struct(struct_name, s.get());
      return s;
    }

    return {};
  }

  ParserResult<uptr<MetaDefExprAst>> parse_meta_expr(int &offset) {
    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    if (!check_tokens({Meta}, offset))
      return {};

    auto meta_tk = get_tk(offset).value();
    offset += 1;

    auto meta_fn = ctx->get_meta(meta_tk.value);
    if (!meta_fn)
      return ParserError::undefined_meta_fn(get_tk(offset).value(),
                                            meta_tk.value);

    std::vector<AstExpression> args;
    for (int i = 0; i < meta_fn.value()->arg_num(); i++) {
      auto expr = parse_expr(offset);
      if (expr.is_err()) {
        panic_until(NewLine, offset);
        return expr.get_err();
      }

      args.push_back(std::move(expr.get_res()));
    }

    return std::make_unique<MetaDefExprAst>(meta_tk.value, std::move(args));
  }

  ParserResult<uptr<FnDefAst>> parse_fn_def(int &offset) {
    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    bool is_external = false;
    if (check_tokens({Extern}, offset)) {
      is_external = true;
      offset += 1;
    }

    if (!check_tokens({Id, Colon, Colon, Fn}, offset))
      return {};

    auto fn_id = *get_tk(offset);
    offset += 4;
    auto header_r = parse_fn_header(fn_id.value, offset);
    if (!header_r)
      return header_r.get_err();

    auto header = header_r.get_res().get();
    header->is_external = is_external;

    std::optional<uptr<BodyExprAst>> body = {};

    for (auto &prefix : header->prefix_args) {
      ctx->defined_vars[prefix->name] =
          new VarDefStmtAst{prefix->name, prefix->type, std::nullopt};
    }
    for (auto &suffix : header->suffix_args) {
      ctx->defined_vars[suffix->name] =
          new VarDefStmtAst{suffix->name, suffix->type, std::nullopt};
    }

    if (!is_external) {
      auto body_r = parse_fn_body(offset);
      if (!body_r)
        return body_r.get_err();
      body = std::move(body_r.get_res());
    }

    if (!body && !is_external)
      return ParserError::fn_missing_body(get_tk(offset).value());

    auto fn = std::make_unique<FnDefAst>(std::move(header_r.get_res()),
                                         std::move(body));
    ctx->define_fn(fn->fn_header->name, fn->fn_header.get());
    return fn;
  }

  ParserResult<AstTypeId> parse_type(int &offset) {
    bool is_ref = false;
    if (check_tokens({Pointy}, offset)) {
      is_ref = true;
      offset += 1;
    }

    if (!check_tokens({Id}, offset))
      return ParserError::unexpected_token(*get_tk(offset), Id);

    auto id = get_tk(offset).value().value;
    offset += 1;

    auto type_info = ctx->type_db.get_type_by_name(id);
    if (!type_info)
      return ParserError::type_not_found(*get_tk(offset), id);

    // TODO: This will need to be done to handle types defined inside types
    /* while(check_tokens({Dot}, offset)) { */
    /*   offset += 1; */
    /*   if(!check_tokens({Id}, offset)) */
    /*     return {}; */
    /*   auto subtype_name = get_tk(offset).value().value; */
    /*   auto type = type_info.value().get_fiel */
    /* } */

    if (is_ref)
      return ctx->type_db.new_ref(type_info.value()->get_id());

    return type_info.value()->get_id();
  }

  ParserResult<uptr<EnumDefAst>> parse_enum_def(int &offset) {
    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    if (!check_tokens({Id, Colon, Colon, Enum, LBrace}, offset))
      return {};

    auto enum_name = get_tk(offset).value().value;
    offset += 5;

    // Skip new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    auto enum_id = ctx->type_db.new_enum(enum_name, {}, {});
    asttype_stack.push_back(enum_id);

    // Get struct fields
    std::vector<AstTypeField> field_types;
    std::vector<AstStatement> field_stmts;
    while (!check_tokens({RBrace}, offset)) {
      // A enum variant without struct:
      if (check_tokens({Id, Comma}, offset)) {
        auto field = get_tk(offset).value().value;
        offset += 1;

        // Create a unit struct to represent enum variant.
        auto s_type = ctx->type_db.new_struct(
            field, std::vector<AstTypeField>({}), enum_id);
        auto s_vars = std::vector<uptr<ArgDefAst>>();
        auto s = std::make_unique<StructDefAst>(s_type, std::move(s_vars));
        ctx->define_struct(field, s.get());

        field_types.push_back(AstTypeField{field, s_type});
        field_stmts.push_back(std::move(s));
      } else if (auto stmt = parse_file_statement(offset)) {
        // Convert the declared struct into another variant for the enum

        auto stmt_type_id = AstTypeId{};
        if (std::holds_alternative<uptr<StructDefAst>>(stmt.get_res())) {
          auto s = &std::get<uptr<StructDefAst>>(stmt.get_res());
          stmt_type_id = s->get()->type;
        } else {
          return ParserError::non_struct_enum_variant(get_tk(offset).value());
        }

        auto stmt_type = ctx->type_db.get_type(stmt_type_id).value();
        field_types.push_back(
            AstTypeField{stmt_type->get_name(), stmt_type_id});
        field_stmts.push_back(std::move(stmt.get_res()));

      } else {
        return ParserError::unsupported_enum_variant(*get_tk(offset));
      }

      if (!check_tokens({Comma}, offset))
        return ParserError::unexpected_token(get_tk(offset).value(), Comma);
      offset += 1;

      // Skip new lines
      while (check_tokens({NewLine}, offset))
        offset += 1;
    }
    offset += 1;

    asttype_stack.pop_back();

    auto enum_type = ctx->type_db.get_type_mut(enum_id).value();
    enum_type->set_fields(field_types);
    if (asttype_stack.size() > 0)
      enum_type->set_parent_id(asttype_stack.back());

    auto e = std::make_unique<EnumDefAst>(enum_id, std::move(field_stmts));

    ctx->define_enum(enum_type->get_fullname(), e.get());
    return e;
  }

  /// Will read all expressions in a line and merge unused ones into prefix
  /// arguments.
  ParserResult<AstExpression>
  consume_expressions(int &offset,
                      std::initializer_list<TokenKind> halt_tokens) {
    while (!check_any_tokens(halt_tokens, offset)) {
      auto expr = parse_expr(offset);
      if (expr.is_cancel())
        break;

      if (expr.is_err()) {
        line_expressions.clear();
        panic_until(halt_tokens, offset);
        return expr.get_err();
      }

      line_expressions.push_back(std::move(expr.get_res()));
    }
    offset += 1; // To account for the halt token.

    if (line_expressions.size() == 0)
      return ParserResult<uptr<NoOpAst>>{std::make_unique<NoOpAst>()};

    auto last_expr = std::move(line_expressions.back());
    line_expressions.pop_back();
    if (line_expressions.size() > 0)
      std::println("!! line expressions unnused: {}", line_expressions.size());
    line_expressions.clear();
    return std::move(last_expr);
  }

  ParserResult<uptr<VarAssignStmtAst>>
  parse_var_assign(int &offset, std::initializer_list<TokenKind> halt_tokens) {
    auto tmp_offset = offset;
    auto lvalue = parse_expr(tmp_offset);
    if (!lvalue)
      return lvalue.get_err();

    if (!check_tokens({Id}, tmp_offset) ||
        get_tk(tmp_offset).value().value != "=")
      return ParserError::unexpected_token(get_tk(tmp_offset).value(), "=");
    tmp_offset += 1;

    auto rvalue = consume_expressions(tmp_offset, halt_tokens);
    if (!rvalue)
      return rvalue.get_err();

    offset = tmp_offset;
    auto def_var = std::make_unique<VarAssignStmtAst>(
        std::move(lvalue.get_res()), std::move(rvalue.get_res()));
    return def_var;
  }

  ParserResult<uptr<VarDefStmtAst>> parse_var_decl(int &offset) {
    if (!check_tokens({Id, Colon, Id}, offset) ||
        get_tk(offset + 2).value().value != "=")
      return {};

    auto id = get_tk(offset).value().value;
    offset += 3;
    auto expr = consume_expressions(offset, {NewLine});
    if (!expr)
      return expr.get_err();

    auto type = std::visit(type_visitor, expr.get_res());

    LOG("var definition expr found");
    auto def_var =
        std::make_unique<VarDefStmtAst>(id, type, std::move(expr.get_res()));
    ctx->defined_vars[id] = def_var.get();
    return def_var;
    LOG("parsing var declaration failed");
    return {};
  }

  ParserResult<AstStatement> parse_statement(int &offset) {
    // Var declaration
    if (auto var_decl = parse_var_decl(offset))
      return var_decl;

    // Var assigment
    if (auto var_assign = parse_var_assign(offset, {NewLine})) {
      return var_assign;
    }

    // If nothing else found, try to find a expression like a call function.
    // This handles the line expressions in case prefixed arguments are found.
    auto last_expr = consume_expressions(offset, {NewLine});
    if (!last_expr)
      return last_expr.get_err();

    LOG("statement expr found");
    auto stmt_expr =
        std::make_unique<StatementExprAst>(std::move(last_expr.get_res()));
    return ParserResult<uptr<StatementExprAst>>{std::move(stmt_expr)};
  }

  ParserResult<AstExpression> parse_expr(int &offset) {
    auto inner_expr = parse_inner_expr(offset);
    if (inner_expr.is_cancel())
      return {};

    if (inner_expr.is_err())
      return inner_expr.get_err();

    if (auto mem_acc =
            parser_member_access_expr(offset, inner_expr.get_res())) {
      AstExpression _inner_expr = std::move(mem_acc.get_res());
      mem_acc = parser_member_access_expr(offset, _inner_expr);
      while (mem_acc) {
        _inner_expr = std::move(mem_acc.get_res());
        mem_acc = parser_member_access_expr(offset, _inner_expr);
      }
      return std::move(_inner_expr);
    }

    return inner_expr;
  }

  ParserResult<AstExpression> parse_inner_expr(int &offset) {
    LOG("parsing expression");

    if (check_tokens({Id}, offset)) {
      auto identifier = get_tk(offset).value().value;

      // Enum expr
      if (auto enum_expr = parse_enum_expr(offset))
        return enum_expr;

      // Call expr
      if (auto call = parse_call_expr(offset))
        return call;

      // If the identifier is a fn but the previous call failed because not all
      // args are parsed yet, avoid creating a variable below
      if (auto overloads = ctx->get_overloads(identifier))
        return {};

      if (auto struct_expr = parse_struct_expr(offset))
        return struct_expr;

      // Variable
      auto var = std::make_unique<VarExprAst>(identifier);
      LOG("var expression found");
      offset += 1;
      return ParserResult<uptr<VarExprAst>>{std::move(var)};
    }

    if (auto body = parse_fn_body(offset))
      return body;

    if (check_tokens({LPar}, offset)) {
      offset += 1;
      auto expr = consume_expressions(offset, {TokenKind::RPar});
      if (!expr)
        return expr.get_err();
      return ParserResult<uptr<GroupExprAst>>{
          std::make_unique<GroupExprAst>(std::move(expr.get_res()))};
    }

    if (auto match_expr = parse_single_match_expr(offset))
      return match_expr;

    if (auto if_expr = parse_if_expr(offset))
      return if_expr;

    if (auto for_expr = parse_loop_expr(offset))
      return for_expr;

    if (check_tokens({Meta}, offset)) {
      auto meta = parse_meta_expr(offset);
      return meta;
    }

    if (check_tokens({String}, offset)) {
      LOG("string found");
      std::string str = tk_queue[offset].value;
      offset += 1;
      return ParserResult<uptr<StringExprAst>>{
          std::make_unique<StringExprAst>(str)};
    }

    if (check_tokens({Bool}, offset)) {
      LOG("bool found");
      std::string i = tk_queue[offset].value;
      offset += 1;
      return ParserResult<uptr<BoolExprAst>>{
          std::make_unique<BoolExprAst>(i == "true" ? true : false)};
    }

    if (check_tokens({Int}, offset)) {
      LOG("int found");
      std::string i = tk_queue[offset].value;
      offset += 1;
      return ParserResult<uptr<IntExprAst>>{
          std::make_unique<IntExprAst>(std::stoi(i))};
    }

    if (check_tokens({Float}, offset)) {
      LOG("float found");
      std::string f = tk_queue[offset].value;
      offset += 1;
      return ParserResult<uptr<FloatExprAst>>{
          std::make_unique<FloatExprAst>(std::stof(f))};
    }

    if (check_tokens({Amper}, offset)) {
      LOG("ref found");
      offset += 1;
      auto expr = parse_expr(offset);
      if (!expr)
        return expr.get_err();
      return ParserResult<uptr<RefExprAst>>{
          std::make_unique<RefExprAst>(std::move(expr.get_res()))};
    }

    if (check_tokens({Pointy}, offset)) {
      LOG("deref found");
      offset += 1;
      auto expr = parse_expr(offset);
      if (!expr)
        return expr.get_err();
      return ParserResult<uptr<DerefExprAst>>{
          std::make_unique<DerefExprAst>(std::move(expr.get_res()))};
    }

    LOG("no expression found");
    return {};
  }

  ParserResult<uptr<EnumExprAst>> parse_enum_expr(int &offset) {
    if (!check_tokens({Id, Dot, Id}, offset))
      return {};
    auto enum_name = get_tk(offset).value().value;
    auto enum_member_name = get_tk(offset + 2).value().value;

    auto enum_type_id = ctx->type_db.get_id_by_name(enum_name);
    if (!enum_type_id)
      return ParserError::type_not_found(get_tk(offset).value(), enum_name);

    auto enum_type = ctx->type_db.get_type(*enum_type_id).value();
    if (!enum_type->is_enum())
      return ParserError::expected_type_enum(get_tk(offset).value(), enum_type);

    auto enum_member_id =
        enum_type->get_field_by_name(enum_member_name).value()->type;
    auto enum_member_type = ctx->type_db.get_type(enum_member_id);

    offset += 3;
    std::vector<uptr<StructExprAst::StructFieldAssign>> vars;
    if (!enum_member_type.value()->is_unit()) {
      auto _vars = parse_struct_assigments(offset);
      if (!_vars)
        return _vars.get_err();
      vars = std::move(_vars.get_res());
    }

    LOG("enum expr parsed");
    auto struct_expr =
        std::make_unique<StructExprAst>(enum_member_id, std::move(vars));
    return std::make_unique<EnumExprAst>(*enum_type_id, std::move(struct_expr));
  }

  ParserResult<uptr<MemberAccesorExprAst>>
  parser_member_access_expr(int &offset, AstExpression &base_expr) {
    LOG("starting parsing access member");

    if (!check_tokens({Dot, Id}, offset))
      return {};

    offset += 2;
    auto id = get_tk(offset - 1).value().value;

    LOG("member access parsed");
    return std::make_unique<MemberAccesorExprAst>(std::move(base_expr), id);
  }

  ParserResult<uptr<SingleMatchExprAst>> parse_single_match_expr(int &offset) {
    LOG("starting single match parsing");

    int tmp_offset = offset;
    if (!check_tokens({Match}, tmp_offset))
      return {};

    tmp_offset += 1;

    auto match_expr = parse_expr(tmp_offset);

    if (!check_tokens({Colon}, tmp_offset))
      return ParserError::unexpected_token(get_tk(offset).value(), Colon);
    tmp_offset += 1;

    // TODO: Need to handle types as a general function
    if (!check_tokens({Id, Dot, Id}, tmp_offset))
      return ParserError::unexpected_token(get_tk(offset).value(),
                                           {Id, Dot, Id});
    auto enum_name = get_tk(tmp_offset).value().value;
    auto enum_member_name = get_tk(tmp_offset + 2).value().value;
    tmp_offset += 3;

    auto casted_var_name = std::string("");
    if (check_tokens({Id}, tmp_offset)) {
      casted_var_name = get_tk(tmp_offset).value().value;
      tmp_offset += 1;
    }

    auto enum_type = ctx->type_db.get_type_by_name(enum_name);
    if (!enum_type)
      return ParserError::type_not_found(get_tk(offset).value(), enum_name);
    auto enum_member_type =
        enum_type.value()->get_field_by_name(enum_member_name);
    if (!enum_member_type)
      return ParserError::undefined_enum_member(get_tk(offset).value(),
                                                enum_name, enum_member_name);
    /* auto enum_member_ty =
     * ctx->type_db.get_type(enum_member_type.value()->type); */

    auto casted_var = std::make_unique<VarDefStmtAst>(
        casted_var_name, enum_member_type.value()->type, std::nullopt);

    auto then_expr = parse_expr(tmp_offset);
    if (!then_expr)
      return then_expr.get_err();

    offset = tmp_offset;

    LOG("single match expression found");
    auto match = std::make_unique<SingleMatchExprAst>(
        std::move(match_expr.get_res()), std::move(casted_var),
        std::move(then_expr.get_res()));
    ctx->defined_vars[match->casted_enum_var->name] =
        match->casted_enum_var.get();
    return match;
  }

  ParserResult<uptr<IfExprAst>> parse_if_expr(int &offset) {
    if (!check_tokens({If}, offset))
      return {};

    LOG("parsing if expression");
    offset += 1;

    auto condition = consume_expressions(offset, {LBrace});
    if (!condition)
      return condition.get_err();

    offset -= 1;

    auto then_expr = parse_expr(offset);
    if (!then_expr)
      return then_expr.get_err();

    std::optional<AstExpression> else_expr = std::nullopt;
    if (check_tokens({Else}, offset)) {
      offset += 1;
      auto _else_expr = parse_expr(offset);
      if (!_else_expr)
        return _else_expr.get_err();
      else_expr = std::move(_else_expr.get_res());
    }

    LOG("if expression found");
    return std::make_unique<IfExprAst>(std::move(condition.get_res()),
                                       std::move(then_expr.get_res()),
                                       std::move(else_expr));
  }

  ParserResult<uptr<ForExprAst>> parse_loop_expr(int &offset) {
    if (check_tokens({Loop}, offset)) {
      LOG("parsing for expression");
      offset += 1;

      auto for_cond_expr = consume_expressions(offset, {LBrace});
      if (!for_cond_expr)
        return for_cond_expr.get_err();

      offset -= 1;

      auto for_body = parse_expr(offset);
      if (!for_body)
        return for_body.get_err();

      LOG("for expression found");
      return std::make_unique<ForExprAst>(std::move(for_cond_expr.get_res()),
                                          std::move(for_body.get_res()));
    }

    return {};
  }

  ParserResult<std::unique_ptr<CallExprAst>> parse_call_expr(int &offset) {
    LOG("parsing call");
    auto identifier = get_tk(offset).value().value;
    auto overloads = ctx->get_overloads(identifier);
    if (!overloads) {
      LOG("parsing call failed - no overload found");
      return ParserError::undefined_call(get_tk(offset).value(), identifier);
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

          auto expr = parse_expr(tmp_offset);
          if (!expr)
            return expr.get_err();

          if (!fn->is_vararic() && (i >= (int)fn->suffix_args.size() ||
                                    std::visit(type_visitor, expr.get_res()) !=
                                        fn->suffix_args[i]->type)) {
            matched_args = false;
            break;
          }

          suffix_args.push_back(std::move(expr.get_res()));
          i += 1;
        }

        if (!matched_args)
          continue;

      } else {
        int expected_arg_count = fn->suffix_args.size();
        for (int i = 0; i < expected_arg_count; i++) {
          auto expr = parse_expr(tmp_offset);
          if (!expr)
            return expr.get_err();

          if (i >= (int)fn->suffix_args.size() ||
              (!fn->suffix_args[i]->is_varadic &&
               std::visit(type_visitor, expr.get_res()) !=
                   fn->suffix_args[i]->type)) {
            matched_args = false;
            break;
          }

          suffix_args.push_back(std::move(expr.get_res()));
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

  ParserResult<std::unique_ptr<StructExprAst>> parse_struct_expr(int &offset) {
    LOG("parsing struct expr");
    auto identifier = get_tk(offset).value().value;
    auto s = ctx->get_struct(identifier);
    if (!s) {
      LOG("parsing struct failed - no struct found");
      return ParserError::undefined_struct(get_tk(offset).value(), identifier);
    }

    // Consume the identifier
    int tmp_offset = offset;
    tmp_offset += 1;

    auto field_assigns = parse_struct_assigments(tmp_offset);
    if (!field_assigns)
      return field_assigns.get_err();

    offset = tmp_offset;

    LOG("struct expr found");
    return std::make_unique<StructExprAst>(s.value()->type,
                                           std::move(field_assigns.get_res()));
  }

  ParserResult<std::vector<uptr<StructExprAst::StructFieldAssign>>>
  parse_struct_assigments(int &offset) {
    std::vector<uptr<StructExprAst::StructFieldAssign>> field_assigns;
    if (!check_tokens({Dot, LBrace}, offset))
      return ParserError::unexpected_token(get_tk(offset).value(),
                                           {Dot, LBrace});
    offset += 2;

    while (!check_tokens({RBrace}, offset)) {
      LOG("parsing struct var assigment");

      if (!check_tokens({Dot, Id}, offset))
        return ParserError::unexpected_token(get_tk(offset).value(), {Dot, Id});

      auto id = get_tk(offset + 1).value().value;
      offset += 2;

      if (!check_tokens({Id}, offset) || get_tk(offset).value().value != "=")
        return ParserError::unexpected_token(get_tk(offset).value(), "=");
      offset += 1;

      auto rvalue = consume_expressions(offset, {Comma, NewLine, RBrace});
      if (!rvalue)
        return rvalue.get_err();

      LOG("struct var assignment found");
      auto struct_var = std::make_unique<StructExprAst::StructFieldAssign>(
          id, std::move(rvalue.get_res()));
      field_assigns.push_back(std::move(struct_var));

      if (check_tokens({RBrace}, offset - 1))
        break;
    }
    // No need to increase offset by one, as inside the loop we are checking
    // if the previous line is RBrace
    return field_assigns;
  }

  ParserResult<std::unique_ptr<BodyExprAst>> parse_fn_body(int &offset) {
    if (!check_tokens({LBrace}, offset))
      return {};

    offset += 1;
    while (check_tokens({NewLine}, offset)) { // Ignore new line
      offset += 1;
    }

    std::vector<AstStatement> stmts{};
    while (!check_tokens({RBrace}, offset)) {
      auto stmt = parse_statement(offset);
      if (!stmt) {
        panic_until(NewLine, offset);
        errors.push_back(stmt.get_err());
        continue;
      }
      stmts.push_back(std::move(stmt.get_res()));

      // Skip all new lines
      while (check_tokens({NewLine}, offset))
        offset += 1;
    }

    // Skip RBrace
    offset += 1;
    line_expressions.clear();

    LOG("got fn body");
    return std::make_unique<BodyExprAst>(std::move(stmts));
  }

  ParserResult<std::unique_ptr<FnHeaderAst>>
  parse_fn_header(std::string header_id, int &offset) {
    LOG("parsing fn header");
    auto ret_type = VOID_TYPE.get_id();

    if (!check_tokens({LPar}, offset))
      return ParserError::unexpected_token(get_tk(offset).value(), LPar);
    offset += 1;

    // Prev arguments
    LOG("prefix args:");
    auto prefix = parse_fn_args(offset);
    if (!prefix)
      return prefix.get_err();

    // Divider type
    if (!check_tokens({Bar}, offset))
      return ParserError::unexpected_token(get_tk(offset).value(), Bar);
    offset += 1;

    // Next arguments
    LOG("suffix args:");
    auto suffix = parse_fn_args(offset);
    if (!suffix)
      return {};

    if (!check_tokens({RPar}, offset))
      return ParserError::unexpected_token(get_tk(offset).value(), RPar);
    offset += 1;

    // Return type
    auto found_ret_type = parse_type(offset);
    if (found_ret_type)
      ret_type = found_ret_type.get_res();

    return std::make_unique<FnHeaderAst>(header_id, ret_type, false,
                                         std::move(prefix.get_res()),
                                         std::move(suffix.get_res()));
  }

  ParserResult<std::vector<uptr<ArgDefAst>>> parse_fn_args(int &offset) {
    auto args = std::vector<uptr<ArgDefAst>>();

    // No arguments found
    if (check_tokens({Bar}, offset) || check_tokens({RPar}, offset))
      return args;

    while (true) {
      auto arg = parse_arg_def(offset);
      if (!arg)
        return ParserError::argument_expected(get_tk(offset).value());

      args.push_back(std::move(arg.get_res()));

      if (check_tokens({Comma}, offset)) {
        offset += 1;
        continue;
      }

      if (check_tokens({Bar}, offset) || check_tokens({RPar}, offset)) {
        if (check_tokens({NewLine}, offset))
          offset += 1;
        break;
      }

      return ParserError::unexpected_token(get_tk(offset).value(),
                                           {Comma, Bar, RPar});
    }

    return args;
  }

  ParserResult<uptr<ArgDefAst>> parse_arg_def(int &offset) {
    // Varadic argument
    if (check_tokens({Id, Colon, Dot, Dot, Dot}, offset)) {
      auto field = std::make_unique<ArgDefAst>(tk_queue[offset].value,
                                               VOID_TYPE.get_id(), true);
      offset += 5;
      return field;
    }

    if (check_tokens({Id, Colon}, offset)) {
      auto field_name = get_tk(offset).value().value;
      offset += 2;
      auto arg_type = parse_type(offset);
      if (!arg_type)
        return arg_type.get_err();
      auto field =
          std::make_unique<ArgDefAst>(field_name, arg_type.get_res(), false);
      return field;
    }

    return {};
  }
};
