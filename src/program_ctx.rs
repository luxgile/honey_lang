use std::collections::HashMap;

use crate::{ast::*, meta_fn::MetaFn, type_db::AstTypeDb, types::*};

#[derive(Default)]
struct VarDefCtx {
    pub name: String,
    pub ty: AstTypeId,
}

#[derive(Default)]
pub struct LocalContext {
    variables: Vec<VarDefCtx>,
}

impl LocalContext {
    pub fn get_var(&self, name: &str) -> Option<AstTypeId> {
        for var in &self.variables {
            if var.name == name {
                return Some(var.ty);
            }
        }
        None
    }

    pub fn def_var(&mut self, name: String, ty: AstTypeId) {
        self.variables.push(VarDefCtx { name, ty });
    }
}

pub struct ProgramCtx {
    defined_meta: HashMap<String, Box<MetaFn>>,
    pub type_db: AstTypeDb,
    locals: Vec<LocalContext>,
}

impl ProgramCtx {
    pub fn new() -> Self {
        Self {
            defined_meta: HashMap::new(),
            type_db: AstTypeDb::new(),
            locals: vec![LocalContext::default()],
        }
    }

    pub fn push_local(&mut self) -> &LocalContext {
        let local = LocalContext::default();
        self.locals.push(local);
        self.locals.last().unwrap()
    }

    pub fn pop_local(&mut self) {
        self.locals.pop();
    }

    pub fn get_var(&self, name: &str) -> Option<AstTypeId> {
        for local in self.locals.iter().rev() {
            let var = local.get_var(name);
            if var.is_some() {
                return var;
            }
        }
        None
    }

    pub fn def_var(&mut self, name: String, ty: AstTypeId) {
        self.locals.last_mut().unwrap().def_var(name, ty);
    }

    /// Helper to register a fn into the type db.
    pub fn define_fn(
        &mut self,
        name: String,
        fn_header: &FnHeaderAst, // Take reference, not raw pointer for FnHeaderAst
        parent_struct: Option<AstTypeId>,
    ) -> AstTypeId {
        // Convert arguments to AstNamedType
        let pre_args: Vec<AstNamedType> = fn_header
            .prefix_args
            .iter()
            .map(|pre| AstNamedType {
                name: pre.name.clone(),
                id: pre.type_id,
                is_varadic: false, // C++ code explicitly says false here
            })
            .collect();

        let su_args: Vec<AstNamedType> = fn_header
            .suffix_args
            .iter()
            .map(|su| AstNamedType {
                name: su.name.clone(),
                id: su.type_id,
                is_varadic: su.is_varadic,
            })
            .collect();

        let fns = self.type_db.get_fns_by_name(&name);
        let mut overload_n = fns.len() as u32;
        let registered_fn = self.type_db.get_fn_by_args(&name, &pre_args, &su_args);

        if let Some(reg_fn_id) = registered_fn {
            if let Some(registered_fn_ty) = self.type_db.get_type(reg_fn_id) {
                overload_n = registered_fn_ty.get_overload();
            }
        }

        self.type_db.new_fn(
            name,
            overload_n,
            pre_args,
            su_args,
            fn_header.ret_type,
            fn_header.is_external,
            parent_struct,
        )
    }

    pub fn define_meta(&mut self, meta: MetaFn) {
        self.defined_meta.insert(meta.id.clone(), Box::new(meta));
    }

    pub fn get_meta(&self, name: &str) -> Option<&MetaFn> {
        self.defined_meta.get(name).map(|b| b.as_ref())
    }
}
