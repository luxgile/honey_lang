
#include "program_ctx.h"
#include "v_expr_type.h"

std::optional<std::string>
OverloadFnGroup::get_mangled_name(CallExprAst *call,
                                  AstExprTypeVisitor *type_visitor) {
  if (fns.size() == 0 || fns[0]->name != call->fn_name)
    return std::nullopt;

  std::vector<AstTypeId> prefix_args;
  for (auto &prefix : call->prefix_args) {
    prefix_args.push_back(std::visit(*type_visitor, prefix));
  }

  std::vector<AstTypeId> suffix_args;
  for (auto &suffix : call->suffix_args) {
    suffix_args.push_back(std::visit(*type_visitor, suffix));
  }

  auto fn_touple = get_fn(prefix_args, suffix_args);
  auto fn = std::get<1>(*fn_touple);
  auto idx = std::get<0>(*fn_touple);
  // Main and external fns should not be mangled
  if (fn->name == "main" || fn->is_external)
    return call->fn_name;

  if (!fn_touple)
    return std::nullopt;

  return fn->name + "_" + std::to_string(idx);
}

std::optional<std::string> OverloadFnGroup::get_mangled_name(FnHeaderAst *fn) {
  // Main and external fns should not be mangled
  if (fn->name == "main" || fn->is_external)
    return fn->name;

  auto fn_overload = get_fn(fn);
  if (!fn_overload)
    return std::nullopt;

  return std::get<1>(*fn_overload)->name + "_" +
         std::to_string(std::get<0>(*fn_overload));
}
