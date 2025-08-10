use crate::types::AstTypeId;

#[derive(Clone, Copy, Debug)]
pub enum BinOpKind {
    Add,
    Minus,
    Mult,
    Div,
    Rem,
    And,
    Or,
    LShr,
    Shl,
}

#[derive(Clone, Copy, Debug)]
pub enum CmpOpKind {
    Eq,
    Ne,
    Less,
    LessEq,
    Greater,
    GreaterEq,
}

#[derive(Clone, Debug)]
pub enum MetaFnKind {
    BinOp(BinOpKind),
    CmpOp(CmpOpKind),
}

#[derive(Clone, Copy, Debug)]
pub enum MetaReturnType {
    Int,
    Float,
    Type(AstTypeId),
}

#[derive(Clone, Debug)]
pub struct MetaFn {
    pub id: String,
    pub kind: MetaFnKind,
}

impl MetaFn {
    pub fn new(id: impl Into<String>, kind: MetaFnKind) -> Self {
        Self {
            id: id.into(),
            kind,
        }
    }

    pub fn get_arg_size(&self) -> i32 {
        match self.kind {
            MetaFnKind::BinOp(_) => 2,
            MetaFnKind::CmpOp(_) => 2,
        }
    }
}
