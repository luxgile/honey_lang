
#include "program_ctx.h"
#include "v_expr_type.h"

/* std::optional<std::string> */
/* OverloadFnGroup::get_mangled_name(CallExprAst *call, */
/*                                   AstExprTypeVisitor *type_visitor) { */
/*   if (fns.size() == 0 || fns[0]->name != call->fn_name) */
/*     return std::nullopt; */
/**/
/*   std::vector<AstTypeId> prefix_args; */
/*   for (auto &prefix : call->prefix_args) { */
/*     prefix_args.push_back(std::visit(*type_visitor, prefix)); */
/*   } */
/**/
/*   std::vector<AstTypeId> suffix_args; */
/*   for (auto &suffix : call->suffix_args) { */
/*     suffix_args.push_back(std::visit(*type_visitor, suffix)); */
/*   } */
/**/
/*   auto fn_touple = get_fn(prefix_args, suffix_args); */
/*   auto fn = std::get<1>(*fn_touple); */
/*   auto idx = std::get<0>(*fn_touple); */
/*   // Main and external fns should not be mangled */
/*   if (fn->name == "main" || fn->is_external) */
/*     return call->fn_name; */
/**/
/*   if (!fn_touple) */
/*     return std::nullopt; */
/**/
/*   return fn->name + "_" + std::to_string(idx); */
/* } */
/**/
/* std::optional<std::string> OverloadFnGroup::get_mangled_name(FnHeaderAst *fn) { */
/*   // Main and external fns should not be mangled */
/*   if (fn->name == "main" || fn->is_external) */
/*     return fn->name; */
/**/
/*   auto fn_overload = get_fn(fn); */
/*   if (!fn_overload) */
/*     return std::nullopt; */
/**/
/*   return std::get<1>(*fn_overload)->name + "_" + */
/*          std::to_string(std::get<0>(*fn_overload)); */
/* } */
AstTypeId ProgramCtx::define_fn(std::string name, FnHeaderAst *fn,
                                std::optional<AstTypeId> parent_struct) {
  // Convert arguments
  std::vector<AstNamedType> pre_args;
  for (auto &pre : fn->prefix_args)
    pre_args.push_back(AstNamedType{pre->name, pre->type, false});

  std::vector<AstNamedType> su_args;
  for (auto &su : fn->suffix_args)
    su_args.push_back(AstNamedType{su->name, su->type, su->is_varadic});

  auto fns = type_db.get_fns_by_name(name);
  auto overload_n = fns.size();
  auto registered_fn = type_db.get_fn_by_args(name, pre_args, su_args);
  if (registered_fn) {
    auto registered_fn_ty = type_db.get_type(*registered_fn);
    overload_n = registered_fn_ty.value()->get_overload();
  }

  auto fn_ty = type_db.new_fn(name, overload_n, pre_args, su_args, fn->ret_type,
                              parent_struct);

  return fn_ty;
}

void ProgramCtx::define_llvm_type(AstTypeId type_id, llvm::Type *type) {
  llvm_types[type_id] = type;
}

std::optional<llvm::Type *> ProgramCtx::get_llvm_type(AstTypeId id) {
  auto ty = type_db.get_type(id);
  if (!ty)
    return std::nullopt;

  if (ty.value()->is_ref())
    return ptr_llvm_ty;

  if (ty.value()->is_array()) {
    auto base_type = get_llvm_type(ty.value()->get_subtype()).value();
    return llvm::ArrayType::get(base_type, ty.value()->get_array_size());
  }

  if (!llvm_types.contains(id))
    return std::nullopt;
  return llvm_types[id];
}

std::optional<EnumDefAst *> ProgramCtx::get_enum(std::string name) {
  auto e = enums[name];
  if (e == nullptr)
    return std::nullopt;
  return e;
}

void ProgramCtx::define_struct(std::string name, StructDefAst *value) {
  /* if (structs.contains(name)) */
  /*   throw "trying to redefine a struct"; */
  /**/
  structs[name] = value;
}

void ProgramCtx::define_enum(std::string name, EnumDefAst *value) {
  enums[name] = value;
}

std::optional<StructDefAst *> ProgramCtx::get_struct(std::string name) {
  if (!structs.contains(name))
    return std::nullopt;
  return structs[name];
}

void ProgramCtx::define_meta(std::string name, MetaFunction *meta) {
  defined_meta[name] = meta;
}

std::optional<MetaFunction *> ProgramCtx::get_meta(std::string name) {
  auto meta = defined_meta[name];
  if (meta == nullptr)
    return std::nullopt;
  return meta;
}

void ProgramCtx::define_primitive(std::string name, AstTypeId id) {
  primitives.insert({name, id});
}

