use crate::types::{AstNamedType, AstTypeId};

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
    NoOp(Box<NoOpAst>),
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

#[derive(Debug, Clone)]
pub struct MetaExprAst {
    pub name: String,
    pub args: Vec<AstExpression>,
}

/// Used for body statements that can be used as well as expressions.
/// This ignores the value of the expression.
#[derive(Debug, Clone)]
pub struct StatementExprAst {
    pub expr: AstExpression,
}

#[derive(Debug, Clone)]
pub struct VarDefStmtAst {
    pub name: String,
    /// It can be implicit based on the expression.
    pub type_id: AstTypeId,
    pub assignment: Option<AstExpression>,
}

#[derive(Debug, Clone)]
pub struct NoOpAst;

#[derive(Debug, Clone)]
pub struct VarAssignStmtAst {
    pub lvalue: AstExpression,
    pub rvalue: AstExpression,
}

#[derive(Debug, Clone)]
pub struct MemberAccesorExprAst {
    pub base: AstExpression,
    pub field: Option<Box<VarExprAst>>,
    pub method: Option<Box<CallExprAst>>,
}

#[derive(Debug, Clone)]
pub struct FileStmtAst {
    pub filename: String,
    pub statements: Vec<AstStatement>,
}

#[derive(Debug, Clone)]
pub struct EnumDefAst {
    pub type_id: AstTypeId,
    pub values: Vec<AstStatement>,
}

#[derive(Debug, Clone)]
pub struct EnumExprAst {
    pub enum_type: AstTypeId,
    pub struct_expr: Box<StructExprAst>,
}

#[derive(Debug, Clone)]
pub struct StructDefAst {
    pub type_id: AstTypeId,
    pub fields: Vec<ArgDefAst>,
    pub methods: Vec<FnDefAst>,
}

#[derive(Debug, Clone)]
pub struct StructExprAst {
    pub type_id: AstTypeId,
    pub fields: Vec<StructFieldAssign>,
}

#[derive(Debug, Clone)]
pub struct StructFieldAssign {
    pub name: String,
    pub rvalue: AstExpression,
}

#[derive(Debug, Clone)]
pub struct TupleDefAst {
    pub type_id: AstTypeId,
    pub fields: Vec<ArgDefAst>,
}

#[derive(Debug, Clone)]
pub struct ReturnStmtAst {
    pub expr: Option<AstExpression>,
}

#[derive(Debug, Clone)]
pub struct ArgDefAst {
    pub name: String,
    pub type_id: AstTypeId,
    pub is_varadic: bool,
}
impl From<ArgDefAst> for VarDefStmtAst {
    fn from(val: ArgDefAst) -> Self {
        VarDefStmtAst {
            name: val.name,
            type_id: val.type_id,
            assignment: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ArrayExprAst {
    pub type_id: AstTypeId,
    pub elements: Vec<AstExpression>,
}

#[derive(Debug, Clone)]
pub struct IndexExprAst {
    pub base: AstExpression,
    pub index: AstExpression,
}

#[derive(Debug, Clone)]
pub struct IntExprAst {
    pub id: AstTypeId, // Which int type is
    pub value: i64,    // Assuming int maps to i32
}

#[derive(Debug, Clone)]
pub struct FloatExprAst {
    pub id: AstTypeId, // Which float type is
    pub value: f64,    // Assuming double maps to f64
}

#[derive(Debug, Clone)]
pub struct BoolExprAst {
    pub value: bool,
}

#[derive(Debug, Clone)]
pub struct StringExprAst {
    pub value: String,
}

#[derive(Debug, Clone)]
pub struct RefExprAst {
    pub expr: AstExpression,
}

#[derive(Debug, Clone)]
pub struct DerefExprAst {
    pub expr: AstExpression,
}

#[derive(Debug, Clone)]
pub struct VarExprAst {
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct GroupExprAst {
    pub expr: AstExpression,
}

#[derive(Debug, Clone)]
pub struct SingleMatchExprAst {
    pub enum_expr: AstExpression,
    pub casted_enum_var: Box<VarDefStmtAst>,
    pub then_expr: AstExpression,
}

#[derive(Debug, Clone)]
pub struct IfExprAst {
    pub condition: AstExpression,
    pub then_expr: AstExpression,
    pub else_expr: Option<AstExpression>,
}

#[derive(Debug, Clone)]
pub struct ForExprAst {
    pub condition: AstExpression,
    pub for_body: AstExpression,
}

#[derive(Debug, Clone)]
pub struct CallExprAst {
    /// Non-mangled name of the function
    pub fn_name: String,
    /// Actual overloaded fn reference
    pub fn_id: AstTypeId,
    pub prefix_args: Vec<AstExpression>,
    pub suffix_args: Vec<AstExpression>,
}

/// 'main := prev | ret | next '
#[derive(Debug, Clone)]
pub struct FnHeaderAst {
    pub name: String,
    pub ret_type: AstTypeId,
    pub is_external: bool,
    pub prefix_args: Vec<ArgDefAst>,
    pub suffix_args: Vec<ArgDefAst>,
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
    pub statements: Vec<AstStatement>,
}

/// Function declaration 'main := | | {}'
#[derive(Debug, Clone)]
pub struct FnDefAst {
    pub id: AstTypeId,
    pub fn_header: Box<FnHeaderAst>,
    pub body: Option<AstExpression>,
}

#[derive(Debug, Clone)]
pub struct DeferStmtAst {
    pub stmt: AstStatement,
}

#[derive(Debug, Clone)]
pub struct ModuleId {
    pub name: String,
    pub child: Option<Box<ModuleId>>,
}

#[derive(Debug, Clone)]
pub struct ModuleStmtAst {
    pub ty: AstTypeId,
    pub id: ModuleId,
    pub stmts: Vec<AstStatement>,
}

#[derive(Debug, Clone)]
pub struct ModuleAccessExprAst {
    pub ty: AstTypeId,
    pub expr: AstExpression,
}

#[derive(Debug, Clone)]
pub struct ImportStmtAst {
    pub id: String,
    pub path: String,
    pub ast: FileStmtAst,
}
