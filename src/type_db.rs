use std::collections::HashMap;

use crate::types::*;

pub struct AstTypeDb {
    types: HashMap<AstTypeId, AstType>,
    next_synthetic_id: AstTypeId,
}

impl AstTypeDb {
    pub fn new() -> Self {
        let mut db = AstTypeDb {
            types: HashMap::new(),
            next_synthetic_id: 1,
        };
        db.add_type(VOID_TYPE.clone());
        db.add_type(BOOL_TYPE.clone());
        db.add_type(INT_TYPE.clone());
        db.add_type(FLOAT_TYPE.clone());
        db.add_type(RAW_STRING_TYPE.clone());
        db
    }

    fn add_type(&mut self, ty: AstType) {
        let t = AstType::new_primitive(ty.get_name().into());
        self.types.insert(t.get_id(), t);
    }

    fn get_next_synthetic_id(&mut self) -> AstTypeId {
        let id = self.next_synthetic_id;
        self.next_synthetic_id += 1;
        id
    }

    pub fn new_struct(
        &mut self,
        name: String,
        fields: Vec<AstNamedType>,
        parent_id: Option<AstTypeId>,
    ) -> AstTypeId {
        let ty = AstType::new_struct(name, fields, parent_id, self);
        let id = ty.get_id();
        self.types.insert(id, ty);
        id
    }

    pub fn new_enum(
        &mut self,
        name: String,
        fields: Vec<AstNamedType>,
        parent_id: Option<AstTypeId>,
    ) -> AstTypeId {
        let ty = AstType::new_enum(name, fields, parent_id, self);
        let id = ty.get_id();
        self.types.insert(id, ty);
        id
    }

    pub fn new_ref(&mut self, subtype_id: AstTypeId) -> AstTypeId {
        let ty = AstType::new_reference(subtype_id, self);
        let id = ty.get_id();
        self.types.insert(id, ty);
        id
    }

    pub fn get_ref(&self, subtype_id: AstTypeId) -> AstTypeId {
        let types: Vec<AstTypeId> = self
            .types
            .iter()
            .filter(|x| x.1.is_ref() && x.1.get_subtype() == subtype_id)
            .map(|x| x.1.get_id())
            .collect();

        if types.is_empty() {
            return AstType::new_reference(subtype_id, self).get_id();
        }

        if types.len() > 1 {
            panic!(
                "more than one reference type found for subtype {}",
                subtype_id
            );
        }

        *types.first().unwrap()
    }

    pub fn new_array(&mut self, subtype_id: AstTypeId, size: usize) -> AstTypeId {
        let ty = AstType::new_array(subtype_id, size, self);
        let id = ty.get_id();
        self.types.insert(id, ty);
        id
    }

    pub fn new_fn(
        &mut self,
        name: String,
        overload: u32,
        pre_args: Vec<AstNamedType>,
        su_args: Vec<AstNamedType>,
        ret_type: AstTypeId,
        parent_struct: Option<AstTypeId>,
    ) -> AstTypeId {
        let ty = AstType::new_fn(
            name,
            overload,
            pre_args,
            su_args,
            ret_type,
            parent_struct,
            self,
        );
        let id = ty.get_id();
        self.types.insert(id, ty);
        id
    }

    pub fn get_id_by_name(&self, name: &str) -> Option<AstTypeId> {
        for (id, ty) in &self.types {
            if ty.get_name() == name {
                return Some(*id);
            }
        }
        None
    }

    pub fn get_type(&self, id: AstTypeId) -> Option<&AstType> {
        self.types.get(&id)
    }

    pub fn get_type_mut(&mut self, id: AstTypeId) -> Option<&mut AstType> {
        self.types.get_mut(&id)
    }

    pub fn get_type_by_name(&self, name: &str) -> Option<&AstType> {
        self.get_id_by_name(name).and_then(|id| self.get_type(id))
    }

    pub fn get_fns_by_name(&self, name: &str) -> Vec<AstTypeId> {
        self.types
            .iter()
            .filter_map(|(&id, ty)| {
                if ty.is_fn() && ty.get_name() == name {
                    Some(id)
                } else {
                    None
                }
            })
            .collect()
    }

    pub fn get_fn_by_args(
        &self,
        name: &str,
        pre: &[AstNamedType],
        su: &[AstNamedType],
    ) -> Option<AstTypeId> {
        let eq_arg_types = |lhs: &[AstNamedType], rhs: &[AstNamedType], is_varadic: bool| {
            if lhs.len() != rhs.len() && !is_varadic {
                return false;
            }

            for i in 0..lhs.len() {
                if lhs[i].is_varadic {
                    return true;
                }
                if lhs[i].id != rhs[i].id {
                    return false;
                }
            }
            true
        };

        let fns = self.get_fns_by_name(name);
        if fns.is_empty() {
            return None;
        }

        for fn_id in fns {
            let fn_ty = self
                .get_type(fn_id)
                .expect("Function ID from get_fns_by_name must be valid");

            if !eq_arg_types(fn_ty.get_pre_args(), pre, false) {
                continue;
            }

            if !eq_arg_types(fn_ty.get_su_args(), su, fn_ty.is_varadic()) {
                continue;
            }

            return Some(fn_id);
        }
        None
    }
}
