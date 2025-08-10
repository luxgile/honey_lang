use crate::{ast::*, program_ctx::ProgramCtx, types::AstTypeId};

fn get_type_name(ty: AstTypeId, ctx: &ProgramCtx) -> String {
    ctx.type_db.get_type(ty).unwrap().get_name().to_string()
}

fn print_indent_spaces(indent: u32) {
    for _ in 0..indent {
        print!("  ");
    }
}

pub trait AstPrint {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32);
}

impl AstPrint for AstStatement {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        match self {
            AstStatement::ArgDef(arg_def_ast) => arg_def_ast.print_ast(ctx, indent),
            AstStatement::FnHeader(fn_header_ast) => fn_header_ast.print_ast(ctx, indent),
            AstStatement::FnDef(fn_def_ast) => fn_def_ast.print_ast(ctx, indent),
            AstStatement::StatementExpr(statement_expr_ast) => {
                statement_expr_ast.print_ast(ctx, indent)
            }
            AstStatement::VarDefStmt(var_def_stmt_ast) => var_def_stmt_ast.print_ast(ctx, indent),
            AstStatement::ReturnStmt(return_stmt_ast) => return_stmt_ast.print_ast(ctx, indent),
            AstStatement::VarAssignStmt(var_assign_stmt_ast) => {
                var_assign_stmt_ast.print_ast(ctx, indent)
            }
            AstStatement::StructDef(struct_def_ast) => struct_def_ast.print_ast(ctx, indent),
            AstStatement::EnumDef(enum_def_ast) => enum_def_ast.print_ast(ctx, indent),
            AstStatement::File(file) => file.print_ast(ctx, indent),
            AstStatement::Defer(defer) => defer.print_ast(ctx, indent),
            AstStatement::Module(module) => module.print_ast(ctx, indent),
            AstStatement::Import(import) => import.print_ast(ctx, indent),
        }
    }
}

impl AstPrint for ImportStmtAst {
    fn print_ast(&self, _ctx: &ProgramCtx, indent: u32) {
        print_indent_spaces(indent);
        println!("{} :: import \"{}\"", self.id, self.path);
    }
}

impl AstPrint for ModuleStmtAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print_indent_spaces(indent);
        print!("module ");
        self.id.print_ast(ctx, indent);
        println!(" {{");
        for stmt in &self.stmts {
            stmt.print_ast(ctx, indent + 1);
        }
        print_indent_spaces(indent);
        println!("}}");
    }
}

impl AstPrint for ModuleId {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("{}", self.name);
        if let Some(child) = &self.child {
            print!(".");
            child.print_ast(ctx, indent);
        }
    }
}

impl AstPrint for DeferStmtAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("defer ");
        self.stmt.print_ast(ctx, indent);
    }
}

impl AstPrint for FileStmtAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        println!("file: {}", self.filename);
        println!();
        self.statements.iter().for_each(|x| {
            x.print_ast(ctx, indent);
            println!();
        });
    }
}

impl AstPrint for AstExpression {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        match self {
            AstExpression::Array(array_expr_ast) => array_expr_ast.print_ast(ctx, indent),
            AstExpression::Index(index_expr_ast) => index_expr_ast.print_ast(ctx, indent),
            AstExpression::Int(i) => i.print_ast(ctx, indent),
            AstExpression::Float(f) => f.print_ast(ctx, indent),
            AstExpression::String(string_expr_ast) => string_expr_ast.print_ast(ctx, indent),
            AstExpression::Ref(ref_expr_ast) => ref_expr_ast.print_ast(ctx, indent),
            AstExpression::Deref(deref_expr_ast) => deref_expr_ast.print_ast(ctx, indent),
            AstExpression::Bool(bool_expr_ast) => bool_expr_ast.print_ast(ctx, indent),
            AstExpression::Call(call_expr_ast) => call_expr_ast.print_ast(ctx, indent),
            AstExpression::Body(body_expr_ast) => body_expr_ast.print_ast(ctx, indent),
            AstExpression::Var(var_expr_ast) => var_expr_ast.print_ast(ctx, indent),
            AstExpression::MetaDef(meta_def_expr_ast) => meta_def_expr_ast.print_ast(ctx, indent),
            AstExpression::Statement(statement_expr_ast) => {
                statement_expr_ast.print_ast(ctx, indent)
            }
            AstExpression::Group(group_expr_ast) => group_expr_ast.print_ast(ctx, indent),
            AstExpression::If(if_expr_ast) => if_expr_ast.print_ast(ctx, indent),
            AstExpression::For(for_expr_ast) => for_expr_ast.print_ast(ctx, indent),
            AstExpression::Struct(struct_expr_ast) => struct_expr_ast.print_ast(ctx, indent),
            AstExpression::MemberAccessor(member_accesor_expr_ast) => {
                member_accesor_expr_ast.print_ast(ctx, indent)
            }
            AstExpression::Enum(enum_expr_ast) => enum_expr_ast.print_ast(ctx, indent),
            AstExpression::SingleMatch(single_match_expr_ast) => {
                single_match_expr_ast.print_ast(ctx, indent)
            }
            AstExpression::ModuleAccess(module) => module.print_ast(ctx, indent),
            AstExpression::Type(ty) => ty.print_ast(ctx, indent),
            AstExpression::NoOp(_) => {}
        }
    }
}

impl AstPrint for TypeExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, _indent: u32) {
        let ty = ctx.type_db.get_type(self.id).unwrap();
        print!("{}", ty.get_name());
    }
}

impl AstPrint for ModuleAccessExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        self.expr.print_ast(ctx, indent);
    }
}

impl AstPrint for IntExprAst {
    fn print_ast(&self, _ctx: &ProgramCtx, _indent: u32) {
        print!("{}", self.value);
    }
}

impl AstPrint for FloatExprAst {
    fn print_ast(&self, _ctx: &ProgramCtx, _indent: u32) {
        print!("{}", self.value);
    }
}

impl AstPrint for StringExprAst {
    fn print_ast(&self, _ctx: &ProgramCtx, _indent: u32) {
        let mut str = self.value.clone(); // Clone to modify
        str = str.replace('\n', "\\n");
        str = str.replace('\t', "\\t");
        print!("\"{str}\"");
    }
}

impl AstPrint for VarExprAst {
    fn print_ast(&self, _ctx: &ProgramCtx, _indent: u32) {
        print!("{}", self.name);
    }
}

impl AstPrint for ArgDefAst {
    fn print_ast(&self, ctx: &ProgramCtx, _indent: u32) {
        print!("{}: {}", self.name, get_type_name(self.type_id, ctx));
    }
}

impl AstPrint for CallExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("(");
        for (i, arg) in self.prefix_args.iter().enumerate() {
            arg.print_ast(ctx, indent);
            if i != self.prefix_args.len() - 1 {
                print!(", ");
            }
        }
        print!(")>");

        print!(" {} ", self.fn_name);

        print!("<(");
        for (i, arg) in self.suffix_args.iter().enumerate() {
            arg.print_ast(ctx, indent);
            if i != self.suffix_args.len() - 1 {
                print!(", ");
            }
        }
        print!(")");
    }
}

impl AstPrint for FnHeaderAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print_indent_spaces(indent);
        print!("{} :: fn (", self.name);

        for arg in &self.prefix_args {
            arg.print_ast(ctx, indent);
        }

        print!(" | ");

        for (i, arg) in self.suffix_args.iter().enumerate() {
            arg.print_ast(ctx, indent);
            if i != self.suffix_args.len() - 1 {
                print!(", ");
            }
        }
        print!(")");
        print!(" {} ", get_type_name(self.ret_type, ctx));
    }
}

impl AstPrint for BodyExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        println!("{{");
        let next_indent = indent + 1;
        for expr in &self.statements {
            print_indent_spaces(next_indent);
            expr.print_ast(ctx, next_indent);
            println!();
        }
        print_indent_spaces(indent);
        println!("}}");
    }
}

impl AstPrint for StatementExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        self.expr.print_ast(ctx, indent);
    }
}

impl AstPrint for FnDefAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        self.fn_header.print_ast(ctx, indent);
        if self.fn_header.is_external {
            println!();
            return;
        }

        self.body.as_ref().inspect(|x| x.print_ast(ctx, indent));
    }
}

impl AstPrint for MetaExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("@{} ", self.name);
        for expr in &self.args {
            expr.print_ast(ctx, indent);
            print!(", ");
        }
    }
}

impl AstPrint for VarDefStmtAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("{} := ", self.name);
        if let Some(assignment) = &self.assignment {
            assignment.print_ast(ctx, indent);
        }
    }
}

impl AstPrint for ReturnStmtAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("return ");
        if let Some(expr) = &self.expr {
            expr.print_ast(ctx, indent);
        }
    }
}

impl AstPrint for GroupExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("(");
        self.expr.print_ast(ctx, indent);
        print!(")");
    }
}

impl AstPrint for IfExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("if ");
        self.condition.print_ast(ctx, indent);
        print!(" ");
        self.then_expr.print_ast(ctx, indent);
        if let Some(else_expr) = &self.else_expr {
            print!("else ");
            else_expr.print_ast(ctx, indent);
        }
        println!();
    }
}

impl AstPrint for BoolExprAst {
    fn print_ast(&self, _ctx: &ProgramCtx, _indent: u32) {
        print!("{}", self.value);
    }
}

impl AstPrint for ForExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("for ");
        self.condition.print_ast(ctx, indent);
        print!(" ");
        self.for_body.print_ast(ctx, indent);
    }
}

impl AstPrint for VarAssignStmtAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        self.lvalue.print_ast(ctx, indent);
        print!(" = ");
        self.rvalue.print_ast(ctx, indent);
    }
}

impl AstPrint for StructDefAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        let struct_ty = ctx.type_db.get_type(self.type_id).unwrap();
        if struct_ty.is_unit() {
            println!("{} := struct", get_type_name(self.type_id, ctx));
            return;
        }
        println!("{} := struct {{", get_type_name(self.type_id, ctx));
        let next_indent = indent + 1;
        for field in &self.fields {
            print_indent_spaces(next_indent);
            println!("{}: {},", field.name, get_type_name(field.type_id, ctx));
        }
        for method in &self.methods {
            print_indent_spaces(next_indent);
            method.print_ast(ctx, next_indent);
        }
        print_indent_spaces(indent);
        println!("}}");
    }
}

impl AstPrint for StructExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        let struct_type = ctx
            .type_db
            .get_type(self.type_id)
            .expect("Struct type not found in type_db");
        print!("{} .{{", struct_type.get_name()); // Using clone for the placeholder get_type_name

        if struct_type.is_unit() {
            println!("}}");
            return;
        }
        println!();
        let next_indent = indent + 1;
        for field in &self.fields {
            print_indent_spaces(next_indent);
            print!(".{} = ", field.name);
            field.rvalue.print_ast(ctx, next_indent);
            println!(",");
        }
        print_indent_spaces(indent);
        println!("}}");
    }
}

impl AstPrint for MemberAccesorExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        self.base.print_ast(ctx, indent);
        print!(".");
        if let Some(field) = &self.field {
            field.print_ast(ctx, indent);
        }
        if let Some(method) = &self.method {
            method.print_ast(ctx, indent);
        }
    }
}

impl AstPrint for EnumDefAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        println!("{} :: enum {{", get_type_name(self.type_id, ctx));
        let next_indent = indent + 1;
        for value in &self.values {
            print_indent_spaces(next_indent);
            value.print_ast(ctx, next_indent); // Assuming enum values are AstExpressions
        }
        print_indent_spaces(indent);
        println!("}}");
    }
}

impl AstPrint for EnumExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        let enum_type_name = get_type_name(self.enum_type, ctx);
        let struct_expr_type_name = get_type_name(self.struct_expr.type_id, ctx); // Assuming struct_expr has a 'ty' field
        print!("{enum_type_name}.{struct_expr_type_name}");
        print!(".{{");
        let enum_member_type = ctx
            .type_db
            .get_type(self.struct_expr.type_id)
            .expect("Enum member type not found in type_db");
        if enum_member_type.is_unit() {
            println!("}}");
            return;
        }
        println!();

        let next_indent_level1 = indent + 1;
        for field in &self.struct_expr.fields {
            print_indent_spaces(next_indent_level1);
            print!("{} :: ", field.name); // Assuming field has a 'name' field
            let next_indent_level2 = next_indent_level1 + 1;
            field.rvalue.print_ast(ctx, next_indent_level2); // Assuming field has an 'rvalue' field which is an AstExpression
            println!(",");
        }
        print_indent_spaces(indent);
        println!("}}");
    }
}

impl AstPrint for SingleMatchExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("match ");
        self.enum_expr.print_ast(ctx, indent);
        print!(" : ");
        self.casted_enum_var.print_ast(ctx, indent);
        self.then_expr.print_ast(ctx, indent);
        println!();
    }
}

impl AstPrint for RefExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("&");
        self.expr.print_ast(ctx, indent);
    }
}

impl AstPrint for DerefExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("^");
        self.expr.print_ast(ctx, indent);
    }
}

impl AstPrint for ArrayExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        print!("[");
        for (i, expr) in self.elements.iter().enumerate() {
            expr.print_ast(ctx, indent);
            if i != self.elements.len() - 1 {
                print!(", ");
            }
        }
        print!("]");
    }
}

impl AstPrint for IndexExprAst {
    fn print_ast(&self, ctx: &ProgramCtx, indent: u32) {
        self.base.print_ast(ctx, indent);
        print!("[");
        self.index.print_ast(ctx, indent);
        print!("]");
    }
}
