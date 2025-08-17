use std::string;

use crate::{
    lexer::FileRange,
    types::{AstNamedType, AstTypeId},
};

pub trait AstNode {
    fn get_range(&self) -> FileRange;
}

#[derive(Debug, Clone)]
pub enum AstExpression {
    Array(Box<ArrayExprAst>),
    Index(Box<IndexExprAst>),
    Int(Box<IntExprAst>),
    Float(Box<FloatExprAst>),
    String(Box<StringExprAst>),
    Ref(Box<RefExprAst>),
    Deref(Box<DerefExprAst>),
    Bool(Box<BoolExprAst>),
    Call(Box<CallExprAst>),
    Body(Box<BodyExprAst>),
    Var(Box<VarExprAst>),
    MetaDef(Box<MetaExprAst>),
    Statement(Box<StatementExprAst>),
    Group(Box<GroupExprAst>),
    If(Box<IfExprAst>),
    For(Box<ForExprAst>),
    Struct(Box<StructExprAst>),
    MemberAccessor(Box<MemberAccesorExprAst>),
    Enum(Box<EnumExprAst>),
    SingleMatch(Box<SingleMatchExprAst>),
    ModuleAccess(Box<ModuleAccessExprAst>),
    Type(Box<TypeExprAst>),
    NoOp(Box<NoOpAst>),
}
impl AstNode for AstExpression {
    fn get_range(&self) -> FileRange {
        match self {
            AstExpression::Array(array_expr_ast) => array_expr_ast.get_range(),
            AstExpression::Index(index_expr_ast) => index_expr_ast.get_range(),
            AstExpression::Int(int_expr_ast) => int_expr_ast.get_range(),
            AstExpression::Float(float_expr_ast) => float_expr_ast.get_range(),
            AstExpression::String(string_expr_ast) => string_expr_ast.get_range(),
            AstExpression::Ref(ref_expr_ast) => ref_expr_ast.get_range(),
            AstExpression::Deref(deref_expr_ast) => deref_expr_ast.get_range(),
            AstExpression::Bool(bool_expr_ast) => bool_expr_ast.get_range(),
            AstExpression::Call(call_expr_ast) => call_expr_ast.get_range(),
            AstExpression::Body(body_expr_ast) => body_expr_ast.get_range(),
            AstExpression::Var(var_expr_ast) => var_expr_ast.get_range(),
            AstExpression::MetaDef(meta_expr_ast) => meta_expr_ast.get_range(),
            AstExpression::Statement(statement_expr_ast) => statement_expr_ast.get_range(),
            AstExpression::Group(group_expr_ast) => group_expr_ast.get_range(),
            AstExpression::If(if_expr_ast) => if_expr_ast.get_range(),
            AstExpression::For(for_expr_ast) => for_expr_ast.get_range(),
            AstExpression::Struct(struct_expr_ast) => struct_expr_ast.get_range(),
            AstExpression::MemberAccessor(member_accesor_expr_ast) => member_accesor_expr_ast.get_range(),
            AstExpression::Enum(enum_expr_ast) => enum_expr_ast.get_range(),
            AstExpression::SingleMatch(single_match_expr_ast) => single_match_expr_ast.get_range(),
            AstExpression::ModuleAccess(module_access_expr_ast) => module_access_expr_ast.get_range(),
            AstExpression::Type(type_expr_ast) => type_expr_ast.get_range(),
            AstExpression::NoOp(no_op_ast) => no_op_ast.get_range(),
        }
    }
}

#[derive(Debug, Clone)]
pub enum AstStatement {
    ArgDef(Box<ArgDefAst>),
    FnHeader(Box<FnHeaderAst>),
    FnDef(Box<FnDefAst>),
    StatementExpr(Box<StatementExprAst>),
    VarDefStmt(Box<VarDefStmtAst>),
    ReturnStmt(Box<ReturnStmtAst>),
    VarAssignStmt(Box<VarAssignStmtAst>),
    StructDef(Box<StructDefAst>),
    EnumDef(Box<EnumDefAst>),
    File(Box<FileStmtAst>),
    Defer(Box<DeferStmtAst>),
    Module(Box<ModuleStmtAst>),
    Import(Box<ImportStmtAst>),
}
impl AstNode for AstStatement {
    fn get_range(&self) -> FileRange {
        match self {
            AstStatement::ArgDef(arg_def_ast) => arg_def_ast.get_range(),
            AstStatement::FnHeader(fn_header_ast) => fn_header_ast.get_range(),
            AstStatement::FnDef(fn_def_ast) => fn_def_ast.get_range(),
            AstStatement::StatementExpr(statement_expr_ast) => statement_expr_ast.get_range(),
            AstStatement::VarDefStmt(var_def_stmt_ast) => var_def_stmt_ast.get_range(),
            AstStatement::ReturnStmt(return_stmt_ast) => return_stmt_ast.get_range(),
            AstStatement::VarAssignStmt(var_assign_stmt_ast) => var_assign_stmt_ast.get_range(),
            AstStatement::StructDef(struct_def_ast) => struct_def_ast.get_range(),
            AstStatement::EnumDef(enum_def_ast) => enum_def_ast.get_range(),
            AstStatement::File(file_stmt_ast) => file_stmt_ast.get_range(),
            AstStatement::Defer(defer_stmt_ast) => defer_stmt_ast.get_range(),
            AstStatement::Module(module_stmt_ast) => module_stmt_ast.get_range(),
            AstStatement::Import(import_stmt_ast) => import_stmt_ast.get_range(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct MetaExprAst {
    pub pos: FileRange,
    pub name: String,
    pub args: Vec<AstExpression>,
}
impl AstNode for MetaExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

/// Used for body statements that can be used as well as expressions.
/// This ignores the value of the expression.
#[derive(Debug, Clone)]
pub struct StatementExprAst {
    pub expr: AstExpression,
}
impl AstNode for StatementExprAst {
    fn get_range(&self) -> FileRange {
        self.expr.get_range()
    }
}

#[derive(Debug, Clone)]
pub struct VarDefStmtAst {
    pub pos: FileRange,
    pub name: String,
    /// It can be implicit based on the expression.
    pub type_id: AstTypeId,
    pub assignment: Option<AstExpression>,
}
impl AstNode for VarDefStmtAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct NoOpAst;
impl AstNode for NoOpAst {
    fn get_range(&self) -> FileRange {
        FileRange::new()
    }
}

#[derive(Debug, Clone)]
pub struct VarAssignStmtAst {
    pub pos: FileRange,
    pub lvalue: AstExpression,
    pub rvalue: AstExpression,
}
impl AstNode for VarAssignStmtAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct MemberAccesorExprAst {
    pub pos: FileRange,
    pub base: AstExpression,
    pub field: Option<Box<VarExprAst>>,
    pub method: Option<Box<CallExprAst>>,
}
impl AstNode for MemberAccesorExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct FileStmtAst {
    pub pos: FileRange,
    pub filename: String,
    pub statements: Vec<AstStatement>,
}
impl AstNode for FileStmtAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct EnumDefAst {
    pub pos: FileRange,
    pub type_id: AstTypeId,
    pub values: Vec<AstStatement>,
}
impl AstNode for EnumDefAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct EnumExprAst {
    pub pos: FileRange,
    pub enum_type: AstTypeId,
    pub struct_expr: Box<StructExprAst>,
}
impl AstNode for EnumExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct StructDefAst {
    pub pos: FileRange,
    pub type_id: AstTypeId,
    pub fields: Vec<ArgDefAst>,
    pub methods: Vec<FnDefAst>,
    pub external: bool,
}
impl AstNode for StructDefAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct StructExprAst {
    pub pos: FileRange,
    pub type_id: AstTypeId,
    pub fields: Vec<StructFieldAssign>,
}
impl AstNode for StructExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct StructFieldAssign {
    pub pos: FileRange,
    pub name: String,
    pub rvalue: AstExpression,
}
impl AstNode for StructFieldAssign {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct TupleDefAst {
    pub pos: FileRange,
    pub type_id: AstTypeId,
    pub fields: Vec<ArgDefAst>,
}
impl AstNode for TupleDefAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct ReturnStmtAst {
    pub pos: FileRange,
    pub expr: Option<AstExpression>,
}
impl AstNode for ReturnStmtAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct ArgDefAst {
    pub pos: FileRange,
    pub name: String,
    pub type_id: AstTypeId,
    pub is_varadic: bool,
}
impl AstNode for ArgDefAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

impl From<ArgDefAst> for VarDefStmtAst {
    fn from(val: ArgDefAst) -> Self {
        VarDefStmtAst {
            pos: val.pos,
            name: val.name,
            type_id: val.type_id,
            assignment: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ArrayExprAst {
    pub pos: FileRange,
    pub type_id: AstTypeId,
    pub elements: Vec<AstExpression>,
}
impl AstNode for ArrayExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct IndexExprAst {
    pub pos: FileRange,
    pub base: AstExpression,
    pub index: AstExpression,
}
impl AstNode for IndexExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct TypeExprAst {
    pub pos: FileRange,
    pub id: AstTypeId,
}
impl AstNode for TypeExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct IntExprAst {
    pub pos: FileRange,
    pub id: AstTypeId, // Which int type is
    pub value: i64,    // Assuming int maps to i32
}
impl AstNode for IntExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct FloatExprAst {
    pub pos: FileRange,
    pub id: AstTypeId, // Which float type is
    pub value: f64,    // Assuming double maps to f64
}
impl AstNode for FloatExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct BoolExprAst {
    pub pos: FileRange,
    pub value: bool,
}
impl AstNode for BoolExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct StringExprAst {
    pub pos: FileRange,
    pub value: String,
}
impl AstNode for StringExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct RefExprAst {
    pub pos: FileRange,
    pub expr: AstExpression,
}
impl AstNode for RefExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct DerefExprAst {
    pub pos: FileRange,
    pub expr: AstExpression,
}
impl AstNode for DerefExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct VarExprAst {
    pub pos: FileRange,
    pub name: String,
}
impl AstNode for VarExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct GroupExprAst {
    pub pos: FileRange,
    pub expr: AstExpression,
}
impl AstNode for GroupExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct SingleMatchExprAst {
    pub pos: FileRange,
    pub enum_expr: AstExpression,
    pub casted_enum_var: Box<VarDefStmtAst>,
    pub then_expr: AstExpression,
}
impl AstNode for SingleMatchExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct IfExprAst {
    pub pos: FileRange,
    pub condition: AstExpression,
    pub then_expr: AstExpression,
    pub else_expr: Option<AstExpression>,
}
impl AstNode for IfExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct ForExprAst {
    pub pos: FileRange,
    pub condition: AstExpression,
    pub for_body: AstExpression,
}
impl AstNode for ForExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct CallExprAst {
    pub pos: FileRange,
    /// Non-mangled name of the function
    pub fn_name: String,
    /// Actual overloaded fn reference
    pub fn_id: AstTypeId,
    pub prefix_args: Vec<AstExpression>,
    pub suffix_args: Vec<AstExpression>,
}
impl AstNode for CallExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

/// 'main := prev | ret | next '
#[derive(Debug, Clone)]
pub struct FnHeaderAst {
    pub pos: FileRange,
    pub name: String,
    pub ret_type: AstTypeId,
    pub is_external: bool,
    pub prefix_args: Vec<ArgDefAst>,
    pub suffix_args: Vec<ArgDefAst>,
}
impl AstNode for FnHeaderAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

impl FnHeaderAst {
    pub fn is_varadic(&self) -> bool {
        self.suffix_args.last().is_some_and(|arg| arg.is_varadic)
    }

    pub fn get_prefix_named_ids(&self) -> Vec<AstNamedType> {
        self.prefix_args
            .iter()
            .map(|arg| AstNamedType {
                name: arg.name.clone(),
                id: arg.type_id,
                is_varadic: arg.is_varadic,
            })
            .collect()
    }

    pub fn get_suffix_named_ids(&self) -> Vec<AstNamedType> {
        self.suffix_args
            .iter()
            .map(|arg| AstNamedType {
                name: arg.name.clone(),
                id: arg.type_id,
                is_varadic: arg.is_varadic,
            })
            .collect()
    }

    /// Will treat prefix and suffix as a single list and retrieve by index.
    pub fn get_arg_linear(&self, idx: usize) -> Option<&ArgDefAst> {
        if idx < self.prefix_args.len() {
            Some(&self.prefix_args[idx])
        } else {
            let suffix_idx = idx - self.prefix_args.len();
            if suffix_idx < self.suffix_args.len() {
                Some(&self.suffix_args[suffix_idx])
            } else {
                None
            }
        }
    }
}

#[derive(Debug, Clone)]
pub struct BodyExprAst {
    pub pos: FileRange,
    pub statements: Vec<AstStatement>,
}
impl AstNode for BodyExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

/// Function declaration 'main := | | {}'
#[derive(Debug, Clone)]
pub struct FnDefAst {
    pub pos: FileRange,
    pub id: AstTypeId,
    pub fn_header: FnHeaderAst,
    pub body: Option<AstExpression>,
}
impl AstNode for FnDefAst {
    fn get_range(&self) -> FileRange {
        let mut pos = self.fn_header.get_range();
        if let Some(body) = &self.body {
            pos = FileRange::new_merging(&pos, &body.get_range());
        }
        pos
    }
}

#[derive(Debug, Clone)]
pub struct DeferStmtAst {
    pub pos: FileRange,
    pub stmt: AstStatement,
}
impl AstNode for DeferStmtAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct ModuleIdAst {
    pub pos: FileRange,
    pub name: String,
    pub child: Option<Box<ModuleIdAst>>,
}
impl AstNode for ModuleIdAst {
    fn get_range(&self) -> FileRange {
        let mut pos = self.pos;
        if let Some(child) = &self.child {
            pos = FileRange::new_merging(&pos, &child.pos);
        }
        pos
    }
}

#[derive(Debug, Clone)]
pub struct ModuleStmtAst {
    pub ty: AstTypeId,
    pub id: ModuleIdAst,
    pub stmts: Vec<AstStatement>,
}
impl AstNode for ModuleStmtAst {
    fn get_range(&self) -> FileRange {
        FileRange::new_merging(&self.id.pos, &self.stmts.last().unwrap().get_range())
    }
}

/// math.add
#[derive(Debug, Clone)]
pub struct ModuleAccessExprAst {
    pub pos: FileRange,
    pub ty: AstTypeId,
    pub expr: AstExpression,
}
impl AstNode for ModuleAccessExprAst {
    fn get_range(&self) -> FileRange {
        self.pos
    }
}

#[derive(Debug, Clone)]
pub struct ImportStmtAst {
    pub id: Option<String>,
    pub path: String,
    pub ast: FileStmtAst,
}
impl AstNode for ImportStmtAst {
    fn get_range(&self) -> FileRange {
        self.ast.get_range()
    }
}
