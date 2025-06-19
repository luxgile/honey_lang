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
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <variant>
#include <vector>

struct ParseError {
  FilePos pos;
  std::string msg;
};

template <class T> struct ParserResult {
  std::optional<T> result;
  std::optional<ParseError> error;
  /* bool incomplete; */

  ParserResult<T> new_ok(T result) { return ParserResult<T>{result, {}}; }
  ParserResult<T> new_error(FilePos pos, std::string msg) {
    return ParserResult<T>{{}, ParseError{pos, msg}};
  }
  /* ParserResult<T> new_incomplete() { return ParserResult<T>{{}, {}, true}; }
   */

  bool is_ok() { return result.has_value(); }
  bool is_err() { return error.has_value(); }
  /* bool is_incomplete() { return incomplete; } */

  T get_res() { return result.value(); }
  ParseError get_err() { return error.value(); }
};

struct Parser {
  bool debug_scan = false;
  bool debug_checks = false;
  ProgramCtx *ctx;
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

  bool check_any_tokens(std::initializer_list<TokenKind> tokens,
                        int offset = 0) {
    if (tokens.size() + offset > tk_queue.size()) {
      if (debug_checks) {
        std::print("> [x] or check [+{}]: ", offset);
        for (auto tk : tokens) {
          std::print("{} ", token_kind_to_string(tk));
        }
        std::print("\n");
      }
      return false;
    }

    for (TokenKind tk : tokens) {
      if (tk == tk_queue[offset].kind) {
        if (debug_checks) {
          std::print("> [O] or check [+{}]: ", offset);
          for (auto tk : tokens) {
            std::print("{} ", token_kind_to_string(tk));
          }
          std::print("\n");
        }
        return true;
      }
    }

    if (debug_checks) {
      std::print("> [x] or check [+{}]: ", offset);
      for (auto tk : tokens) {
        std::print("{} ", token_kind_to_string(tk));
      }
      std::print("\n");
    }
    return false;
  }

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

    auto offset = 0;
    asttype_stack.clear();
    LOG("starting parsing...");
    return handle_file_statement(offset, true);
  }

  std::optional<AstStatement> handle_file_statement(int &offset,
                                                    bool clear_tks) {

    if (auto _struct = handle_struct_def(offset, clear_tks)) {
      return _struct;
    }

    if (auto _enum = handle_enum_def(offset, clear_tks)) {
      return _enum;
    }

    if (auto fn = handle_fn_def(offset)) {
      return fn;
    }

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

  std::optional<uptr<StructDefAst>> handle_struct_def(int &offset,
                                                      bool clear_tks) {
    LOG("parsing struct");

    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    if (check_tokens({Id, Colon, Colon, Struct, LBrace}, offset)) {
      LOG("struct def found");
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
        auto field = handle_arg_def(offset);
        if (!field)
          return {};

        if (!check_tokens({Comma}, offset))
          return {};
        offset += 1;

        // Skip new lines
        while (check_tokens({NewLine}, offset))
          offset += 1;

        field_types.push_back(
            AstTypeField{field.value()->name, field.value()->type});
        fields.push_back(std::move(*field));
      }
      offset += 1;

      LOG("struct parsing successfull");
      if (clear_tks)
        tk_queue.clear();

      asttype_stack.pop_back();

      auto s_type = ctx->type_db.get_type_mut(s_id).value();
      s_type->set_fields(field_types);
      if (asttype_stack.size() > 0)
        s_type->set_parent_id(asttype_stack.back());
      auto s = std::make_unique<StructDefAst>(s_id, std::move(fields));
      ctx->define_struct(struct_name, s.get());
      return s;
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

      auto meta_fn = get_opt(ctx->get_meta(meta_tk.value));

      std::vector<AstExpression> args;
      for (int i = 0; i < meta_fn->arg_num(); i++) {
        auto expr = get_opt(handle_expr(offset));
        args.push_back(std::move(expr));
      }

      LOG("meta found");
      return std::make_unique<MetaDefExprAst>(meta_tk.value, std::move(args));
    }

    LOG("parsing meta failed");
    return {};
  }

  std::optional<uptr<FnDefAst>> handle_fn_def(int &offset) {
    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    bool is_external = false;
    if (check_tokens({Extern}, offset)) {
      is_external = true;
      offset += 1;
    }

    if (check_tokens({Id, Colon, Colon, Fn}, offset)) {
      LOG("fn decl found");
      auto fn_id = *get_tk(offset);
      offset += 4;
      auto header = get_opt(handle_fn_header(fn_id.value, offset));

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

      if (!is_external)
        body = handle_fn_body(offset);

      if (body || is_external) {
        tk_queue.clear();
        LOG("got fn expression");
        auto fn =
            std::make_unique<FnDefAst>(std::move(header), std::move(body));
        ctx->define_fn(fn->fn_header->name, fn->fn_header.get());
        return fn;
      }
    }

    return {};
  }

  std::optional<uptr<EnumDefAst>> handle_enum_def(int &offset, bool clear_tks) {
    LOG("starting parsing enum");

    // Skip all new lines
    while (check_tokens({NewLine}, offset))
      offset += 1;

    if (check_tokens({Id, Colon, Colon, Enum, LBrace}, offset)) {
      LOG("enum def found");
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
        if (check_tokens({Id, Comma}, offset)) {
          auto field = get_tk(offset).value().value;
          offset += 1;

          // Create a unit struct to represent enum variant.
          auto s_type = ctx->type_db.new_struct(
              field,
              std::vector<AstTypeField>(
                  {AstTypeField{"_tag", INT_TYPE.get_id()}}),
              enum_id);
          auto s_vars = std::vector<uptr<ArgDefAst>>();
          s_vars.push_back(
              std::make_unique<ArgDefAst>("_tag", INT_TYPE.get_id(), false));
          auto s = std::make_unique<StructDefAst>(s_type, std::move(s_vars));
          ctx->define_struct(field, s.get());

          field_types.push_back(AstTypeField{field, s_type});
          field_stmts.push_back(std::move(s));
        } else if (auto stmt = handle_file_statement(offset, false)) {
          // Convert the declared struct into another variant for the enum
          auto stmt_type_id = AstTypeId{};
          if (std::holds_alternative<uptr<StructDefAst>>(*stmt)) {
            auto s = &std::get<uptr<StructDefAst>>(*stmt);
            // We need to push the tag variable to the beggining both on struct
            // def and type
            s->get()->fields.insert(
                s->get()->fields.begin(),
                std::make_unique<ArgDefAst>("_tag", INT_TYPE.get_id(), false));
            auto s_type = get_opt(ctx->type_db.get_type_mut(s->get()->type));
            auto s_fields = s_type->get_fields();
            s_fields.insert(s_fields.begin(),
                            AstTypeField{"_tag", INT_TYPE.get_id()});
            s_type->set_fields(s_fields);
            stmt_type_id = s->get()->type;
          } else {
            LOG("failed to parse enum - unsuported statement");
            return {};
          }

          auto stmt_type = ctx->type_db.get_type(stmt_type_id).value();
          field_types.push_back(
              AstTypeField{stmt_type->get_name(), stmt_type_id});
          field_stmts.push_back(std::move(*stmt));
        } else {
          LOG("failed to parse enum - unit struct or other stmt not found");
          return {};
        }

        if (!check_tokens({Comma}, offset))
          return {};
        offset += 1;

        // Skip new lines
        while (check_tokens({NewLine}, offset))
          offset += 1;
      }
      offset += 1;

      LOG("enum parsing successful");
      if (clear_tks)
        tk_queue.clear();

      asttype_stack.pop_back();

      auto enum_type = ctx->type_db.get_type_mut(enum_id).value();
      enum_type->set_fields(field_types);
      if (asttype_stack.size() > 0)
        enum_type->set_parent_id(asttype_stack.back());

      auto e = std::make_unique<EnumDefAst>(enum_id, std::move(field_stmts));

      ctx->define_enum(enum_type->get_fullname(), e.get());
      return e;
    }

    LOG("failed parsing enum");
    return {};
  }

  /// Will read all expressions in a line and merge unused ones into prefix
  /// arguments.
  std::optional<AstExpression>
  consume_expressions(int &offset,
                      std::initializer_list<TokenKind> halt_tokens) {
    LOG("starting consuming expressions");
    while (!check_any_tokens(halt_tokens, offset)) {
      auto expr = handle_expr(offset);
      if (!expr) {
        line_expressions.clear();
        LOG("stopped consuming expressions due to error");
        return {};
      }
      line_expressions.push_back(std::move(*expr));
    }
    offset += 1; // To account for the halt token.
    LOG("finished consuming expressions");

    if (line_expressions.size() == 0)
      return {};
    auto last_expr = std::move(line_expressions.back());
    line_expressions.pop_back();
    if (line_expressions.size() > 0)
      std::println("!! line expressions unnused: {}", line_expressions.size());
    line_expressions.clear();
    return std::move(last_expr);
  }

  std::optional<uptr<VarAssignStmtAst>>
  handle_var_assign(int &offset, std::initializer_list<TokenKind> halt_tokens) {
    LOG("parsing var assigment");
    if (check_tokens({Id, Id}, offset) && tk_queue[offset + 1].value == "=") {
      auto id = get_tk(offset)->value;
      offset += 2;
      auto expr = consume_expressions(offset, halt_tokens);
      if (!expr)
        return {};

      LOG("var assignment found");
      auto def_var = std::make_unique<VarAssignStmtAst>(id, std::move(*expr));
      return def_var;
    }
    return {};
  }

  std::optional<uptr<VarDefStmtAst>> handle_var_decl(int &offset) {
    LOG("parsing var declaration");
    if (check_tokens({Id, Colon, Id}, offset) &&
        tk_queue[offset + 2].value == "=") {
      auto id = get_tk(offset)->value;
      offset += 3;
      auto expr = consume_expressions(offset, {NewLine});
      if (!expr)
        return {};

      auto type = std::visit(type_visitor, *expr);

      LOG("var definition expr found");
      auto def_var = std::make_unique<VarDefStmtAst>(id, type, std::move(expr));
      ctx->defined_vars[id] = def_var.get();
      return def_var;
    }
    LOG("parsing var declaration failed");
    return {};
  }

  std::optional<AstStatement> handle_statement(int &offset) {
    LOG("starting statement parsing");

    // Var declaration
    if (auto var_decl = handle_var_decl(offset))
      return var_decl;

    // Var assigment
    if (auto var_assign = handle_var_assign(offset, {NewLine})) {
      return var_assign;
    }

    // If nothing else found, try to find a expression like a call function.
    // This handles the line expressions in case prefixed arguments are found.
    auto last_expr = consume_expressions(offset, {NewLine});
    if (!last_expr)
      return {};

    LOG("statement expr found");
    auto stmt_expr = std::make_unique<StatementExprAst>(std::move(*last_expr));
    return stmt_expr;
  }

  std::optional<AstExpression> handle_expr(int &offset) {
    auto inner_expr = handle_inner_expr(offset);
    if (!inner_expr)
      return {};

    // TODO: This needs to be recursive
    if (auto mem_acc = handle_member_access_expr(offset, *inner_expr)) {
      inner_expr = std::move(*mem_acc);
      mem_acc = handle_member_access_expr(offset, *inner_expr);
      while (mem_acc) {
        inner_expr = std::move(*mem_acc);
        mem_acc = handle_member_access_expr(offset, *inner_expr);
      }
      return inner_expr;
    }

    return inner_expr;
  }

  std::optional<AstExpression> handle_inner_expr(int &offset) {
    LOG("parsing expression");

    if (check_tokens({Id}, offset)) {
      auto identifier = get_tk(offset).value().value;

      // Enum expr
      if (auto enum_expr = handle_enum_expr(offset))
        return enum_expr;

      // Call expr
      if (auto call = handle_call_expr(offset))
        return call;

      // If the identifier is a fn but the previous call failed because not all
      // args are parsed yet, avoid creating a variable below
      if (auto overloads = ctx->get_overloads(identifier))
        return {};

      if (auto struct_expr = handle_struct_expr(offset))
        return struct_expr;

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
      auto expr = consume_expressions(offset, {TokenKind::RPar});
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

  std::optional<uptr<EnumExprAst>> handle_enum_expr(int &offset) {
    LOG("starting parsing enum expr");

    if (check_tokens({Id, Dot, Id}, offset)) {
      auto enum_name = get_tk(offset).value().value;
      auto enum_member_name = get_tk(offset + 2).value().value;

      auto enum_type_id = get_opt(ctx->type_db.get_id_by_name(enum_name));
      auto enum_type = ctx->type_db.get_type(enum_type_id);
      if (!enum_type || !enum_type.value()->is_enum())
        return {};

      auto enum_member_id =
          get_opt(enum_type.value()->get_field_by_name(enum_member_name))->type;
      auto enum_member_type = get_opt(ctx->type_db.get_type(enum_member_id));

      offset += 3;
      std::vector<uptr<VarAssignStmtAst>> vars;
      if (!enum_member_type->is_unit())
        vars = handle_struct_assigments(offset);

      LOG("enum expr parsed");
      auto struct_expr =
          std::make_unique<StructExprAst>(enum_member_id, std::move(vars));
      return std::make_unique<EnumExprAst>(enum_type_id,
                                           std::move(struct_expr));
    }

    LOG("failed to parse enum expr");
    return {};
  }

  std::optional<uptr<MemberAccesorExprAst>>
  handle_member_access_expr(int &offset, AstExpression &base_expr) {
    LOG("starting parsing access member");

    if (check_tokens({Dot, Id}, offset)) {
      offset += 2;

      auto id = get_tk(offset - 1).value().value;

      LOG("member access parsed");
      return std::make_unique<MemberAccesorExprAst>(std::move(base_expr), id);
    }

    LOG("failed to parse access member");
    return {};
  }

  std::optional<uptr<SingleMatchExprAst>>
  handle_single_match_expr(int &offset) {
    LOG("starting single match parsing");

    int tmp_offset = offset;
    if (check_tokens({Match}, tmp_offset)) {
      tmp_offset += 1;

      auto match_expr = handle_expr(tmp_offset);

      if (!check_tokens({Colon}, tmp_offset))
        return {};
      tmp_offset += 1;

      throw "FUCK";
    }

    LOG("failed to parse single match");
    return {};
  }

  std::optional<uptr<IfExprAst>> handle_if_expr(int &offset) {
    if (check_tokens({If}, offset)) {
      LOG("parsing if expression");
      offset += 1;

      auto condition = consume_expressions(offset, {LBrace});
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

      auto for_cond_expr = consume_expressions(offset, {LBrace});
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
    auto overloads = ctx->get_overloads(identifier);
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

  std::optional<std::unique_ptr<StructExprAst>>
  handle_struct_expr(int &offset) {
    LOG("parsing struct expr");
    auto identifier = get_tk(offset).value().value;
    auto s = ctx->get_struct(identifier);
    if (!s) {
      LOG("parsing struct failed - no struct found");
      return {};
    }

    // Consume the identifier
    int tmp_offset = offset;
    tmp_offset += 1;

    auto field_assigns = handle_struct_assigments(tmp_offset);

    offset = tmp_offset;

    LOG("struct expr found");
    return std::make_unique<StructExprAst>(s.value()->type,
                                           std::move(field_assigns));
  }

  std::vector<uptr<VarAssignStmtAst>> handle_struct_assigments(int &offset) {
    std::vector<uptr<VarAssignStmtAst>> field_assigns;
    if (check_tokens({Dot, LBrace}, offset)) {
      offset += 2;
      while (!check_tokens({RBrace}, offset)) {
        auto assign = handle_var_assign(offset, {RBrace, Comma});
        if (!assign)
          return {};
        field_assigns.push_back(std::move(*assign));

        if (check_tokens({RBrace}, offset - 1))
          break;
      }
      // No need to increase offset by one, as inside the loop we are checking
      // if the previous line is RBrace
    }
    return field_assigns;
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
    auto ret_type = VOID_TYPE.get_id();

    if (!check_tokens({LPar}, offset))
      return {};
    offset += 1;

    // Prev arguments
    LOG("prefix args:");
    auto prefix = handle_fn_args(offset);
    if (!prefix)
      return {};

    // Divider type
    if (!check_tokens({Bar}, offset))
      return {};
    offset += 1;

    // Next arguments
    LOG("suffix args:");
    auto suffix = handle_fn_args(offset);
    if (!suffix)
      return {};

    if (!check_tokens({RPar}, offset))
      return {};
    offset += 1;

    // Return type
    if (check_tokens({Id}, offset)) {
      LOG("getting fn return type");
      auto type_name = tk_queue[offset].value;
      auto type = ctx->type_db.get_id_by_name(type_name);
      if (!type)
        return {};
      ret_type = *type;
      offset += 1;
    } else {
      LOG("no return type for fn");
    }

    LOG("found fn header");
    return std::make_unique<FnHeaderAst>(
        header_id, ret_type, false, std::move(*prefix), std::move(*suffix));
  }

  std::optional<std::vector<uptr<ArgDefAst>>> handle_fn_args(int &offset) {
    LOG("parsing fn arguments");
    auto args = std::vector<uptr<ArgDefAst>>();

    // No arguments found
    if (check_tokens({Bar}, offset) || check_tokens({RPar}, offset))
      return args;

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

      if (check_tokens({Bar}, offset) || check_tokens({RPar}, offset)) {
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
      auto type_name = tk_queue[offset + 2].value;
      auto ast_type = ctx->type_db.get_id_by_name(type_name);
      if (!ast_type)
        return {};
      auto field =
          std::make_unique<ArgDefAst>(tk_queue[offset].value, *ast_type, false);
      offset += 3;
      return field;
    }

    // Varadic argument
    if (check_tokens({Id, Colon, Dot, Dot, Dot}, offset)) {
      auto field = std::make_unique<ArgDefAst>(tk_queue[offset].value,
                                               VOID_TYPE.get_id(), true);
      offset += 5;
      return field;
    }

    return {};
  }
};
