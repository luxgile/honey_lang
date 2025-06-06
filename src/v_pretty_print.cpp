#include "v_pretty_print.h"

void PrettyPrintAstVisitor::operator()(uptr<IntExprAst> &node) {
  std::print("{}", node->value);
}

void PrettyPrintAstVisitor::operator()(uptr<FloatExprAst> &node) {
  std::print("{}", node->value);
}

void PrettyPrintAstVisitor::operator()(uptr<StringExprAst> &node) {
  std::print("{}", node->value);
}

void PrettyPrintAstVisitor::operator()(uptr<VarExprAst> &node) {
  std::print("{}", node->name);
}

void PrettyPrintAstVisitor::operator()(uptr<ArgDefAst> &node) {
  std::println("{}: {}", node->name, node->type);
}

void PrettyPrintAstVisitor::operator()(uptr<CallExprAst> &node) {

  std::print("( ");
  for (auto &arg : node->prefix_args) {
    std::visit(*this, arg);
    std::print(", ");
  }
  std::print(" )");

  std::print(" |{}| ", node->fn_name);

  std::print("( ");
  for (auto &arg : node->suffix_args) {
    std::visit(*this, arg);
    std::print(", ");
  }
  std::print(" )");
}

void PrettyPrintAstVisitor::operator()(uptr<FnHeaderAst> &node) {
  std::print("{} := ", node->name);

  for (auto &arg : node->prefix_args) {
    (*this)(arg);
  }

  std::print("|{}|", node->ret_type ? *node->ret_type : " Void ");

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
  std::println("\n}}");
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

