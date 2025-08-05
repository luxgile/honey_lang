use std::collections::HashMap;

use crate::{ast::*, meta_fn::MetaFn, type_db::AstTypeDb, types::*};

pub struct VarDefCtx {
    pub name: String,
    pub ty: AstTypeId,
}

pub struct ProgramCtx {
    // enums: HashMap<String, Box<EnumDefAst>>,
    // structs: HashMap<String, Box<StructDefAst>>,
    // primitives: HashMap<String, AstTypeId>,
    defined_meta: HashMap<String, Box<MetaFn>>,
    pub type_db: AstTypeDb,
    pub defined_vars: HashMap<String, VarDefCtx>,
}

impl ProgramCtx {
    pub fn new() -> Self {
        Self {
            // llvm_types: HashMap::new(),
            // enums: HashMap::new(),
            // structs: HashMap::new(),
            // primitives: HashMap::new(),
            defined_meta: HashMap::new(),
            type_db: AstTypeDb::new(),
            // ptr_llvm_ty: std::ptr::null_mut(), // Initialize as null
            defined_vars: HashMap::new(),
        }
    }

    // pub fn define_primitive(&mut self, name: String, id: AstTypeId) {
    //     self.primitives.insert(name, id);
    // }
    //
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
            parent_struct,
        )
    }

    // pub fn define_enum(&mut self, name: String, value: Box<EnumDefAst>) {
    //     self.enums.insert(name, value);
    // }
    //
    // pub fn get_enum(&self, name: &str) -> Option<&EnumDefAst> {
    //     self.enums.get(name).map(|b| b.as_ref())
    // }
    //
    // pub fn define_struct(&mut self, name: String, value: Box<StructDefAst>) {
    //     self.structs.insert(name, value);
    // }
    //
    // pub fn get_struct(&self, name: &str) -> Option<&StructDefAst> {
    //     self.structs.get(name).map(|b| b.as_ref())
    // }

    pub fn define_meta(&mut self, meta: MetaFn) {
        self.defined_meta.insert(meta.id.clone(), Box::new(meta));
    }

    pub fn get_meta(&self, name: &str) -> Option<&MetaFn> {
        self.defined_meta.get(name).map(|b| b.as_ref())
    }

    // You might also need a `define_var` method if `defined_vars` is populated
    // by this context rather than just observed.
    pub fn define_var(&mut self, name: String, var: VarDefCtx) {
        self.defined_vars.insert(name, var);
    }
}
