use std::fs::{self, File};

use crate::{
    ast::*,
    ast_typer::AstTyped,
    errors::{CompilerError, ParserErrorKind},
    lexer::*,
    program_ctx::ProgramCtx,
    types::{AstNamedType, AstTypeId, F32_TYPE, I32_TYPE, VOID_TYPE},
};

pub type ParserResult<T> = Result<T, CompilerError>;
pub type OptionalParserResult<T> = Result<Option<T>, CompilerError>;

pub struct Parser<'a> {
    errors: Vec<CompilerError>,
    debug_scan: bool,
    debug_checks: bool,
    ctx: &'a mut ProgramCtx,
    lexer: Lexer,
    tk_queue: Vec<Token>,

    /// Holds all expressions not used in a line. Used for prefix arguments.
    line_expressions: Vec<AstExpression>,

    /// Meta tags fn defined and not used
    line_meta_def: Vec<MetaExprAst>, // Changed to Box<MetaDefExprAst>

    /// Stack of types for types declared inside other types.
    asttype_stack: Vec<AstTypeId>,

    /// Used when the expression has an unclear type, but can be deduced from a previous statement
    expected_type: Option<AstTypeId>,
}

impl<'a> Parser<'a> {
    pub fn new(ctx: &'a mut ProgramCtx) -> Self {
        Self {
            errors: Vec::new(),
            debug_scan: false,
            debug_checks: false,
            ctx,
            lexer: Lexer::new(),
            tk_queue: Vec::new(),
            line_expressions: Vec::new(),
            line_meta_def: Vec::new(),
            asttype_stack: Vec::new(),
            expected_type: None,
        }
    }

    pub fn has_parsing_errors(&self) -> bool {
        !self.errors.is_empty()
    }

    pub fn get_errors(&self) -> Vec<CompilerError> {
        self.errors.clone()
    }

    pub fn print_parsing_errors(&self, source: &str) {
        for err in &self.errors {
            CompilerError::print_error(err, source); // Use lexer.source
        }
    }

    fn get_tk(&self, offset: usize) -> &Token {
        if offset >= self.tk_queue.len() {
            panic!("end of file found unexpectedly");
        }
        &self.tk_queue[offset]
    }

    fn check_any_tokens(&self, kinds: &[TokenKind], offset: usize) -> bool {
        if offset >= self.tk_queue.len() {
            return false;
        }
        let current_token_kind = self.tk_queue[offset].kind;
        kinds.contains(&current_token_kind)
    }

    fn check_tokens(&self, kinds: &[TokenKind], offset: usize) -> bool {
        if kinds.is_empty() {
            return true;
        }

        if offset + kinds.len() > self.tk_queue.len() {
            return false;
        }

        for (i, expected_kind) in kinds.iter().enumerate() {
            if self.tk_queue[offset + i].kind != *expected_kind {
                return false;
            }
        }
        true
    }

    fn skip_until(&self, kinds: &[TokenKind], offset: &mut usize) {
        while !self.check_any_tokens(kinds, *offset) && *offset < self.tk_queue.len() {
            *offset += 1;
        }
    }

    fn skip_until_single(&self, kind: TokenKind, offset: &mut usize) {
        self.skip_until(&[kind], offset);
    }

    fn skip_all(&self, kinds: &[TokenKind], offset: &mut usize) {
        while self.check_any_tokens(kinds, *offset) && *offset < self.tk_queue.len() {
            *offset += 1;
        }
    }

    pub fn parse_source(&mut self, name: &str, src: &str, print_tokens: bool) -> FileStmtAst {
        self.lexer.set_source(src);
        self.tk_queue.clear(); // Clear any previous tokens

        loop {
            let tk = self.lexer.get_token();
            if print_tokens {
                tk.print_token();
            }
            self.tk_queue.push(tk.clone()); // tk is moved, clone for push_back
            if tk.kind == TokenKind::EoF {
                break;
            }
        }

        let mut offset = 0;
        let statements = self.parse_file_statements(
            &[TokenKind::EoF],
            &[TokenKind::NewLine, TokenKind::EoF],
            &mut offset,
        );

        FileStmtAst {
            pos: FileRange::new(),
            filename: name.to_string(),
            statements,
        }
    }

    pub fn parse_file_statements(
        &mut self,
        end_tokens: &[TokenKind],
        halt_tokens: &[TokenKind],
        offset: &mut usize,
    ) -> Vec<AstStatement> {
        let mut statements: Vec<AstStatement> = Vec::new();
        while !self.check_tokens(end_tokens, *offset) && *offset < self.tk_queue.len() {
            let statement_res = self.parse_file_statement(offset);
            match statement_res {
                Ok(statement) => {
                    statements.push(statement);
                }
                Err(err) => {
                    self.errors.push(err);
                    self.skip_until(halt_tokens, offset); // Skip to next line or EOF on error
                    if self
                        .tk_queue
                        .get(*offset)
                        .is_some_and(|t| t.kind == TokenKind::NewLine)
                    {
                        *offset += 1; // Consume newline if present
                    }
                }
            }
            self.skip_all(&[TokenKind::NewLine], offset);
        }
        statements
    }

    pub fn parse_file_statement(&mut self, offset: &mut usize) -> ParserResult<AstStatement> {
        self.skip_all(&[TokenKind::NewLine], offset);
        let s_range = self.get_tk(*offset).range;

        if let Some(_struct) = self.parse_struct_def(offset)? {
            return Ok(AstStatement::StructDef(Box::new(_struct)));
        }

        if let Some(_enum) = self.parse_enum_def(offset)? {
            return Ok(AstStatement::EnumDef(Box::new(_enum)));
        }

        if let Some(fn_def) = self.parse_fn_def(offset)? {
            // None for parent_struct
            return Ok(AstStatement::FnDef(Box::new(fn_def)));
        }

        if let Some(import) = self.parse_import(offset)? {
            return Ok(AstStatement::Import(Box::new(import)));
        }

        if let Some(mod_def) = self.parse_module(offset)? {
            return Ok(AstStatement::Module(Box::new(mod_def)));
        }

        if let Some(meta_expr) = self.parse_meta_expr(offset)? {
            // Meta functions at file level
            return Ok(AstStatement::StatementExpr(Box::new(StatementExprAst {
                expr: AstExpression::MetaDef(meta_expr),
            })));
        }

        self.skip_until(&[TokenKind::NewLine, TokenKind::EoF], offset);
        Err(CompilerError::undefined_statement(s_range))
    }

    pub fn parse_import(&mut self, offset: &mut usize) -> OptionalParserResult<ImportStmtAst> {
        self.skip_all(&[TokenKind::NewLine], offset);

        if !self.check_tokens(
            &[
                TokenKind::Id,
                TokenKind::Colon,
                TokenKind::Colon,
                TokenKind::Import,
            ],
            *offset,
        ) {
            return Ok(None);
        }

        let import_tk = self.get_tk(*offset).clone();
        *offset += 4;

        if !self.check_tokens(&[TokenKind::String], *offset) {
            panic!("expected string for import declaration");
        }

        let path_tk = self.get_tk(*offset).clone();
        *offset += 1;

        let module_ty = self.ctx.type_db.new_module(
            &ModuleIdAst {
                pos: import_tk.range,
                name: import_tk.value.clone(),
                child: None,
            },
            None,
        );

        // Parse new imported file before keep going
        let mut parser = Parser::new(self.ctx);
        parser.asttype_stack.push(module_ty);
        let file = parser.parse_source(
            &import_tk.value,
            &fs::read_to_string(&path_tk.value).unwrap(),
            false,
        );
        parser.asttype_stack.pop();

        if parser.has_parsing_errors() {
            self.errors.extend_from_slice(&parser.get_errors());
            return Err(CompilerError::import_failed(
                self.get_tk(*offset),
                path_tk.value,
            ));
        }

        Ok(Some(ImportStmtAst {
            id: import_tk.value,
            path: path_tk.value,
            ast: file,
        }))
    }

    pub fn parse_module(&mut self, offset: &mut usize) -> OptionalParserResult<ModuleStmtAst> {
        self.skip_all(&[TokenKind::NewLine], offset);

        let mut tmp_offset = *offset;

        let id = if let Some(id) = self.parse_module_id(&mut tmp_offset)? {
            id
        } else {
            return Ok(None);
        };

        if !self.check_tokens(
            &[TokenKind::Colon, TokenKind::Colon, TokenKind::Module],
            tmp_offset,
        ) {
            return Ok(None);
        }
        tmp_offset += 3; // Consume ' :: module'

        if !self.check_tokens(&[TokenKind::LBrace], tmp_offset) {
            return Err(CompilerError::unexpected_token(
                self.get_tk(*offset),
                &[TokenKind::LBrace],
            ));
        }
        tmp_offset += 1;

        let ty = self.ctx.type_db.new_module(&id, self.asttype_stack.last());
        self.asttype_stack.push(ty);

        let statements =
            self.parse_file_statements(&[TokenKind::RBrace], &[TokenKind::RBrace], &mut tmp_offset);

        if !self.check_tokens(&[TokenKind::RBrace], tmp_offset) {
            return Err(CompilerError::unexpected_token(
                self.get_tk(tmp_offset),
                &[TokenKind::RBrace],
            ));
        }
        tmp_offset += 1;
        *offset = tmp_offset;

        self.asttype_stack.pop();

        Ok(Some(ModuleStmtAst {
            ty,
            id,
            stmts: statements,
        }))
    }

    pub fn parse_module_id(&mut self, offset: &mut usize) -> OptionalParserResult<ModuleIdAst> {
        if !self.check_tokens(&[TokenKind::Id], *offset) {
            return Ok(None);
        }

        let tk_id = self.get_tk(*offset).clone();
        *offset += 1;

        if !self.check_tokens(&[TokenKind::Dot], *offset) {
            return Ok(Some(ModuleIdAst {
                pos: tk_id.range,
                name: tk_id.value,
                child: None,
            }));
        }

        let child = self.parse_module_id(offset)?;
        if child.is_none() {
            panic!("expected module id");
        }

        let child = child.unwrap();
        Ok(Some(ModuleIdAst {
            pos: FileRange::new_merging(&tk_id.range, &child.get_range()),
            name: tk_id.value,
            child: Some(Box::new(child)),
        }))
    }

    pub fn parse_struct_def(&mut self, offset: &mut usize) -> OptionalParserResult<StructDefAst> {
        self.skip_all(&[TokenKind::NewLine], offset);

        if !self.check_tokens(
            &[
                TokenKind::Id,
                TokenKind::Colon,
                TokenKind::Colon,
                TokenKind::Struct,
                TokenKind::LBrace,
            ],
            *offset,
        ) {
            return Ok(None);
        }
        let struct_name_tk = self.get_tk(*offset).clone();
        let struct_name = struct_name_tk.value.clone();
        *offset += 5;

        self.skip_all(&[TokenKind::NewLine], offset);

        let s_id = self
            .ctx
            .type_db
            .new_struct(struct_name.clone(), Vec::new(), None);
        self.asttype_stack.push(s_id);

        let mut methods: Vec<FnDefAst> = Vec::new();
        let mut method_types: Vec<AstTypeId> = Vec::new();
        let mut fields: Vec<ArgDefAst> = Vec::new();
        let mut field_types: Vec<AstNamedType> = Vec::new();

        while !self.check_tokens(&[TokenKind::RBrace], *offset) {
            self.skip_all(&[TokenKind::NewLine], offset); // Skip new lines inside struct
            let s_range = self.get_tk(*offset).range;

            // Try parse method
            // Need to check for `Id :: Fn` to distinguish method from field
            if self.check_tokens(
                &[
                    TokenKind::Id,
                    TokenKind::Colon,
                    TokenKind::Colon,
                    TokenKind::Fn,
                ],
                *offset,
            ) {
                if let Some(fn_def) = self.parse_fn_def(offset)? {
                    method_types.push(fn_def.id);
                    methods.push(fn_def);

                    let s_type_mut = self.ctx.type_db.get_type_mut(s_id).ok_or_else(|| {
                        CompilerError::from_kind(ParserErrorKind::TypeNotFound {
                            type_name: struct_name.clone(),
                        })
                    })?;
                    s_type_mut.set_methods(method_types.clone()); // Update methods on the type
                    self.skip_all(&[TokenKind::NewLine], offset); // Skip new lines inside struct
                    continue;
                } else if let Err(err) = self.parse_fn_def(offset) {
                    self.errors.push(err);
                    self.skip_until_single(TokenKind::RBrace, offset);
                    break;
                }
            }

            // Try parse field
            if let Some(field_def) = self.parse_arg_def(offset)? {
                if !self.check_any_tokens(&[TokenKind::Comma, TokenKind::RBrace], *offset) {
                    self.errors.push(CompilerError::unexpected_token(
                        self.get_tk(*offset),
                        &[TokenKind::Comma],
                    ));
                    self.skip_until_single(TokenKind::RBrace, offset);
                    break;
                }

                if self.check_tokens(&[TokenKind::Comma], *offset) {
                    *offset += 1;
                }

                self.skip_all(&[TokenKind::NewLine], offset);

                field_types.push(AstNamedType {
                    name: field_def.name.clone(),
                    id: field_def.type_id,
                    is_varadic: false,
                });
                fields.push(field_def);

                let s_type_mut = self.ctx.type_db.get_type_mut(s_id).ok_or_else(|| {
                    CompilerError::from_kind(ParserErrorKind::TypeNotFound {
                        type_name: struct_name.clone(),
                    })
                })?;
                s_type_mut.set_fields(field_types.clone()); // Update fields on the type
                self.skip_all(&[TokenKind::NewLine], offset); // Skip new lines inside struct
                continue;
            } else if let Err(err) = self.parse_arg_def(offset) {
                self.errors.push(err);
                self.skip_until_single(TokenKind::RBrace, offset);
                break;
            }

            // If neither method nor field parsed, and not RBrace, then it's an error
            self.errors
                .push(CompilerError::undefined_statement(s_range));
            self.skip_until_single(TokenKind::RBrace, offset);
            break;
        }

        if !self.check_tokens(&[TokenKind::RBrace], *offset) {
            return Err(CompilerError::from_token(
                self.get_tk(*offset),
                ParserErrorKind::UnexpectedTokenKind {
                    expected_kind: vec![TokenKind::RBrace],
                    found_kind: self.get_tk(*offset).kind,
                },
            ));
        }
        let rbrace_tk = self.get_tk(*offset).clone();
        *offset += 1; // Consume RBrace

        self.asttype_stack.pop(); // Pop current struct from stack

        if let Some(parent_id) = self.asttype_stack.last().cloned() {
            let s_type_mut = self.ctx.type_db.get_type_mut(s_id).ok_or_else(|| {
                CompilerError::from_kind(ParserErrorKind::TypeNotFound {
                    type_name: struct_name,
                })
            })?;
            s_type_mut.set_parent_id(parent_id);
        }
        let s = StructDefAst {
            pos: FileRange::new_merging(&struct_name_tk.range, &rbrace_tk.range),
            type_id: s_id,
            fields,
            methods,
        };
        // self.ctx.define_struct(struct_name, s);
        Ok(Some(s))
    }

    pub fn parse_meta_expr(
        &mut self,
        offset: &mut usize,
    ) -> OptionalParserResult<Box<MetaExprAst>> {
        self.skip_all(&[TokenKind::NewLine], offset);

        if !self.check_tokens(&[TokenKind::Meta], *offset) {
            return Ok(None);
        }

        let meta_tk = self.get_tk(*offset).clone();
        *offset += 1;

        let meta_fn = self.ctx.get_meta(&meta_tk.value);
        let meta_fn_ref = meta_fn.ok_or_else(|| {
            CompilerError::from_token(
                self.get_tk(*offset),
                ParserErrorKind::UndefinedMetaFn {
                    name: meta_tk.value.clone(),
                },
            )
        })?;

        let mut args: Vec<AstExpression> = Vec::new();
        for _i in 0..meta_fn_ref.get_arg_size() {
            let expr = self.parse_expr(offset);
            if let Err(err) = expr {
                self.skip_until_single(TokenKind::NewLine, offset);
                return Err(err);
            }

            // If parse_expr returns Ok(None), it means no expression was found, which might be an error here.
            args.push(
                expr.unwrap()
                    .ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?,
            );
        }

        Ok(Some(Box::new(MetaExprAst {
            pos: FileRange::new_merging(&meta_tk.range, &args.last().unwrap().get_range()),
            name: meta_tk.value,
            args,
        })))
    }

    pub fn parse_fn_def(&mut self, offset: &mut usize) -> OptionalParserResult<FnDefAst> {
        self.skip_all(&[TokenKind::NewLine], offset);

        let mut is_external = false;
        if self.check_tokens(&[TokenKind::Extern], *offset) {
            is_external = true;
            *offset += 1;
        }
        self.skip_all(&[TokenKind::NewLine], offset);

        if !self.check_tokens(
            &[
                TokenKind::Id,
                TokenKind::Colon,
                TokenKind::Colon,
                TokenKind::Fn,
            ],
            *offset,
        ) {
            return Ok(None);
        }

        let fn_id_tk = self.get_tk(*offset).clone();
        let fn_name = fn_id_tk.value;
        *offset += 4; // Consume id :: fn

        let parent_ty = self.asttype_stack.last().cloned();
        let mut header = self.parse_fn_header(fn_name.clone(), parent_ty, offset)?;
        header.is_external = is_external;

        self.ctx.push_local();
        header.prefix_args.iter().for_each(|prefix_arg| {
            self.ctx
                .def_var(prefix_arg.name.clone(), prefix_arg.type_id);
        });

        header.suffix_args.iter().for_each(|suffix_arg| {
            self.ctx
                .def_var(suffix_arg.name.clone(), suffix_arg.type_id);
        });

        // Register the fn early for recursion
        let fn_ty_id = self.ctx.define_fn(header.name.clone(), &header, parent_ty);

        let mut body: Option<Box<BodyExprAst>> = None;
        if !is_external {
            self.expected_type = Some(header.ret_type);
            let body_r = self.parse_body(offset)?;
            match &body_r {
                Some(body) => {
                    let body_ty = body.get_type(self.ctx);
                    if body_ty.get_id() != header.ret_type {
                        return Err(CompilerError::incorrect_return_type(
                            body.statements.last().unwrap().get_range(),
                            body_ty,
                            self.ctx.type_db.get_type(header.ret_type).unwrap(),
                        ));
                    }
                }
                None => {
                    return Err(CompilerError::from_token(
                        self.get_tk(*offset),
                        ParserErrorKind::FnMissingBody,
                    ));
                }
            }
            self.expected_type = None;

            body = body_r;
        }
        self.ctx.pop_local();

        Ok(Some(FnDefAst {
            pos: if let Some(body) = &body {
                FileRange::new_merging(&header.pos, &body.pos)
            } else {
                header.pos
            },
            id: fn_ty_id,
            fn_header: header,
            body: body.map(AstExpression::Body),
        }))
    }

    pub fn parse_type(&mut self, offset: &mut usize) -> ParserResult<AstTypeId> {
        let mut is_ref = false;
        let mut tmp_offset = *offset;
        if self.check_tokens(&[TokenKind::Pointy], tmp_offset) {
            is_ref = true;
            tmp_offset += 1;
        }

        if !self.check_tokens(&[TokenKind::Id], tmp_offset) {
            return Err(CompilerError::from_token(
                self.get_tk(tmp_offset),
                ParserErrorKind::UnexpectedTokenKind {
                    expected_kind: vec![self.get_tk(tmp_offset).kind],
                    found_kind: TokenKind::Id,
                },
            ));
        }

        let id_tk = self.get_tk(tmp_offset);
        let id = id_tk.value.clone();
        tmp_offset += 1;

        let type_info_opt = self.ctx.type_db.get_type_by_name(&id);
        let type_info = type_info_opt.ok_or_else(|| {
            CompilerError::from_kind(ParserErrorKind::TypeNotFound {
                type_name: id.clone(),
            })
        })?;
        *offset = tmp_offset;

        // TODO: This will need to be redone to handle types defined inside types
        if is_ref {
            Ok(self.ctx.type_db.new_ref(type_info.get_id()))
        } else {
            Ok(type_info.get_id())
        }
    }

    pub fn parse_enum_def(&mut self, offset: &mut usize) -> OptionalParserResult<EnumDefAst> {
        self.skip_all(&[TokenKind::NewLine], offset);

        if !self.check_tokens(
            &[
                TokenKind::Id,
                TokenKind::Colon,
                TokenKind::Colon,
                TokenKind::Enum,
                TokenKind::LBrace,
            ],
            *offset,
        ) {
            return Ok(None);
        }

        let enum_name_tk = self.get_tk(*offset).clone();
        let enum_name = enum_name_tk.value.clone();
        *offset += 5;

        self.skip_all(&[TokenKind::NewLine], offset);

        let enum_id = self
            .ctx
            .type_db
            .new_enum(enum_name.clone(), Vec::new(), None);
        self.asttype_stack.push(enum_id);

        let mut field_types: Vec<AstNamedType> = Vec::new();
        let mut field_stmts: Vec<AstStatement> = Vec::new();

        while !self.check_tokens(&[TokenKind::RBrace], *offset) {
            self.skip_all(&[TokenKind::NewLine], offset);

            let current_tk = self.get_tk(*offset).clone();

            if self.check_tokens(&[TokenKind::Id, TokenKind::Comma], *offset)
                || (self.check_tokens(&[TokenKind::Id], *offset)
                    && self.check_tokens(&[TokenKind::RBrace], *offset + 1))
            {
                // A enum variant without struct (unit struct)
                let field_tk = self.get_tk(*offset);
                let field_name = field_tk.value.clone();
                *offset += 1;

                let s_type_id = self.ctx.type_db.new_struct(
                    field_name.clone(),
                    Vec::new(), // No fields
                    Some(enum_id),
                );

                // Create a dummy unit struct Ast
                let s = Box::new(StructDefAst {
                    pos: FileRange::new(),
                    type_id: s_type_id,
                    fields: Vec::new(),
                    methods: Vec::new(),
                });
                // self.ctx.define_struct(field_name.clone(), s.as_ref()); // Define it for lookup

                field_types.push(AstNamedType {
                    name: field_name,
                    id: s_type_id,
                    is_varadic: false,
                });
                field_stmts.push(AstStatement::StructDef(s));
            } else if let Ok(stmt_option) = self.parse_file_statement(offset) {
                // Convert a declared struct into another variant for the enum
                if let AstStatement::StructDef(s_box) = stmt_option {
                    let stmt_type_id = s_box.type_id;
                    let stmt_type = self.ctx.type_db.get_type(stmt_type_id).unwrap();
                    field_types.push(AstNamedType {
                        name: stmt_type.get_name().to_string(),
                        id: stmt_type_id,
                        is_varadic: false,
                    });
                    field_stmts.push(AstStatement::StructDef(s_box));
                } else {
                    return Err(CompilerError::from_token(
                        self.get_tk(*offset),
                        ParserErrorKind::NonStructEnumVariant,
                    ));
                }
            } else {
                return Err(CompilerError::from_token(
                    &current_tk,
                    ParserErrorKind::UnsupportedEnumVariant,
                ));
            }

            if !self.check_tokens(&[TokenKind::RBrace], *offset) {
                // If not RBrace, expect comma
                if !self.check_tokens(&[TokenKind::Comma], *offset) {
                    return Err(CompilerError::from_token(
                        self.get_tk(*offset),
                        ParserErrorKind::UnexpectedTokenKind {
                            expected_kind: vec![TokenKind::Comma, TokenKind::RBrace],
                            found_kind: self.get_tk(*offset).kind,
                        },
                    ));
                }
                *offset += 1; // Consume comma
            }

            self.skip_all(&[TokenKind::NewLine], offset);
        }

        if !self.check_tokens(&[TokenKind::RBrace], *offset) {
            return Err(CompilerError::from_token(
                self.get_tk(*offset),
                ParserErrorKind::UnexpectedTokenKind {
                    expected_kind: vec![TokenKind::RBrace],
                    found_kind: self.get_tk(*offset).kind,
                },
            ));
        }
        let rbrace_tk = self.get_tk(*offset).clone();
        *offset += 1; // Consume RBrace

        self.asttype_stack.pop();

        let enum_type_mut = self.ctx.type_db.get_type_mut(enum_id).unwrap();
        enum_type_mut.set_fields(field_types);
        if let Some(parent_id) = self.asttype_stack.last().cloned() {
            enum_type_mut.set_parent_id(parent_id);
        }

        let e = EnumDefAst {
            pos: FileRange::new_merging(&enum_name_tk.range, &rbrace_tk.range),
            type_id: enum_id,
            values: field_stmts,
        };
        // self.ctx
        //     .define_enum(enum_type_mut.get_fullname(), e.as_ref());
        Ok(Some(e))
    }

    // consume_expressions
    pub fn consume_expressions(
        &mut self,
        offset: &mut usize,
        halt_tokens: &[TokenKind],
        consume_halt: bool,
    ) -> OptionalParserResult<AstExpression> {
        self.line_expressions.clear(); // Clear for this call

        while !self.check_any_tokens(halt_tokens, *offset) {
            let expr_res = self.parse_expr(offset)?; // Use ? to propagate parsing errors
            match expr_res {
                Some(expr) => {
                    self.line_expressions.push(expr);
                }
                None => {
                    break;
                }
            }
        }

        if consume_halt && self.check_any_tokens(halt_tokens, *offset) {
            *offset += 1;
        }

        if self.line_expressions.is_empty() {
            return Ok(Some(AstExpression::NoOp(Box::new(NoOpAst {}))));
        }

        let last_expr = self.line_expressions.pop().unwrap(); // Safe due to check above
        self.line_expressions.clear(); // Ensure it's clear after use
        Ok(Some(last_expr))
    }

    // parse_var_assign
    pub fn parse_var_assign(
        &mut self,
        offset: &mut usize,
        halt_tokens: &[TokenKind],
    ) -> OptionalParserResult<Box<VarAssignStmtAst>> {
        let mut tmp_offset = *offset;
        let lvalue_res = self.parse_expr(&mut tmp_offset)?;

        let lvalue = match lvalue_res {
            Some(l) => l,
            None => return Ok(None),
        };

        if !self.check_tokens(&[TokenKind::Id], tmp_offset) || self.get_tk(tmp_offset).value != "="
        {
            return Ok(None); // Not a var assignment
        }
        tmp_offset += 1;

        let rvalue_res = self.consume_expressions(&mut tmp_offset, halt_tokens, true)?;
        let rvalue = match rvalue_res {
            Some(r) => r,
            None => {
                return Err(CompilerError::expression_expected(self.get_tk(tmp_offset)));
            }
        };

        *offset = tmp_offset; // Update outer offset
        Ok(Some(Box::new(VarAssignStmtAst {
            pos: FileRange::new_merging(&lvalue.get_range(), &rvalue.get_range()),
            lvalue,
            rvalue,
        })))
    }

    // parse_var_decl
    pub fn parse_var_decl(&mut self, offset: &mut usize) -> OptionalParserResult<VarDefStmtAst> {
        if !self.check_tokens(&[TokenKind::Id, TokenKind::Colon, TokenKind::Id], *offset)
            || self.get_tk(*offset + 2).value != "="
        {
            return Ok(None); // Not a var decl
        }
        let id_tk = self.get_tk(*offset).clone();
        let id = id_tk.value.clone();
        *offset += 3; // Consume `id :=`

        let expr_res = self.consume_expressions(offset, &[TokenKind::NewLine], true)?;
        let expr = match expr_res {
            Some(e) => e,
            None => return Ok(None),
        };

        let ty_id = expr.get_type_id(self.ctx);

        let def_var = VarDefStmtAst {
            pos: FileRange::new_merging(&id_tk.range, &expr.get_range()),
            name: id.clone(),
            type_id: ty_id,
            assignment: Some(expr),
        };
        self.ctx.def_var(def_var.name.clone(), def_var.type_id);
        Ok(Some(def_var))
    }

    pub fn parse_return(&mut self, offset: &mut usize) -> OptionalParserResult<Box<ReturnStmtAst>> {
        if !self.check_tokens(&[TokenKind::Return], *offset) {
            return Ok(None);
        }
        let return_tk = self.get_tk(*offset).clone();
        *offset += 1; // Consume 'return'

        if self.check_tokens(&[TokenKind::NewLine], *offset) {
            *offset += 1; // Consume NewLine
            return Ok(Some(Box::new(ReturnStmtAst {
                pos: return_tk.range,
                expr: None,
            })));
        }

        let expr = self.consume_expressions(offset, &[TokenKind::NewLine], true)?;

        Ok(Some(Box::new(ReturnStmtAst {
            pos: FileRange::new_merging(&return_tk.range, &expr.as_ref().unwrap().get_range()),
            expr,
        })))
    }

    pub fn parse_defer(&mut self, offset: &mut usize) -> OptionalParserResult<Box<DeferStmtAst>> {
        if !self.check_tokens(&[TokenKind::Defer], *offset) {
            return Ok(None);
        }
        let defer_tk = self.get_tk(*offset).clone();
        *offset += 1; // Consume 'defer'

        let stmt = self.parse_statement(offset)?;

        Ok(Some(Box::new(DeferStmtAst {
            pos: FileRange::new_merging(&defer_tk.range, &stmt.get_range()),
            stmt,
        })))
    }

    // parse_statement
    pub fn parse_statement(&mut self, offset: &mut usize) -> ParserResult<AstStatement> {
        self.skip_all(&[TokenKind::NewLine], offset); // Ensure leading newlines are skipped

        if let Some(defer) = self.parse_defer(offset)? {
            return Ok(AstStatement::Defer(defer));
        }

        if let Some(ret_stmt) = self.parse_return(offset)? {
            return Ok(AstStatement::ReturnStmt(ret_stmt));
        }

        if let Some(var_decl_stmt) = self.parse_var_decl(offset)? {
            return Ok(AstStatement::VarDefStmt(Box::new(var_decl_stmt)));
        }

        if let Some(var_assign_stmt) = self.parse_var_assign(offset, &[TokenKind::NewLine])? {
            return Ok(AstStatement::VarAssignStmt(var_assign_stmt));
        }

        // If nothing else, try to parse an expression as a statement
        let last_expr_res =
            self.consume_expressions(offset, &[TokenKind::NewLine, TokenKind::RBrace], false)?;
        let last_expr = match last_expr_res {
            Some(expr) => expr,
            None => {
                return Err(CompilerError::expression_expected(self.get_tk(*offset)));
            }
        };

        Ok(AstStatement::StatementExpr(Box::new(StatementExprAst {
            expr: last_expr,
        })))
    }

    pub fn parse_expr(&mut self, offset: &mut usize) -> OptionalParserResult<AstExpression> {
        let inner_expr_res = self.parse_inner_expr(offset)?;
        let mut inner_expr = match inner_expr_res {
            Some(e) => e,
            None => return Ok(None), // Nothing found for inner_expr
        };

        // Check if indexing expression (loop for multiple accesses)
        loop {
            let (_expr, res) = self.parse_index_expr(offset, inner_expr);
            inner_expr = _expr;
            if !res {
                break;
            }
        }

        // Check if accessing expression (loop for multiple accesses)
        loop {
            let (_expr, res) = self.parse_member_access_expr(offset, inner_expr);
            inner_expr = _expr;
            if !res {
                break;
            }
        }

        Ok(Some(inner_expr))
    }

    pub fn parse_inner_expr(&mut self, offset: &mut usize) -> OptionalParserResult<AstExpression> {
        if self.check_tokens(&[TokenKind::Id], *offset) {
            let identifier_tk = self.get_tk(*offset).clone();
            let identifier = identifier_tk.value.clone();

            if self
                .ctx
                .type_db
                .get_type_by_name(&identifier)
                .is_some_and(|x| x.is_enum())
            {
                if let Some(enum_expr) = self.parse_enum_expr(offset)? {
                    return Ok(Some(AstExpression::Enum(Box::new(enum_expr))));
                }
            }
        }

        // Call expr
        if self.check_tokens(&[TokenKind::Id], *offset) {
            let identifier_tk = self.get_tk(*offset).clone();
            let identifier = identifier_tk.value;
            if !self.ctx.type_db.get_fns_by_name(&identifier).is_empty() {
                if let Some(call) = self.parse_call_expr(offset, None)? {
                    // None for parent
                    return Ok(Some(AstExpression::Call(Box::new(call))));
                }
            }
        }

        // Struct expr (instantiation)
        if let Some(struct_expr) = self.parse_struct_expr(offset)? {
            return Ok(Some(AstExpression::Struct(struct_expr)));
        }

        // Module expr
        if let Some(module_expr) = self.parse_module_access(offset)? {
            return Ok(Some(AstExpression::ModuleAccess(Box::new(module_expr))));
        }

        // Type expr
        if let Some(ty) = self.parse_type_expr(offset)? {
            return Ok(Some(AstExpression::Type(Box::new(ty))));
        }

        // Variable
        if self.check_tokens(&[TokenKind::Id], *offset) {
            let identifier_tk = self.get_tk(*offset).clone();
            let identifier = identifier_tk.value.clone();
            if self.ctx.get_var(&identifier).is_none() {
                return Err(CompilerError::undefined_id(identifier_tk.range, identifier));
            }
            *offset += 1; // Consume Id
            return Ok(Some(AstExpression::Var(Box::new(VarExprAst {
                pos: identifier_tk.range,
                name: identifier,
            }))));
        }

        if let Some(body) = self.parse_body(offset)? {
            return Ok(Some(AstExpression::Body(body)));
        }

        if self.check_tokens(&[TokenKind::LPar], *offset) {
            let lpar_tk = self.get_tk(*offset).clone();
            *offset += 1;
            let expr_res = self.consume_expressions(offset, &[TokenKind::RPar], true)?;
            let expr =
                expr_res.ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;
            let rpar_tk = self.get_tk(*offset - 1).clone();
            return Ok(Some(AstExpression::Group(Box::new(GroupExprAst {
                pos: FileRange::new_merging(&lpar_tk.range, &rpar_tk.range),
                expr,
            }))));
        }

        // Array expression
        if let Some(array_expr) = self.parse_array_expr(offset)? {
            return Ok(Some(AstExpression::Array(Box::new(array_expr))));
        }

        // Match expression
        if let Some(match_expr) = self.parse_single_match_expr(offset)? {
            return Ok(Some(AstExpression::SingleMatch(Box::new(match_expr))));
        }

        // If expression
        if let Some(if_expr) = self.parse_if_expr(offset)? {
            return Ok(Some(AstExpression::If(Box::new(if_expr))));
        }

        // Loop (for) expression
        if let Some(for_expr) = self.parse_loop_expr(offset)? {
            return Ok(Some(AstExpression::For(Box::new(for_expr))));
        }

        // Meta expression (if not handled as a statement)
        if self.check_tokens(&[TokenKind::Meta], *offset) {
            if let Some(meta_expr) = self.parse_meta_expr(offset)? {
                return Ok(Some(AstExpression::MetaDef(meta_expr)));
            }
        }

        // Literals
        if self.check_tokens(&[TokenKind::String], *offset) {
            let str_tk = self.get_tk(*offset).clone();
            *offset += 1;
            return Ok(Some(AstExpression::String(Box::new(StringExprAst {
                pos: str_tk.range,
                value: str_tk.value,
            }))));
        }
        if self.check_tokens(&[TokenKind::Bool], *offset) {
            let bool_tk = self.get_tk(*offset).clone();
            *offset += 1;
            return Ok(Some(AstExpression::Bool(Box::new(BoolExprAst {
                pos: bool_tk.range,
                value: bool_tk.value == "true",
            }))));
        }
        if self.check_tokens(&[TokenKind::Int], *offset) {
            let int_tk = self.get_tk(*offset).clone();
            *offset += 1;
            return Ok(Some(AstExpression::Int(Box::new(IntExprAst {
                pos: int_tk.range,
                value: int_tk.value.parse().unwrap_or_default(),
                id: self.expected_type.unwrap_or(I32_TYPE.get_id()),
            }))));
        }
        if self.check_tokens(&[TokenKind::Float], *offset) {
            let float_tk = self.get_tk(*offset).clone();
            *offset += 1;
            return Ok(Some(AstExpression::Float(Box::new(FloatExprAst {
                pos: float_tk.range,
                value: float_tk.value.parse().unwrap_or_default(),
                id: self.expected_type.unwrap_or(F32_TYPE.get_id()),
            }))));
        }

        // Reference (&) and Dereference (^)
        if self.check_tokens(&[TokenKind::Amper], *offset) {
            let amper_tk = self.get_tk(*offset).clone();
            *offset += 1;
            let expr_res = self.parse_expr(offset)?;
            let expr =
                expr_res.ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;
            return Ok(Some(AstExpression::Ref(Box::new(RefExprAst {
                pos: FileRange::new_merging(&amper_tk.range, &expr.get_range()),
                expr,
            }))));
        }
        if self.check_tokens(&[TokenKind::Pointy], *offset) {
            let pointy_tk = self.get_tk(*offset).clone();
            *offset += 1;
            let expr_res = self.parse_expr(offset)?;
            let expr =
                expr_res.ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;
            return Ok(Some(AstExpression::Deref(Box::new(DerefExprAst {
                pos: FileRange::new_merging(&pointy_tk.range, &expr.get_range()),
                expr,
            }))));
        }

        Ok(None) // No matching expression production found
    }

    pub fn parse_type_expr(&mut self, offset: &mut usize) -> OptionalParserResult<TypeExprAst> {
        let s_tk = self.get_tk(*offset).clone();
        let ty = self.parse_type(offset);
        let e_tk = self.get_tk(*offset).clone();
        if let Ok(ty) = ty {
            Ok(Some(TypeExprAst {
                pos: FileRange::new_merging(&s_tk.range, &e_tk.range),
                id: ty,
            }))
        } else {
            Ok(None)
        }
    }

    pub fn parse_module_access(
        &mut self,
        offset: &mut usize,
    ) -> OptionalParserResult<ModuleAccessExprAst> {
        if !self.check_tokens(&[TokenKind::Id, TokenKind::Dot], *offset) {
            return Ok(None);
        }

        let mut tmp_offset = *offset;
        let module_tk = self.get_tk(tmp_offset).clone();
        tmp_offset += 2;

        let module_id = self.ctx.type_db.get_id_by_name(&module_tk.value);
        if module_id.is_none() {
            return Ok(None);
        }

        {
            let module_ty = self.ctx.type_db.get_type(module_id.unwrap());
            if module_ty.is_none_or(|x| !x.is_module()) {
                return Ok(None);
            }
        }

        let expr = self.parse_expr(&mut tmp_offset)?;
        if expr.is_none() {
            panic!("expression expected in a module");
        }

        *offset = tmp_offset;

        Ok(Some(ModuleAccessExprAst {
            pos: FileRange::new_merging(&module_tk.range, &expr.as_ref().unwrap().get_range()),
            ty: module_id.unwrap(),
            expr: expr.unwrap(),
        }))
    }

    // parse_enum_expr
    pub fn parse_enum_expr(&mut self, offset: &mut usize) -> OptionalParserResult<EnumExprAst> {
        if !self.check_tokens(&[TokenKind::Id, TokenKind::Dot, TokenKind::Id], *offset) {
            return Ok(None);
        }
        let enum_name_tk = self.get_tk(*offset).clone();
        let enum_name = enum_name_tk.value.clone();
        let enum_member_name_tk = self.get_tk(*offset + 2);
        let enum_member_name = enum_member_name_tk.value.clone();

        let enum_type_id = self
            .ctx
            .type_db
            .get_id_by_name(&enum_name)
            .ok_or_else(|| CompilerError::type_not_found(&enum_name_tk, enum_name.clone()))?;
        let enum_type = self.ctx.type_db.get_type(enum_type_id).unwrap().clone();
        if !enum_type.is_enum() {
            return Err(CompilerError::expected_type_enum(&enum_name_tk, &enum_type));
        }

        let enum_member_named_type = enum_type
            .get_field_by_name(&enum_member_name)
            .unwrap_or_else(|| panic!("no field '{enum_member_name}' found on type '{enum_name}'"));
        let enum_member_type = self.ctx.type_db.get_type(enum_member_named_type.id);

        *offset += 3; // Consume `Id . Id`

        let mut vars: Vec<StructFieldAssign> = Vec::new();
        if !enum_member_type.unwrap().is_unit() {
            let struct_assignments_res = self.parse_struct_assignments(offset)?;
            let _vars = struct_assignments_res
                .ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;
            vars = _vars;
        }
        let e_tk = self.get_tk(*offset).clone();

        let struct_expr = Box::new(StructExprAst {
            pos: FileRange::new(),
            type_id: enum_member_named_type.id, // ID of the struct representing the variant
            fields: vars,
        });
        Ok(Some(EnumExprAst {
            pos: FileRange::new_merging(&enum_name_tk.range, &e_tk.range),
            enum_type: enum_type_id,
            struct_expr,
        }))
    }

    // parser_member_access_expr (renamed to parse_member_access_expr for Rust conventions)
    pub fn parse_member_access_expr(
        &mut self,
        offset: &mut usize,
        base_expr: AstExpression,
    ) -> (AstExpression, bool) {
        if !self.check_tokens(&[TokenKind::Dot, TokenKind::Id], *offset) {
            return (base_expr, false);
        }

        let id_tk = self.get_tk(*offset + 1).clone();
        let id = id_tk.value.clone();
        let mut base_ty_ref = base_expr.get_type(self.ctx).clone();

        // Dereference if it's a reference type
        if base_ty_ref.is_ref() {
            base_ty_ref = self
                .ctx
                .type_db
                .get_type(base_ty_ref.get_subtype())
                .unwrap()
                .clone();
        }

        if base_ty_ref.get_field_by_name(&id).is_some() {
            // Member is a field
            let field_expr = Box::new(VarExprAst {
                pos: id_tk.range,
                name: id,
            });
            *offset += 2; // Consume . and Id
            (
                AstExpression::MemberAccessor(Box::new(MemberAccesorExprAst {
                    pos: FileRange::new_merging(
                        &base_expr.get_range(),
                        &self.get_tk(*offset).range,
                    ),
                    base: base_expr,
                    field: Some(field_expr),
                    method: None,
                })),
                true,
            )
        } else if base_ty_ref
            .get_method_by_name(&id, &self.ctx.type_db)
            .is_some()
        {
            // Member is a method
            *offset += 1; // Consume . (Id will be consumed by parse_call_expr)
            let call_res = self
                .parse_call_expr(offset, Some(base_ty_ref.get_id()))
                .unwrap();
            let call = call_res
                .ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))
                .unwrap();
            (
                AstExpression::MemberAccessor(Box::new(MemberAccesorExprAst {
                    pos: FileRange::new_merging(
                        &base_expr.get_range(),
                        &self.get_tk(*offset).range,
                    ),
                    base: base_expr,
                    field: None,
                    method: Some(Box::new(call)),
                })),
                true,
            )
        } else {
            self.errors
                .push(CompilerError::member_not_found(&id_tk, &base_ty_ref, id));
            *offset += 2; // Skip . and id
            return (base_expr, false);
        }
    }

    // parse_single_match_expr
    pub fn parse_single_match_expr(
        &mut self,
        offset: &mut usize,
    ) -> OptionalParserResult<SingleMatchExprAst> {
        let mut tmp_offset = *offset;
        if !self.check_tokens(&[TokenKind::Match], tmp_offset) {
            return Ok(None);
        }
        let match_tk = self.get_tk(*offset).clone();
        tmp_offset += 1;

        let match_expr_res = self.parse_expr(&mut tmp_offset)?;
        let match_expr = match_expr_res
            .ok_or_else(|| CompilerError::expression_expected(self.get_tk(tmp_offset)))?;

        if !self.check_tokens(&[TokenKind::Colon], tmp_offset) {
            return Err(CompilerError::unexpected_token(
                self.get_tk(tmp_offset),
                &[TokenKind::Colon],
            ));
        }
        tmp_offset += 1;

        if !self.check_tokens(&[TokenKind::Id, TokenKind::Dot, TokenKind::Id], tmp_offset) {
            return Err(CompilerError::unexpected_token(
                self.get_tk(tmp_offset),
                &[TokenKind::Id, TokenKind::Dot, TokenKind::Id],
            ));
        }
        let enum_name_tk = self.get_tk(tmp_offset);
        let enum_name = enum_name_tk.value.clone();
        let enum_member_name_tk = self.get_tk(tmp_offset + 2);
        let enum_member_name = enum_member_name_tk.value.clone();
        tmp_offset += 3;

        let mut casted_var_name = String::new();
        if self.check_tokens(&[TokenKind::Id], tmp_offset) {
            casted_var_name = self.get_tk(tmp_offset).value.clone();
            tmp_offset += 1;
        }

        let enum_type_opt = self.ctx.type_db.get_type_by_name(&enum_name);
        let enum_type =
            enum_type_opt.ok_or_else(|| CompilerError::type_not_found(enum_name_tk, enum_name))?;

        let enum_member_named_type =
            enum_type
                .get_field_by_name(&enum_member_name)
                .ok_or_else(|| {
                    CompilerError::undefined_enum_member(
                        enum_member_name_tk,
                        enum_type.get_fullname(&self.ctx.type_db),
                        enum_member_name.clone(),
                    )
                })?;

        let casted_var = Box::new(VarDefStmtAst {
            pos: FileRange::new(),
            name: casted_var_name.clone(),
            type_id: enum_member_named_type.id,
            assignment: None,
        });

        self.ctx
            .def_var(casted_var_name.clone(), casted_var.type_id);

        let then_expr_res = self.parse_expr(&mut tmp_offset)?;
        let then_expr = then_expr_res
            .ok_or_else(|| CompilerError::expression_expected(self.get_tk(tmp_offset)))?;

        *offset = tmp_offset; // Update outer offset

        let match_ast = SingleMatchExprAst {
            pos: FileRange::new_merging(&match_tk.range, &self.get_tk(*offset).range),
            enum_expr: match_expr,
            casted_enum_var: casted_var,
            then_expr,
        };

        Ok(Some(match_ast))
    }

    pub fn parse_if_expr(&mut self, offset: &mut usize) -> OptionalParserResult<IfExprAst> {
        if !self.check_tokens(&[TokenKind::If], *offset) {
            return Ok(None);
        }
        let if_tk = self.get_tk(*offset).clone();
        *offset += 1; // Consume 'if'

        let condition_res =
            self.consume_expressions(offset, &[TokenKind::LBrace, TokenKind::NewLine], false)?;
        let condition = condition_res
            .ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;

        let then_expr_res = self.parse_expr(offset)?;
        let then_expr = then_expr_res
            .ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;

        let mut else_expr: Option<AstExpression> = None;
        if self.check_tokens(&[TokenKind::Else], *offset) {
            *offset += 1; // Consume 'else'
            let _else_expr_res = self.parse_expr(offset)?;
            if let Some(e) = _else_expr_res {
                else_expr = Some(e);
            }
        }

        Ok(Some(IfExprAst {
            pos: FileRange::new_merging(&if_tk.range, &self.get_tk(*offset).range),
            condition,
            then_expr,
            else_expr,
        }))
    }

    // parse_loop_expr (for expression in C++)
    pub fn parse_loop_expr(&mut self, offset: &mut usize) -> OptionalParserResult<ForExprAst> {
        if !self.check_tokens(&[TokenKind::Loop], *offset) {
            return Ok(None);
        }
        let loop_tk = self.get_tk(*offset).clone();
        *offset += 1; // Consume 'loop'

        let for_cond_expr_res =
            self.consume_expressions(offset, &[TokenKind::LBrace, TokenKind::NewLine], false)?;
        let for_cond_expr = for_cond_expr_res
            .ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;

        let for_body_res = self.parse_expr(offset)?;
        let for_body =
            for_body_res.ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;

        Ok(Some(ForExprAst {
            pos: FileRange::new_merging(&loop_tk.range, &self.get_tk(*offset).range),
            condition: for_cond_expr,
            for_body,
        }))
    }

    pub fn parse_call_expr(
        &mut self,
        offset: &mut usize,
        parent: Option<AstTypeId>,
    ) -> OptionalParserResult<CallExprAst> {
        let identifier_tk = self.get_tk(*offset).clone();
        let fns = self.ctx.type_db.get_fns_by_name(&identifier_tk.value);

        for fn_id in &fns {
            let fn_ty = self.ctx.type_db.get_type(*fn_id).unwrap().clone();
            if let Some(parent_id) = parent {
                match fn_ty.get_parent_id() {
                    Some(fn_parent_id) => {
                        if parent_id != fn_parent_id {
                            continue;
                        }
                    }
                    None => continue,
                }
            }

            let mut tmp_offset = *offset;
            tmp_offset += 1; // Consume identifier

            let mut matched_args = true;

            let pre_args = fn_ty.get_pre_args();
            if self.line_expressions.len() < pre_args.len() {
                continue;
            }

            for (i, pre_arg) in pre_args.iter().enumerate() {
                if self.line_expressions[i].get_type_id(self.ctx) != pre_arg.id {
                    matched_args = false;
                    break;
                }
            }

            if !matched_args {
                continue;
            }

            let prefix_args = self
                .line_expressions
                .drain(0..pre_args.len())
                .collect::<Vec<_>>();

            // self.line_expressions.clear();

            // Suffix arguments
            let mut suffix_args: Vec<AstExpression> = Vec::new();
            if fn_ty.is_varadic() {
                loop {
                    if self.check_tokens(&[TokenKind::NewLine], tmp_offset) {
                        break;
                    }

                    let expr_res = self.parse_expr(&mut tmp_offset)?;
                    if expr_res.is_none() {
                        break;
                    }

                    let res_expr = expr_res.unwrap();
                    let i = suffix_args.len(); // Current index for suffix_args

                    if !fn_ty.is_varadic()
                        && (i >= fn_ty.get_su_args().len()
                            || res_expr.get_type_id(self.ctx) != fn_ty.get_su_args()[i].id)
                    {
                        matched_args = false;
                        break;
                    }

                    suffix_args.push(res_expr);
                }

                if !matched_args {
                    continue;
                }
            } else {
                for (i, su_arg) in fn_ty.get_su_args().iter().enumerate() {
                    // Handle 'self' argument for methods on a parent struct
                    if parent.is_some()
                        && i == 0
                        && su_arg.name == "self"
                        && su_arg.id == self.ctx.type_db.new_ref(parent.unwrap())
                    {
                        continue;
                    }

                    let expr_res = self.parse_expr(&mut tmp_offset)?;

                    if expr_res.is_none() {
                        break;
                    }

                    let res_expr = expr_res.unwrap();

                    // Check if suffix argument matches expected type or is varadic
                    if su_arg.is_varadic || res_expr.get_type_id(self.ctx) == su_arg.id {
                        suffix_args.push(res_expr);
                    } else {
                        matched_args = false;
                        break;
                    }
                }

                if !matched_args {
                    continue;
                }
            }

            *offset = tmp_offset; // Update the outer offset

            let mut s_range = identifier_tk.range;
            if !prefix_args.is_empty() {
                s_range = prefix_args.last().unwrap().get_range();
            }

            return Ok(Some(CallExprAst {
                pos: FileRange::new_merging(&s_range, &self.get_tk(*offset).range),
                fn_name: fn_ty.get_name().to_string(),
                fn_id: *fn_id,
                prefix_args,
                suffix_args,
            }));
        }

        if self.debug_scan {
            println!("parsing call failed - no overload matched");
        }

        Err(CompilerError::no_overload_call_matched(
            self.get_tk(*offset),
            &self.ctx.type_db,
            fns,
        ))
    }

    // ParserResult<std::unique_ptr<StructExprAst>> parse_struct_expr(int &offset);
    pub fn parse_struct_expr(
        &mut self,
        offset: &mut usize,
    ) -> OptionalParserResult<Box<StructExprAst>> {
        let mut tmp_offset = *offset;
        if !self.check_tokens(
            &[TokenKind::Id, TokenKind::Dot, TokenKind::LBrace],
            tmp_offset,
        ) {
            return Ok(None);
        }

        let struct_name_tk = self.get_tk(tmp_offset).clone();
        let struct_name = struct_name_tk.value.clone();
        tmp_offset += 1; // Consume only the id

        let struct_type_id = self
            .ctx
            .type_db
            .get_id_by_name(&struct_name)
            .ok_or_else(|| CompilerError::type_not_found(&struct_name_tk, struct_name.clone()))?;

        let fields_res = self.parse_struct_assignments(&mut tmp_offset)?;
        let fields = fields_res.unwrap_or_else(Vec::new); // If parsing fails, treat as empty for now.

        // if !self.check_tokens(&[TokenKind::RBrace], tmp_offset) {
        //     return Err(CompilerError::unexpected_token(
        //         self.get_tk(tmp_offset),
        //         &[TokenKind::RBrace],
        //     ));
        // }
        // tmp_offset += 1; // Consume RBrace

        *offset = tmp_offset;

        Ok(Some(Box::new(StructExprAst {
            pos: FileRange::new_merging(&struct_name_tk.range, &self.get_tk(*offset).range),
            type_id: struct_type_id,
            fields,
        })))
    }

    pub fn parse_index_expr(
        &mut self,
        offset: &mut usize,
        base: AstExpression,
    ) -> (AstExpression, bool) {
        if !self.check_tokens(&[TokenKind::LBracks], *offset) {
            return (base, false);
        }
        *offset += 1; // Consume [

        let index_res = self.parse_expr(offset).unwrap();
        let index_expr =
            index_res.ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)));
        if index_expr.is_err() {
            self.errors.push(index_expr.err().unwrap());
            return (base, false);
        }

        if !self.check_tokens(&[TokenKind::RBracks], *offset) {
            // Assuming RBrace means ]
            self.errors.push(CompilerError::unexpected_token(
                self.get_tk(*offset),
                &[TokenKind::RBrace],
            ));
            return (base, false);
        }
        *offset += 1; // Consume ]

        (
            AstExpression::Index(Box::new(IndexExprAst {
                pos: FileRange::new_merging(&base.get_range(), &self.get_tk(*offset - 1).range),
                base,
                index: index_expr.unwrap(),
            })),
            true,
        )
    }

    pub fn parse_array_expr(&mut self, offset: &mut usize) -> OptionalParserResult<ArrayExprAst> {
        // Check for opening left bracket
        if !self.check_tokens(&[TokenKind::LBracks], *offset) {
            return Ok(None);
        }
        let lbracks_tk = self.get_tk(*offset).clone();
        *offset += 1; // Consume the '[' token

        let mut el_type: Option<AstTypeId> = None; // Stores the inferred element type of the array
        let mut elements: Vec<AstExpression> = Vec::new(); // Stores the parsed array elements

        loop {
            let expr_res = self.parse_expr(offset)?;

            if expr_res.is_none() {
                break;
            }

            let expr = expr_res.unwrap();
            let expr_ty = expr.get_type_id(self.ctx);

            // Ensure all expressions in the array are of the same type
            if el_type.is_some_and(|x| x != expr_ty) {
                return Err(CompilerError::multityped_array(
                    self.get_tk(*offset),
                    self.ctx.type_db.get_type(el_type.unwrap()).unwrap(),
                    self.ctx.type_db.get_type(expr_ty).unwrap(),
                ));
            } else if el_type.is_none() {
                el_type = Some(expr_ty);
            }

            elements.push(expr);

            // Check for a comma separator, continue if found
            if self.check_tokens(&[TokenKind::Comma], *offset) {
                *offset += 1;
                continue;
            }

            // Check for the closing right bracket, break if found (array parsing complete)
            if self.check_tokens(&[TokenKind::RBracks], *offset) {
                *offset += 1;
                break;
            }

            // If neither a comma nor a closing bracket is found, it's an unexpected token
            return Err(CompilerError::unexpected_token(
                self.get_tk(*offset),
                &[TokenKind::Comma, TokenKind::RBracks],
            ));
        }

        let final_el_type = el_type.unwrap_or_else(|| VOID_TYPE.get_id());

        // Create the array type in the type database
        let array_type = self.ctx.type_db.new_array(final_el_type, elements.len());

        // Return the parsed ArrayExprAst wrapped in a Box and ParserResult::Ok
        Ok(Some(ArrayExprAst {
            pos: FileRange::new_merging(&lbracks_tk.range, &self.get_tk(*offset - 1).range),
            type_id: array_type,
            elements,
        }))
    }

    pub fn parse_struct_assignments(
        &mut self,
        offset: &mut usize,
    ) -> OptionalParserResult<Vec<StructFieldAssign>> {
        let mut assignments: Vec<StructFieldAssign> = Vec::new();

        self.skip_all(&[TokenKind::NewLine], offset);

        if !self.check_tokens(&[TokenKind::Dot, TokenKind::LBrace], *offset) {
            return Ok(None);
        }
        *offset += 2; // Consume .{

        while !self.check_tokens(&[TokenKind::RBrace], *offset) {
            self.skip_all(&[TokenKind::NewLine], offset); // Skip newlines inside block

            if !self.check_tokens(&[TokenKind::Dot, TokenKind::Id], *offset) {
                return Err(CompilerError::unexpected_token(
                    self.get_tk(*offset),
                    &[TokenKind::Id],
                ));
            }
            let field_name = self.get_tk(*offset + 1).value.clone();
            *offset += 2; // Consume field name Id

            if !self.check_tokens(&[TokenKind::Id], *offset) && self.get_tk(*offset).value == "=" {
                return Err(CompilerError::unexpected_token(
                    self.get_tk(*offset),
                    &[TokenKind::Id],
                ));
            }
            *offset += 1; // Consume =

            let value_res = self.parse_expr(offset)?;
            let value = value_res
                .ok_or_else(|| CompilerError::expression_expected(self.get_tk(*offset)))?;

            assignments.push(StructFieldAssign {
                pos: FileRange::new(),
                name: field_name,
                rvalue: value,
            });

            if self.check_tokens(&[TokenKind::Comma], *offset) {
                *offset += 1; // Consume comma
            } else if !self.check_tokens(&[TokenKind::RBrace], *offset) {
                return Err(CompilerError::unexpected_token(
                    self.get_tk(*offset),
                    &[TokenKind::Comma, TokenKind::RBrace],
                ));
            }
        }
        *offset += 1; // Consume }

        Ok(Some(assignments))
    }

    // ParserResult<std::unique_ptr<BodyExprAst>> parse_fn_body(int &offset);
    pub fn parse_body(&mut self, offset: &mut usize) -> OptionalParserResult<Box<BodyExprAst>> {
        if !self.check_tokens(&[TokenKind::LBrace], *offset) {
            return Ok(None);
        }
        let lbrace_tk = self.get_tk(*offset).clone();
        *offset += 1; // Consume {

        let mut statements: Vec<AstStatement> = Vec::new();
        while !self.check_tokens(&[TokenKind::RBrace], *offset) {
            self.skip_all(&[TokenKind::NewLine], offset);
            let stmt_res = self.parse_statement(offset);
            match stmt_res {
                Ok(stmt) => {
                    statements.push(stmt);
                    self.skip_all(&[TokenKind::NewLine], offset);
                }
                Err(err) => {
                    self.errors.push(err);
                    self.skip_until(&[TokenKind::NewLine], offset);
                },
            }
        }
        *offset += 1; // Consume }

        Ok(Some(Box::new(BodyExprAst {
            pos: FileRange::new_merging(&lbrace_tk.range, &self.get_tk(*offset).range),
            statements,
        })))
    }

    pub fn parse_fn_header(
        &mut self,
        header_id: String,
        parent_struct: Option<AstTypeId>,
        offset: &mut usize,
    ) -> ParserResult<FnHeaderAst> {
        if !self.check_tokens(&[TokenKind::LPar], *offset) {
            return Err(CompilerError::unexpected_token(
                self.get_tk(*offset),
                &[TokenKind::LPar],
            ));
        }
        let lpar = self.get_tk(*offset).clone();
        *offset += 1; // Consume (

        let prefix_args_res = self.parse_fn_args(parent_struct, offset)?;
        let prefix_args = prefix_args_res.unwrap_or_else(Vec::new);

        let mut suffix_args: Vec<ArgDefAst> = Vec::new();
        if self.check_tokens(&[TokenKind::Bar], *offset) {
            *offset += 1; // Consume |
            let suffix_args_res = self.parse_fn_args(parent_struct, offset)?;
            suffix_args = suffix_args_res.unwrap_or_else(Vec::new);
            if !self.check_tokens(&[TokenKind::RPar], *offset) {
                return Err(CompilerError::unexpected_token(
                    self.get_tk(*offset),
                    &[TokenKind::RPar],
                ));
            }
            *offset += 1; // Consume )
        }

        let ret_type = if self.check_any_tokens(&[TokenKind::LBrace, TokenKind::NewLine], *offset) {
            VOID_TYPE.get_id()
        } else {
            self.parse_type(offset)?
        };

        Ok(FnHeaderAst {
            pos: FileRange::new_merging(&lpar.range, &self.get_tk(*offset - 1).range),
            name: header_id,
            ret_type,
            is_external: false, // Set by parse_fn_def
            prefix_args,
            suffix_args,
        })
    }

    pub fn parse_fn_args(
        &mut self,
        parent_struct: Option<AstTypeId>,
        offset: &mut usize,
    ) -> OptionalParserResult<Vec<ArgDefAst>> {
        let mut args: Vec<ArgDefAst> = Vec::new();

        if self.check_tokens(&[TokenKind::RPar], *offset)
            || self.check_tokens(&[TokenKind::Bar], *offset)
        {
            return Ok(Some(args));
        }

        loop {
            let arg_res: OptionalParserResult<ArgDefAst>;
            // Handle 'self' argument for methods
            if let Some(p_struct_id) = &parent_struct {
                if self.check_tokens(&[TokenKind::Id], *offset)
                    && self.get_tk(*offset).value == "self"
                {
                    let self_type_id = self.ctx.type_db.new_ref(*p_struct_id);
                    arg_res = Ok(Some(ArgDefAst {
                        pos: self.get_tk(*offset).range,
                        name: "self".to_string(),
                        type_id: self_type_id,
                        is_varadic: false,
                    }));
                    *offset += 1;
                } else {
                    arg_res = self.parse_arg_def(offset);
                }
            } else {
                arg_res = self.parse_arg_def(offset);
            }

            let arg =
                arg_res?.ok_or_else(|| CompilerError::argument_expected(self.get_tk(*offset)))?;
            args.push(arg);

            if self.check_tokens(&[TokenKind::Comma], *offset) {
                *offset += 1;
                continue;
            }

            if self.check_tokens(&[TokenKind::RPar], *offset)
                || self.check_tokens(&[TokenKind::Bar], *offset)
            {
                self.skip_all(&[TokenKind::NewLine], offset);
                break;
            }

            return Err(CompilerError::unexpected_token(
                self.get_tk(*offset),
                &[TokenKind::Comma, TokenKind::RPar, TokenKind::Bar],
            ));
        }
        Ok(Some(args))
    }

    // ParserResult<uptr<ArgDefAst>> parse_arg_def(int &offset);
    pub fn parse_arg_def(&mut self, offset: &mut usize) -> OptionalParserResult<ArgDefAst> {
        // Varadic argument: Id, Colon, Dot, Dot, Dot
        if self.check_tokens(
            &[
                TokenKind::Id,
                TokenKind::Colon,
                TokenKind::Dot,
                TokenKind::Dot,
                TokenKind::Dot,
            ],
            *offset,
        ) {
            let field = ArgDefAst {
                pos: FileRange::new_merging(
                    &self.get_tk(*offset).range,
                    &self.get_tk(*offset + 4).range,
                ),
                name: self.get_tk(*offset).value.clone(),
                type_id: VOID_TYPE.get_id(), // Using the lazy_static VOID_TYPE
                is_varadic: true,
            };
            *offset += 5;
            return Ok(Some(field));
        }

        // Standard argument: Id : Type
        if self.check_tokens(&[TokenKind::Id, TokenKind::Colon], *offset) {
            let name_tk = self.get_tk(*offset).clone();
            *offset += 2; // Consume Id and Colon

            let type_id = self.parse_type(offset)?;

            return Ok(Some(ArgDefAst {
                pos: FileRange::new_merging(&name_tk.range, &self.get_tk(*offset - 1).range),
                name: name_tk.value,
                type_id,
                is_varadic: false,
            }));
        }

        Ok(None) // No arg definition found
    }
}
