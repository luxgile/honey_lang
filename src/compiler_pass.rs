use std::str::{self, FromStr};

use crate::{
    ast::*,
    ast_typer::AstTyped,
    lexer::ALLOWED_ID_CHARS,
    meta_fn::{self, BinOpKind, CmpOpKind, MetaFnKind},
    program_ctx::ProgramCtx,
    types::{AstTypeId, VOID_TYPE},
};

pub trait CompilerPass<T> {
    fn run(self, ctx: &ProgramCtx, file: &FileStmtAst) -> T;
}

#[derive(Default)]
pub struct TranspilerFrame {
    queued_defers: Vec<DeferStmtAst>,
}

#[derive(Default)]
pub enum TranspileActiveFile {
    #[default]
    Source,
    Header,
}

#[derive(Default)]
pub struct CTranspilerPass {
    source: String,
    header: String,
    active_file: TranspileActiveFile,

    indent: usize,
    frames: Vec<TranspilerFrame>,
    expr_temp_idx: u32,
    curr_return: Option<String>,
    /// If false, all expressions will create a value and return it.
    /// If true, all expressions will return the expression directly instead.
    raw_mode: bool,
}

impl CompilerPass<(String, String)> for CTranspilerPass {
    fn run(mut self, ctx: &ProgramCtx, file: &FileStmtAst) -> (String, String) {
        self.transpile_file(ctx, file);
        (self.header, self.source)
    }
}

impl CTranspilerPass {
    fn get_active_file(&mut self) -> &mut String {
        match self.active_file {
            TranspileActiveFile::Source => &mut self.source,
            TranspileActiveFile::Header => &mut self.header,
        }
    }

    fn push_frame(&mut self) -> &TranspilerFrame {
        self.frames.push(TranspilerFrame::default());
        self.frames.last().unwrap()
    }

    fn pop_frame(&mut self) {
        self.frames.pop();
    }

    fn curr_frame(&mut self) -> &mut TranspilerFrame {
        self.frames.last_mut().unwrap()
    }

    fn get_all_current_defers(&self) -> Vec<DeferStmtAst> {
        let mut defers = Vec::new();
        for frame in self.frames.iter().rev() {
            frame
                .queued_defers
                .iter()
                .for_each(|x| defers.push(x.clone()));
        }
        defers
    }

    fn hun_type_to_c(ctx: &ProgramCtx, id: &AstTypeId) -> String {
        let ty = ctx.type_db.get_type(*id).unwrap();
        if ty.is_array() {
            return CTranspilerPass::hun_type_to_c(ctx, &ty.get_subtype());
        }

        if ty.is_ref() {
            return CTranspilerPass::hun_type_to_c(ctx, &ty.get_subtype()) + "*";
        }

        match ty.get_name() {
            "i8" => "int8_t",
            "i16" => "int16_t",
            "i32" => "int32_t",
            "i64" => "int64_t",
            "f32" => "float",
            "f64" => "double",
            "bool" => "bool",
            "cstring" => "char*",
            _ => return ty.get_fullname(&ctx.type_db).to_string(),
        }
        .to_string()
    }

    fn mangle_id(str: &str) -> String {
        let mut mangled = String::new();
        str.chars().for_each(|c| {
            if c == '_' {
                mangled.push(c);
            } else if ALLOWED_ID_CHARS.contains(&c) {
                mangled += format!("_{}", c as u32).as_str();
            } else {
                mangled.push(c);
            }
        });
        mangled
    }

    fn indent_space(&self) -> String {
        "  ".repeat(self.indent)
    }

    fn get_temp_expr_id(&self) -> String {
        "__".to_string() + self.expr_temp_idx.to_string().as_str()
    }

    fn gen_temp_expr(&mut self, ctx: &ProgramCtx, ty_id: &AstTypeId) -> (String, String) {
        let id = self.get_temp_expr_id();
        self.expr_temp_idx += 1;
        (CTranspilerPass::hun_type_to_c(ctx, ty_id), id)
    }

    fn transpile_file(&mut self, ctx: &ProgramCtx, file: &FileStmtAst) {
        self.header += "// auto generated file from honey - don't modify manually\n";
        self.header += format!("// file: {}.hun\n\n", file.filename).as_str();
        self.header += "#include <stdio.h>\n";
        self.header += "#include <stdint.h>\n";
        self.header += "\n";

        self.source += "// auto generated file from honey - don't modify manually\n";
        self.source += format!("// file: {}.hun\n\n", file.filename).as_str();
        // This depends on if we are actually creating the files or just compiling from memory
        self.source += &format!("#include \"{}.h\"\n\n", file.filename);

        for stmt in &file.statements {
            self.transpile_statement(ctx, stmt);
        }
    }

    fn transpile_statement(&mut self, ctx: &ProgramCtx, stmt: &AstStatement) {
        match stmt {
            AstStatement::StructDef(s) => self.transpile_struct(ctx, s),
            AstStatement::EnumDef(e) => self.transpile_enum(ctx, e),
            AstStatement::FnDef(f) => self.transpile_fn(ctx, f),
            AstStatement::StatementExpr(e) => {
                let expr_str = self.transpile_expr(ctx, &e.expr);
                self.source += &expr_str;
            }
            AstStatement::VarDefStmt(def) => self.transpile_var_def(ctx, def),
            AstStatement::VarAssignStmt(assign) => self.transpile_var_assign(ctx, assign),
            AstStatement::ReturnStmt(ret) => self.transpile_return(ctx, ret, true),
            AstStatement::Defer(defer) => self.curr_frame().queued_defers.push(*defer.clone()),
            AstStatement::Module(module) => self.transpile_module(ctx, module),
            AstStatement::Import(import) => self.transpile_import(ctx, import),
            _ => {
                todo!("{:?} not implemented", stmt);
            }
        };
    }

    fn transpile_import(&mut self, ctx: &ProgramCtx, import: &ImportStmtAst) {
        let transpiler = CTranspilerPass::default();
        let (header, src) = transpiler.run(ctx, &import.ast);

        println!("src: {src}\n");
        println!("header: {header}");
        todo!(
            "need to create a new file based on this; the transpiler might need to create the files directly based on a folder"
        );
    }

    fn transpile_module(&mut self, ctx: &ProgramCtx, module: &ModuleStmtAst) {
        for stmt in &module.stmts {
            self.transpile_statement(ctx, stmt);
        }
    }

    fn transpile_return(&mut self, ctx: &ProgramCtx, ret: &ReturnStmtAst, include_defers: bool) {
        if include_defers {
            self.transpile_current_defers(ctx);
        }

        self.source += &self.indent_space();
        self.source += "return ";
        if let Some(expr) = &ret.expr {
            let expr_str = self.transpile_expr(ctx, expr);
            self.source += &expr_str;
        }
        self.source += ";\n";
    }

    fn transpile_var_assign(&mut self, ctx: &ProgramCtx, assign: &VarAssignStmtAst) {
        let rvalue = self.transpile_expr(ctx, &assign.rvalue);
        let old_assign_mode = self.raw_mode;
        self.source += &self.indent_space();
        self.raw_mode = true;
        self.transpile_expr(ctx, &assign.lvalue);
        self.raw_mode = old_assign_mode;
        self.source += &format!(" = {rvalue};\n");
        // self.source += &rvalue;
    }

    fn transpile_var_def(&mut self, ctx: &ProgramCtx, def: &VarDefStmtAst) {
        let ty = ctx.type_db.get_type(def.type_id).unwrap();

        let expr_str = if let Some(expr) = &def.assignment {
            self.transpile_expr(ctx, expr)
        } else {
            "".to_string()
        };

        self.source += &self.indent_space();
        self.source += CTranspilerPass::hun_type_to_c(ctx, &def.type_id).as_str();
        self.source += " ";
        self.source += def.name.as_str();
        if ty.is_array() {
            self.source += "[]"
        }
        self.source += " = ";
        self.source += &expr_str;
        self.source += ";\n";
    }

    fn transpile_struct(&mut self, ctx: &ProgramCtx, s: &StructDefAst) {
        let struct_ty = ctx.type_db.get_type(s.type_id).unwrap();
        let struct_name = struct_ty.get_fullname(&ctx.type_db);
        if struct_ty.is_unit() {
            self.header += &self.indent_space();
            self.header.push_str(&format!(
                "typedef struct {struct_name} {{}} {struct_name};\n"
            ));
            return;
        }

        self.header += &self.indent_space();
        self.header += format!("typedef struct {struct_name} {{\n").as_str();

        self.indent += 1;
        for field in &s.fields {
            self.header += &self.indent_space();
            self.header += format!(
                "{} {};\n",
                CTranspilerPass::hun_type_to_c(ctx, &field.type_id),
                field.name
            )
            .as_str();
        }
        self.indent -= 1;

        self.header += &self.indent_space();
        self.header += "} ";
        self.header += &struct_name;
        self.header += ";\n\n";

        for method in &s.methods {
            self.transpile_fn(ctx, method);
            self.source += "\n";
            self.header += "\n";
        }
    }

    fn transpile_enum(&mut self, ctx: &ProgramCtx, e: &EnumDefAst) {
        let e_ty = ctx.type_db.get_type(e.type_id).unwrap();
        let enum_name = e_ty.get_fullname(&ctx.type_db);

        // Variant struct definitions before the actual enum
        for variant in e_ty.get_fields() {
            let v_ty = ctx.type_db.get_type(variant.id).unwrap();
            let struct_def = StructDefAst {
                type_id: variant.id,
                fields: v_ty
                    .get_fields()
                    .iter()
                    .map(|x| ArgDefAst {
                        name: x.name.clone(),
                        type_id: x.id,
                        is_varadic: x.is_varadic,
                    })
                    .collect(),
                methods: Vec::new(),
            };
            self.transpile_struct(ctx, &struct_def);
            self.header += "\n";
        }

        // Enum union
        self.header += &self.indent_space();
        let union_name = &format!("__{enum_name}_union");
        self.header += &format!("typedef union {union_name} {{\n");
        self.indent += 1;
        for (i, variant) in e_ty.get_fields().iter().enumerate() {
            let v_ty = ctx.type_db.get_type(variant.id).unwrap();
            self.header += &self.indent_space();
            self.header += format!(
                "{} __variant_{};\n",
                CTranspilerPass::hun_type_to_c(ctx, &v_ty.get_id()),
                i
            )
            .as_str();
        }
        self.indent -= 1;
        self.header += &self.indent_space();
        self.header += &format!("}} {union_name} ;\n\n");

        // Enum declaration as a tagged union
        self.header += format!("typedef struct {enum_name} {{\n").as_str();
        self.indent += 1;

        // Index
        self.header += &self.indent_space();
        self.header += "int __variant_index;\n";

        // Union
        self.header += &self.indent_space();
        self.header += &format!("{union_name} __variant_value;\n");

        self.indent -= 1;
        self.header += &self.indent_space();
        self.header += "} ";
        self.header += enum_name.as_str();
        self.header += ";\n";
    }

    fn transpile_fn(&mut self, ctx: &ProgramCtx, func: &FnDefAst) {
        let fn_ty = ctx.type_db.get_type(func.id).unwrap();

        if func.fn_header.is_external {
            return;
        }

        let mut fn_header_str = String::new();

        fn_header_str += CTranspilerPass::hun_type_to_c(ctx, &func.fn_header.ret_type).as_str(); // fn type
        fn_header_str += format!(
            " {}",
            CTranspilerPass::mangle_id(&fn_ty.get_fullname(&ctx.type_db))
        )
        .as_str(); // fn name
        fn_header_str += "(";

        // Transpile arguments
        let args = [&fn_ty.get_pre_args()[..], &fn_ty.get_su_args()[..]].concat();
        for (i, arg) in args.iter().enumerate() {
            if arg.is_varadic {
                fn_header_str += "...";
            } else {
                fn_header_str += format!(
                    "{} {}",
                    CTranspilerPass::hun_type_to_c(ctx, &arg.id),
                    arg.name
                )
                .as_str();
                if i != args.len() - 1 {
                    fn_header_str += ", ";
                }
            }
        }

        fn_header_str += ")";

        self.header += &format!("{fn_header_str};\n");
        self.source += &fn_header_str;

        self.expr_temp_idx = 0;
        if let Some(AstExpression::Body(body)) = &func.body {
            self.transpile_fn_body(ctx, body);
        }
    }

    fn transpile_expr(&mut self, ctx: &ProgramCtx, expr: &AstExpression) -> String {
        match expr {
            AstExpression::Bool(b) => { if b.value { "true " } else { "false" } }.to_string(),
            AstExpression::Int(i) => i.value.to_string(),
            AstExpression::Float(f) => f.value.to_string(),
            AstExpression::String(s) => format!("\"{}\"", s.value.escape_debug()),
            AstExpression::Array(arr) => self.transpile_array(ctx, arr),
            AstExpression::Index(idx) => self.transpile_index(ctx, idx),
            AstExpression::Ref(r) => self.transpile_ref(ctx, r),
            AstExpression::Deref(d) => self.transpile_deref(ctx, d),
            AstExpression::Call(call) => self.transpile_call(ctx, call, None),
            AstExpression::Body(body) => self.transpile_body(ctx, body),
            AstExpression::Var(var) => self.transpile_var(ctx, var),
            AstExpression::MetaDef(meta) => self.transpile_meta(ctx, meta),
            AstExpression::Statement(stmt) => self.transpile_expr(ctx, &stmt.expr),
            AstExpression::Group(group) => self.transpile_expr(ctx, &group.expr),
            AstExpression::If(i) => self.transpile_if(ctx, i),
            AstExpression::For(f) => self.transpile_loop(ctx, f),
            AstExpression::Struct(s) => self.transpile_struct_expr(ctx, s),
            AstExpression::MemberAccessor(member) => self.transpile_member_access(ctx, member),
            AstExpression::Enum(enum_expr) => self.transpile_enum_expr(ctx, enum_expr),
            AstExpression::SingleMatch(match_expr) => {
                self.transpile_single_match_expr(ctx, match_expr)
            }
            AstExpression::ModuleAccess(module) => self.transpile_module_access(ctx, module),
            AstExpression::NoOp(_) => "".to_string(),
        }
    }

    fn transpile_module_access(
        &mut self,
        ctx: &ProgramCtx,
        module: &ModuleAccessExprAst,
    ) -> String {
        self.transpile_expr(ctx, &module.expr)
    }

    fn transpile_single_match_expr(&mut self, ctx: &ProgramCtx, m: &SingleMatchExprAst) -> String {
        let s_type = ctx.type_db.get_type(m.casted_enum_var.type_id).unwrap();
        let is_void = m.get_type_id(ctx) == VOID_TYPE.get_id();
        let variant_idx = s_type
            .get_parent(&ctx.type_db)
            .get_field_index_by_id(s_type.get_id());

        let (ty, val) = self.gen_temp_expr(
            ctx,
            &s_type
                .get_parent(&ctx.type_db)
                .get_field_by_idx(variant_idx)
                .id,
        );

        if !is_void {
            self.source += &self.indent_space();
            self.source += &format!("{ty} {val};\n");
        }

        self.source += &self.indent_space();
        self.source += "if (";
        let expr_str = self.transpile_expr(ctx, &m.enum_expr);
        self.source += &expr_str;
        self.source += ".__variant_index == ";
        self.source += variant_idx.to_string().as_str();
        self.source += ") {\n";
        self.indent += 1;

        // Create casted value
        self.source += &self.indent_space();
        self.source += &format!(
            "{} {} = {}.__variant_value.__variant_{};\n",
            CTranspilerPass::hun_type_to_c(ctx, &m.casted_enum_var.type_id),
            m.casted_enum_var.name,
            expr_str,
            variant_idx
        );

        let old_tmp = self.curr_return.clone();
        self.curr_return = Some(val.clone());
        if let AstExpression::Body(body) = &m.then_expr {
            self.transpile_statements(ctx, &body.statements, &val);
            self.indent -= 1;
            self.source += &self.indent_space();
            self.source += "}\n";
        } else {
            unreachable!();
        }
        // self.transpile_expr(ctx, &m.then_expr);
        self.curr_return = old_tmp;
        if is_void { "".to_string() } else { val }
    }

    fn transpile_enum_expr(&mut self, ctx: &ProgramCtx, e: &EnumExprAst) -> String {
        let enum_ty = ctx.type_db.get_type(e.enum_type).unwrap();
        let variant_idx = enum_ty.get_field_index_by_id(e.struct_expr.type_id);

        let mut enum_str = String::new();
        enum_str += " { ";
        enum_str += variant_idx.to_string().as_str();
        enum_str += ", ";
        enum_str += &format!("{{ .__variant_{variant_idx} = ");
        enum_str += &self.transpile_struct_expr(ctx, &e.struct_expr);
        enum_str += " }}";
        enum_str
    }

    fn transpile_member_access(
        &mut self,
        ctx: &ProgramCtx,
        member: &MemberAccesorExprAst,
    ) -> String {
        let member_ty = member.get_type_id(ctx);
        let is_void = member_ty == VOID_TYPE.get_id();
        let (ty, val) = self.gen_temp_expr(ctx, &member_ty);

        let old_assign_mode = self.raw_mode;
        self.raw_mode = false;
        let base = self.transpile_expr(ctx, &member.base);
        let member_op = if member.base.get_type(ctx).is_ref() {
            "->"
        } else {
            "."
        };
        let member = if let Some(field) = &member.field {
            self.transpile_var(ctx, field)
        } else if let Some(method) = &member.method {
            self.transpile_call(ctx, method, Some(base.clone()))
        } else {
            unreachable!()
        };
        self.raw_mode = old_assign_mode;

        if !is_void {
            self.source += &self.indent_space();
            if self.raw_mode {
                self.source += &format!("{base}{member_op}{member}");
                return String::new();
            } else {
                self.source += &format!("{ty} {val} = {base}{member_op}{member};\n");
                return val;
            }
        }

        String::new()
    }

    fn transpile_struct_expr(&mut self, ctx: &ProgramCtx, s: &StructExprAst) -> String {
        let mut struct_str = String::new();
        struct_str += "{ ";
        for (i, field) in s.fields.iter().enumerate() {
            struct_str += ".";
            struct_str += field.name.as_str();
            struct_str += " = ";
            struct_str += &self.transpile_expr(ctx, &field.rvalue);
            if i != s.fields.len() - 1 {
                struct_str += ", ";
            }
        }
        struct_str += " }";
        struct_str
    }

    fn transpile_array(&mut self, ctx: &ProgramCtx, array: &ArrayExprAst) -> String {
        let mut array_str = String::from_str("{").unwrap();
        for (i, element) in array.elements.iter().enumerate() {
            array_str += self.transpile_expr(ctx, element).as_str();
            if i != array.elements.len() - 1 {
                array_str += ", ";
            }
        }
        array_str += "}";
        array_str
    }

    fn transpile_index(&mut self, ctx: &ProgramCtx, idx: &IndexExprAst) -> String {
        let base_type_id = idx.base.get_type_id(ctx);
        let base_type = ctx
            .type_db
            .get_type(base_type_id)
            .expect("Base type for index expression not found in TypeDB");

        let idx_ty = base_type.get_subtype();
        let (ty, val) = self.gen_temp_expr(ctx, &idx_ty);

        let mut index_str = self.transpile_expr(ctx, &idx.base);
        index_str += "[";
        index_str += self.transpile_expr(ctx, &idx.index).as_str();
        index_str += "]";
        self.source += &self.indent_space();
        self.source += &format!("{ty} {val} = {index_str};\n");
        val
    }

    fn transpile_var(&mut self, _ctx: &ProgramCtx, var: &VarExprAst) -> String {
        if self.raw_mode {
            self.source += &var.name;
            String::new()
        } else {
            var.name.clone()
        }
    }

    fn transpile_call(
        &mut self,
        ctx: &ProgramCtx,
        call: &CallExprAst,
        parent: Option<String>,
    ) -> String {
        let fn_ty = ctx.type_db.get_type(call.fn_id).unwrap();
        let (ty, val) = self.gen_temp_expr(ctx, &fn_ty.get_return_type_id());

        let mut call_str = String::new();
        call_str += CTranspilerPass::mangle_id(fn_ty.get_fullname(&ctx.type_db).as_str()).as_str();
        call_str += "(";
        let args: Vec<_> = call
            .prefix_args
            .iter()
            .chain(call.suffix_args.iter())
            .collect();

        // Check for self and add it as a first argument
        if let Some(parent) = &parent
            && !fn_ty.get_su_args().is_empty()
            && fn_ty.get_su_args()[0].name == "self"
        {
            call_str += "&";
            call_str += parent;
            if !args.is_empty() {
                call_str += ", ";
            }
        }

        // Add the rest of the arguments
        for (i, arg) in args.iter().enumerate() {
            call_str += self.transpile_expr(ctx, arg).as_str();
            if i != args.len() - 1 {
                call_str += ", ";
            }
        }

        call_str += ")";

        if fn_ty.get_return_type_id() == VOID_TYPE.get_id() {
            self.source += &self.indent_space();
            self.source += &call_str;
            self.source += ";\n";
            "".to_string()
        } else {
            self.source += &self.indent_space();
            self.source += &format!("{ty} {val} = {call_str};\n");
            val
        }
    }

    fn transpile_loop(&mut self, ctx: &ProgramCtx, for_expr: &ForExprAst) -> String {
        let condition = self.transpile_expr(ctx, &for_expr.condition);
        self.source += &self.indent_space();
        self.source += "while (";
        self.source += &condition;
        self.source += ") {\n";

        self.indent += 1;
        self.push_frame();
        if let AstExpression::Body(body) = &for_expr.for_body {
            self.transpile_statements(ctx, &body.statements, "");
        } else {
            unreachable!();
        }

        // The condition needs to be evaluated again at the end of the while loop
        let condition_2 = self.transpile_expr(ctx, &for_expr.condition);
        self.source += &self.indent_space();
        self.source += &format!("{condition} = {condition_2};\n");

        self.pop_frame();
        self.indent -= 1;
        self.source += &self.indent_space();
        self.source += "}\n";
        "".to_string() // FIXME: Placeholder as not sure yet how to return expressions from loops
    }

    fn transpile_if(&mut self, ctx: &ProgramCtx, if_expr: &IfExprAst) -> String {
        let if_ty = if_expr.then_expr.get_type_id(ctx);
        let is_void = if_ty == VOID_TYPE.get_id();
        let (ty, val) = self.gen_temp_expr(ctx, &if_ty);

        if !is_void {
            self.source += &self.indent_space();
            self.source += &format!("{ty} {val};\n");
        }

        // Condition
        let condition = self.transpile_expr(ctx, &if_expr.condition).clone();

        self.source += &self.indent_space();
        self.source += "if (";
        self.source += &condition;
        self.source += ") {\n";

        // Then
        self.push_frame();
        let old_tmp = self.curr_return.clone();
        self.curr_return = Some(val.clone());
        if let AstExpression::Body(body) = &if_expr.then_expr {
            self.indent += 1;
            let then_val = self.transpile_statements(ctx, &body.statements, &val);
            if !is_void && then_val.is_some() {
                self.source += &self.indent_space();
                self.source += &format!(
                    "{} = {};\n",
                    self.curr_return.clone().unwrap(),
                    then_val.unwrap()
                );
            }
            self.indent -= 1;
        } else {
            unreachable!();
        }
        self.curr_return = old_tmp;
        self.source += &self.indent_space();
        self.source += "}\n";
        self.pop_frame();

        // Else
        if let Some(else_expr) = &if_expr.else_expr {
            self.push_frame();
            self.source += &self.indent_space();
            self.source += "else {\n";

            let old_tmp = self.curr_return.clone();
            self.curr_return = Some(val.clone());
            if let AstExpression::Body(body) = &else_expr {
                self.indent += 1;
                let else_val = self.transpile_statements(ctx, &body.statements, &val);
                if !is_void && else_val.is_some() {
                    self.source += &self.indent_space();
                    self.source += &format!(
                        "{} = {};\n",
                        self.curr_return.clone().unwrap(),
                        else_val.unwrap()
                    );
                }
                self.indent -= 1;
            } else {
                unreachable!();
            }
            self.curr_return = old_tmp;
            self.source += &self.indent_space();
            self.source += "}\n";
            self.pop_frame();
        }

        if is_void { "".to_string() } else { val }
    }

    fn transpile_ref(&mut self, ctx: &ProgramCtx, r: &RefExprAst) -> String {
        if self.raw_mode {
            self.source += "&";
            self.transpile_expr(ctx, &r.expr);
            String::new()
        } else {
            "&".to_string() + self.transpile_expr(ctx, &r.expr).as_str()
        }
    }

    fn transpile_deref(&mut self, ctx: &ProgramCtx, d: &DerefExprAst) -> String {
        if self.raw_mode {
            self.source += "*";
            self.transpile_expr(ctx, &d.expr);
            String::new()
        } else {
            "*".to_string() + self.transpile_expr(ctx, &d.expr).as_str()
        }
    }

    fn transpile_fn_body(&mut self, ctx: &ProgramCtx, body: &BodyExprAst) {
        let body_ty = body.get_type_id(ctx);
        let is_void = body_ty == VOID_TYPE.get_id();
        let (ty, val) = self.gen_temp_expr(ctx, &body.get_type_id(ctx));

        self.source += "{\n";
        self.indent += 1;
        if !is_void {
            self.source += &self.indent_space();
            self.source += &format!("{ty} {val};\n");
        }

        self.push_frame();
        let last_expr = self.transpile_statements(ctx, &body.statements, &val);

        self.transpile_current_defers(ctx);
        if last_expr.is_none() {
            self.transpile_return(ctx, &ReturnStmtAst { expr: None }, false);
        } else if !is_void {
            self.source += &self.indent_space();
            self.source += &format!("{} = {};\n", val, last_expr.unwrap());
            self.transpile_return(
                ctx,
                &ReturnStmtAst {
                    expr: Some(AstExpression::Var(Box::new(VarExprAst { name: val }))),
                },
                false,
            );
        }
        self.pop_frame();

        self.indent -= 1;
        self.source += &self.indent_space();
        self.source += "}\n\n";
    }

    fn transpile_body(&mut self, ctx: &ProgramCtx, body: &BodyExprAst) -> String {
        let (ty, val) = self.gen_temp_expr(ctx, &body.get_type_id(ctx));
        self.source += &format!("{ty} {val};\n");

        self.source += "{\n";
        self.indent += 1;
        self.transpile_statements(ctx, &body.statements, &val);
        self.indent -= 1;
        self.source += &self.indent_space();
        self.source += "}\n";
        val
    }

    fn transpile_statements(
        &mut self,
        ctx: &ProgramCtx,
        stmts: &[AstStatement],
        val: &str,
    ) -> Option<String> {
        let mut last_expr = None;
        for (i, stmt) in stmts.iter().enumerate() {
            // self.add_indent();
            let old_tmp = self.curr_return.clone();
            self.curr_return = Some(val.to_string());
            if i == stmts.len() - 1 {
                if let AstStatement::StatementExpr(expr) = &stmt {
                    last_expr = Some(self.transpile_expr(ctx, &expr.expr));
                } else {
                    self.transpile_statement(ctx, stmt);
                }
            } else {
                self.transpile_statement(ctx, stmt);
            }
            self.curr_return = old_tmp;
        }

        last_expr
    }

    fn transpile_meta(&mut self, ctx: &ProgramCtx, meta_expr: &MetaDefExprAst) -> String {
        let meta = ctx.get_meta(&meta_expr.name).unwrap();
        let (tmp_ty, tmp_val) = self.gen_temp_expr(ctx, &meta.get_type_id(ctx));
        let meta_str = match &meta.kind {
            MetaFnKind::BinOp(op) => {
                self.transpile_expr(ctx, &meta_expr.args[0])
                    + match op {
                        BinOpKind::Add => " + ",
                        BinOpKind::Minus => " - ",
                        BinOpKind::Mult => " * ",
                        BinOpKind::Div => " / ",
                        BinOpKind::Rem => " % ",
                        BinOpKind::And => " && ",
                        BinOpKind::Or => " || ",
                        BinOpKind::LShr => " << ",
                        BinOpKind::Shl => " >> ",
                    }
                    + self.transpile_expr(ctx, &meta_expr.args[1]).as_str()
            }
            MetaFnKind::CmpOp(op) => {
                self.transpile_expr(ctx, &meta_expr.args[0])
                    + match op {
                        CmpOpKind::Eq => " == ",
                        CmpOpKind::Ne => " != ",
                        CmpOpKind::Less => " < ",
                        CmpOpKind::LessEq => " <= ",
                        CmpOpKind::Greater => " > ",
                        CmpOpKind::GreaterEq => " >= ",
                    }
                    + self.transpile_expr(ctx, &meta_expr.args[1]).as_str()
            }
        };

        if !meta.get_type(ctx).is_void() {
            self.source += &self.indent_space();
            self.source += &format!("{tmp_ty} {tmp_val} = {meta_str};\n");
            return tmp_val;
        }
        String::new()
    }

    fn transpile_current_defers(&mut self, ctx: &ProgramCtx) {
        for defer in self.get_all_current_defers() {
            self.transpile_defer(ctx, &defer);
        }
    }

    fn transpile_defer(&mut self, ctx: &ProgramCtx, defer: &DeferStmtAst) {
        self.transpile_statement(ctx, &defer.stmt);
    }
}
