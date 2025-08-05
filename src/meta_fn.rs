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

#[derive(Clone, Copy, Debug)]
pub enum MetaFnKind {
    BinOp(BinOpKind),
    CmpOp(CmpOpKind),
}

#[derive(Clone, Debug)]
pub struct MetaFn {
    pub id: String,
    pub kind: MetaFnKind,
    pub ret_type: AstTypeId,
}

impl MetaFn {
    pub fn new(id: impl Into<String>, kind: MetaFnKind, ret_type: AstTypeId) -> Self {
        Self {
            id: id.into(),
            kind,
            ret_type,
        }
    }

    pub fn get_arg_size(&self) -> i32 {
        2
    }
}
