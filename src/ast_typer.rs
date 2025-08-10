use crate::{
    ast::*,
    meta_fn::{BinOpKind, MetaFn, MetaFnKind, MetaReturnType},
    program_ctx::ProgramCtx,
    types::{AstType, AstTypeId, BOOL_TYPE, F32_TYPE, I32_TYPE, RAW_STRING_TYPE, VOID_TYPE},
};

pub trait AstTyped {
    fn get_type<'a>(&self, ctx: &'a ProgramCtx) -> &'a AstType {
        let id = self.get_type_id(ctx);
        ctx.type_db
            .get_type(id) // `ctx.type_db` is still mutably borrowed from `get_type_id`, but `TypeDB::get_type` takes `&self`. This is okay.
            .unwrap_or_else(|| panic!("Type with ID {id:?} not found in TypeDB"))
    }
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId;
}

impl AstTyped for AstExpression {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        match self {
            AstExpression::Array(array_expr_ast) => array_expr_ast.get_type_id(ctx),
            AstExpression::Index(index_expr_ast) => index_expr_ast.get_type_id(ctx),
            AstExpression::Int(int_expr_ast) => int_expr_ast.get_type_id(ctx),
            AstExpression::Float(float_expr_ast) => float_expr_ast.get_type_id(ctx),
            AstExpression::String(string_expr_ast) => string_expr_ast.get_type_id(ctx),
            AstExpression::Ref(ref_expr_ast) => ref_expr_ast.get_type_id(ctx),
            AstExpression::Deref(deref_expr_ast) => deref_expr_ast.get_type_id(ctx),
            AstExpression::Bool(bool_expr_ast) => bool_expr_ast.get_type_id(ctx),
            AstExpression::Call(call_expr_ast) => call_expr_ast.get_type_id(ctx),
            AstExpression::Body(body_expr_ast) => body_expr_ast.get_type_id(ctx),
            AstExpression::Var(var_expr_ast) => var_expr_ast.get_type_id(ctx),
            AstExpression::MetaDef(meta_def_expr_ast) => meta_def_expr_ast.get_type_id(ctx),
            AstExpression::Statement(statement_expr_ast) => statement_expr_ast.get_type_id(ctx),
            AstExpression::Group(group_expr_ast) => group_expr_ast.get_type_id(ctx),
            AstExpression::If(if_expr_ast) => if_expr_ast.get_type_id(ctx),
            AstExpression::For(for_expr_ast) => for_expr_ast.get_type_id(ctx),
            AstExpression::Struct(struct_expr_ast) => struct_expr_ast.get_type_id(ctx),
            AstExpression::MemberAccessor(member_accesor_expr_ast) => {
                member_accesor_expr_ast.get_type_id(ctx)
            }
            AstExpression::Enum(enum_expr_ast) => enum_expr_ast.get_type_id(ctx),
            AstExpression::SingleMatch(single_match_expr_ast) => {
                single_match_expr_ast.get_type_id(ctx)
            }
            AstExpression::ModuleAccess(module) => module.get_type_id(ctx),
            AstExpression::NoOp(no_op_ast) => no_op_ast.get_type_id(ctx),
        }
    }
}

impl AstTyped for ModuleAccessExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        self.expr.get_type_id(ctx)
    }
}

impl AstTyped for MetaExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        let meta = ctx.get_meta(&self.name).unwrap();

        // Here we are assuming the meta def already validated the arguments for the given type.
        match meta.kind {
            MetaFnKind::BinOp(_) | MetaFnKind::CmpOp(_) => self.args[0].get_type_id(ctx),
        }
    }
}

impl AstTyped for IntExprAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        self.id
    }
}

impl AstTyped for FloatExprAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        self.id
    }
}

impl AstTyped for BoolExprAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        BOOL_TYPE.get_id()
    }
}

impl AstTyped for StringExprAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        RAW_STRING_TYPE.get_id()
    }
}

impl AstTyped for ArrayExprAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        self.type_id
    }
}

impl AstTyped for IndexExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        let base_type_id = self.base.get_type_id(ctx);
        let base_type = ctx
            .type_db
            .get_type(base_type_id)
            .expect("Base type for index expression not found in TypeDB");

        if !base_type.is_array() {
            panic!("Trying to index an expression that's not an array.");
        }
        base_type.get_subtype()
    }
}

impl AstTyped for NoOpAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        VOID_TYPE.get_id()
    }
}

impl AstTyped for RefExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        let expr_type_id = self.expr.get_type_id(ctx);
        ctx.type_db.get_ref(expr_type_id)
    }
}

impl AstTyped for DerefExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        let expr_ptr_id = self.expr.get_type_id(ctx);
        let expr_ptr_type = ctx
            .type_db
            .get_type(expr_ptr_id)
            .expect("Expression pointer type not found in TypeDB");
        expr_ptr_type.get_subtype()
    }
}

impl AstTyped for StructExprAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        self.type_id
    }
}

impl AstTyped for EnumExprAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        self.enum_type
    }
}

impl AstTyped for MemberAccesorExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        let base_type_id = self.base.get_type_id(ctx);
        let mut base_type = ctx
            .type_db
            .get_type(base_type_id)
            .expect("Base type for member accessor not found in TypeDB")
            .clone(); // Clone to allow mutation if it's a reference

        if base_type.is_ref() {
            let subtype_id = base_type.get_subtype();
            base_type = ctx
                .type_db
                .get_type(subtype_id)
                .expect("Subtype for reference in member accessor not found")
                .clone(); // Clone again for potential future mutations if needed
        }

        if let Some(field_node) = &self.field {
            // Assuming `field_node` is a VarExprAst or similar that has a `name` field
            let field_name = &field_node.name; // Adjust based on actual `field` type
            base_type
                .get_field_by_name(field_name)
                .unwrap_or_else(|| {
                    panic!(
                        "Field '{}' not found on type {:?}",
                        field_name,
                        base_type.get_id()
                    )
                })
                .id
        } else if let Some(method) = &self.method {
            let method_ty = base_type
                .get_method_by_name(&method.fn_name, &ctx.type_db)
                .unwrap();
            ctx.type_db
                .get_type(method_ty)
                .unwrap()
                .get_return_type_id()
        } else {
            panic!("No member (field or method) specified for member accessor.");
        }
    }
}

impl AstTyped for VarExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        ctx
            .get_var(&self.name)
            .unwrap_or_else(|| panic!("Defined variable '{}' not found", self.name))
    }
}

impl AstTyped for ArgDefAst {
    fn get_type_id(&self, _ctx: &ProgramCtx) -> AstTypeId {
        self.type_id
    }
}

impl AstTyped for CallExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        ctx.type_db
            .get_type(self.fn_id)
            .expect("Function type not found in TypeDB")
            .get_return_type_id()
    }
}

impl AstTyped for BodyExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        if self.statements.is_empty() {
            return VOID_TYPE.get_id();
        }

        if let AstStatement::StatementExpr(expr) = self.statements.last().unwrap() {
            return expr.expr.get_type_id(ctx);
        }

        VOID_TYPE.get_id()
    }
}

impl AstTyped for StatementExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        self.expr.get_type_id(ctx)
    }
}

impl AstTyped for GroupExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        self.expr.get_type_id(ctx)
    }
}

impl AstTyped for IfExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        self.then_expr.get_type_id(ctx)
    }
}

impl AstTyped for SingleMatchExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        self.then_expr.get_type_id(ctx)
    }
}

impl AstTyped for ForExprAst {
    fn get_type_id(&self, ctx: &ProgramCtx) -> AstTypeId {
        self.for_body.get_type_id(ctx)
    }
}
