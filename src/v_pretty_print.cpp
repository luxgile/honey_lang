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
  std::print("\"{}\"", node->value);
}

void PrettyPrintAstVisitor::operator()(uptr<VarExprAst> &node) {
  std::print("{}", node->name);
}

void PrettyPrintAstVisitor::operator()(uptr<ArgDefAst> &node) {
  std::print("{}: {}", node->name, node->type.get_name());
}

void PrettyPrintAstVisitor::operator()(uptr<CallExprAst> &node) {
  std::print(" (");
  for (auto &arg : node->prefix_args) {
    std::visit(*this, arg);
    std::print(", ");
  }
  std::print(") ");

  std::print(" |{}| ", node->fn_name);

  std::print(" (");
  for (auto &arg : node->suffix_args) {
    std::visit(*this, arg);
    std::print(", ");
  }
  std::print(") ");
}

void PrettyPrintAstVisitor::operator()(uptr<FnHeaderAst> &node) {
  std::print("{} := ", node->name);

  for (auto &arg : node->prefix_args) {
    (*this)(arg);
  }

  std::print("|{}|", node->ret_type.get_name());

  for (auto &arg : node->suffix_args) {
    (*this)(arg);
  }
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
  std::print("\n");
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
  std::visit(*this, node->for_body);
}

void PrettyPrintAstVisitor::operator()(uptr<VarAssignStmtAst> &node) {
  std::print("{} = ", node->id);
  std::visit(*this, node->rvalue);
}

void PrettyPrintAstVisitor::operator()(uptr<StructDefAst> &node) {
  std::println("{} := struct {{", node->type.get_name());
  indent += 1;
  for (auto &field : node->fields) {
    print_indent();
    std::println("{}: {},", field->name, field->type.get_name());
  }
  indent -= 1;
  std::println("}}");
}

void PrettyPrintAstVisitor::operator()(uptr<StructExprAst> &node) {
  std::println("{} .{{", node->type.get_name());
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
