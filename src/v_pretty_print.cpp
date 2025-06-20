#include "v_pretty_print.h"
#include <print>
#include <variant>

void PrettyPrintAstVisitor::operator()(uptr<IntExprAst> &node) {
  std::print("{}", node->value);
}

void PrettyPrintAstVisitor::operator()(uptr<FloatExprAst> &node) {
  std::print("{}", node->value);
}

void PrettyPrintAstVisitor::operator()(uptr<StringExprAst> &node) {
  auto str = node->value;
  // Replace new lines to print them raw
  size_t pos = 0;
  while ((pos = str.find('\n', pos)) != std::string::npos) {
    str.replace(pos, 1, "\\n");
    pos += 2;
  }
  while ((pos = str.find('\t', pos)) != std::string::npos) {
    str.replace(pos, 1, "\\t");
    pos += 2;
  }
  std::print("\"{}\"", str);
}

void PrettyPrintAstVisitor::operator()(uptr<VarExprAst> &node) {
  std::print("{}", node->name);
}

void PrettyPrintAstVisitor::operator()(uptr<ArgDefAst> &node) {
  std::print("{}: {}", node->name, get_type_name(node->type));
}

void PrettyPrintAstVisitor::operator()(uptr<CallExprAst> &node) {
  std::print("(");
  for (int i = 0; i < (int)node->prefix_args.size(); i++) {
    std::visit(*this, node->prefix_args[i]);
    if (i != (int)node->prefix_args.size() - 1)
      std::print(", ");
  }
  std::print(")>");

  std::print(" {} ", node->fn_name);

  std::print("<(");
  for (int i = 0; i < (int)node->suffix_args.size(); i++) {
    std::visit(*this, node->suffix_args[i]);
    if (i != (int)node->suffix_args.size() - 1)
      std::print(", ");
  }
  std::print(")");
}

void PrettyPrintAstVisitor::operator()(uptr<FnHeaderAst> &node) {
  std::print("{} :: fn (", node->name);

  for (auto &arg : node->prefix_args) {
    (*this)(arg);
  }

  std::print(" | ");

  for (auto &arg : node->suffix_args) {
    (*this)(arg);
  }
  std::print(")");
  std::print(" {} ", get_type_name(node->ret_type));
}

void PrettyPrintAstVisitor::operator()(uptr<BodyExprAst> &node) {
  std::println("{{");
  indent += 1;
  for (auto &expr : node->statements) {
    print_indent();
    std::visit(*this, expr);
    std::print("\n");
  }
  indent -= 1;
  print_indent();
  std::println("}}");
}

void PrettyPrintAstVisitor::operator()(uptr<StatementExprAst> &node) {
  std::visit(*this, node->expr);
}

void PrettyPrintAstVisitor::operator()(uptr<FnDefAst> &node) {
  (*this)(node->fn_header);
  if (node->fn_header->is_external) {
    std::print("\n");
    return;
  }

  std::visit(*this, *node->body);
}

void PrettyPrintAstVisitor::operator()(uptr<MetaDefExprAst> &node) {
  std::print("@{} ", node->name);
  for (auto &expr : node->args) {
    std::visit(*this, expr);
    std::print(", ");
  }
}

void PrettyPrintAstVisitor::operator()(uptr<VarDefStmtAst> &node) {
  std::print("{} := ", node->name);
  if (node->assignment)
    std::visit(*this, *node->assignment);
}

void PrettyPrintAstVisitor::operator()(uptr<ReturnStmtAst> &node) {
  std::print("return ");
  if (node->expr.has_value())
    std::visit(*this, *node->expr);
}

void PrettyPrintAstVisitor::operator()(uptr<GroupExprAst> &node) {
  std::print("(");
  std::visit(*this, node->expr);
  std::print(")");
}

void PrettyPrintAstVisitor::operator()(uptr<IfExprAst> &node) {
  std::print("if ");
  std::visit(*this, node->condition);
  std::print(" ");
  std::visit(*this, node->then_expr);
  if (node->else_expr) {
    std::print("else ");
    std::visit(*this, *node->else_expr);
  }
  std::print("\n");
}

void PrettyPrintAstVisitor::operator()(uptr<BoolExprAst> &node) {
  std::print("{}", node->value);
}

void PrettyPrintAstVisitor::operator()(uptr<ForExprAst> &node) {
  std::print("for ");
  std::visit(*this, node->condition);
  std::print(" ");
  std::visit(*this, node->for_body);
}

void PrettyPrintAstVisitor::operator()(uptr<VarAssignStmtAst> &node) {
  std::print("{} = ", node->id);
  std::visit(*this, node->rvalue);
}

void PrettyPrintAstVisitor::operator()(uptr<StructDefAst> &node) {
  std::println("{} := struct {{", get_type_name(node->type));
  indent += 1;
  for (auto &field : node->fields) {
    print_indent();
    std::println("{}: {},", field->name, get_type_name(field->type));
  }
  indent -= 1;
  std::println("}}");
}

void PrettyPrintAstVisitor::operator()(uptr<StructExprAst> &node) {
  auto struct_type = ctx->type_db.get_type(node->type).value();
  std::print("{} .{{", struct_type->get_name());
  if (struct_type->is_unit()) {
    std::println("}}");
    return;
  }
  std::println();
  indent += 1;
  for (auto &field : node->fields) {
    print_indent();
    (*this)(field);
    std::print("\n");
  }
  indent -= 1;
  print_indent();
  std::println("}}");
}

void PrettyPrintAstVisitor::operator()(uptr<MemberAccesorExprAst> &node) {
  std::visit(*this, node->base);
  std::print(".{}", node->member);
}
void PrettyPrintAstVisitor::operator()(uptr<EnumDefAst> &node) {
  std::println("{} :: enum {{", get_type_name(node->type));
  indent += 1;
  for (auto &value : node->values) {
    print_indent();
    std::visit(*this, value);
  }
  indent -= 1;
  std::println("}}");
}

void PrettyPrintAstVisitor::operator()(uptr<EnumExprAst> &node) {
  std::print("{}.{}", get_type_name(node->enum_type),
             get_type_name(node->struct_expr->type));
  std::print(".{{");
  auto enum_member_type =
      ctx->type_db.get_type(node->struct_expr->type).value();
  if (enum_member_type->is_unit()) {
    std::println("}}");
    return;
  }
  std::println();

  indent += 1;
  for (auto &field : node->struct_expr->fields) {
    print_indent();
    (*this)(field);
    std::print("\n");
  }
  indent -= 1;
  print_indent();
  std::println("}}");
}
void PrettyPrintAstVisitor::operator()(uptr<SingleMatchExprAst> &node) {
  std::print("match ");
  std::visit(*this, node->enum_expr);
  std::print(" : ");
  (*this)(node->casted_enum_var);
  std::visit(*this, node->then_expr);
  std::print("\n");
}
