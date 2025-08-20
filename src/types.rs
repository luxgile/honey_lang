use lazy_static::lazy_static;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

use crate::type_db::AstTypeDb;

pub type AstTypeId = usize;

#[derive(Debug, PartialEq, Clone)]
pub struct AstNamedType {
    pub name: String,
    pub id: AstTypeId,
    pub is_varadic: bool,
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum AstTypeKind {
    Primitive,
    Reference,
    Array,
    Vector,
    Alias,
    Struct,
    Function,
    Module,
    Enum,
    Type,
}

#[derive(Debug, PartialEq, Clone)]
pub struct AstType {
    kind: AstTypeKind,
    id: AstTypeId,
    subtype: AstTypeId,
    array_size: usize,
    name: String,
    overload: u32,
    gen_args: Vec<String>,
    fields: Vec<AstNamedType>,
    methods: Vec<AstTypeId>,
    ret_type: AstTypeId,
    pre_args: Vec<AstNamedType>,
    su_args: Vec<AstNamedType>,
    parent: Option<AstTypeId>,
    external: bool,
}

impl AstType {
    fn new_base(kind: AstTypeKind, name: String) -> Self {
        AstType {
            kind,
            id: 0,
            subtype: 0,
            array_size: 0,
            name,
            overload: 0,
            gen_args: Vec::new(),
            fields: Vec::new(),
            methods: Vec::new(),
            ret_type: 0,
            pre_args: Vec::new(),
            su_args: Vec::new(),
            parent: None,
            external: false,
        }
    }

    pub fn new_type() -> Self {
        let mut type_ = Self::new_base(AstTypeKind::Type, String::new());
        type_.id = calculate_hash(&type_.name);
        type_
    }

    pub fn new_primitive(name: String) -> Self {
        let mut type_ = Self::new_base(AstTypeKind::Primitive, name);
        type_.id = calculate_hash(&type_.name);
        type_
    }

    pub fn new_reference(subtype_id: AstTypeId, db: &crate::type_db::AstTypeDb) -> Self {
        let subtype = db
            .get_type(subtype_id)
            .unwrap_or_else(|| panic!("Subtype not found for ID: {subtype_id}"));
        let name = format!("^{}", subtype.name);
        let mut type_ = Self::new_base(AstTypeKind::Reference, name);
        type_.id = calculate_hash(&type_.name);
        type_.subtype = subtype_id;
        type_
    }

    pub fn new_struct(
        name: String,
        fields: Vec<AstNamedType>,
        parent_id: Option<AstTypeId>,
        gen_args: Vec<String>,
        external: bool,
        db: &crate::type_db::AstTypeDb,
    ) -> Self {
        let mut type_ = Self::new_base(AstTypeKind::Struct, name);
        type_.fields = fields;
        type_.parent = parent_id;
        type_.id = calculate_hash(&type_.get_fullname(db));
        type_.gen_args = gen_args;
        type_.external = external;
        type_
    }

    pub fn new_enum(
        name: String,
        fields: Vec<AstNamedType>,
        parent_id: Option<AstTypeId>,
        db: &crate::type_db::AstTypeDb,
    ) -> Self {
        let mut type_ = Self::new_base(AstTypeKind::Enum, name);
        type_.fields = fields;
        type_.parent = parent_id;
        type_.id = calculate_hash(&type_.get_fullname(db));
        type_
    }

    pub fn new_array(subtype_id: AstTypeId, size: usize, db: &crate::type_db::AstTypeDb) -> Self {
        let subtype = db
            .get_type(subtype_id)
            .unwrap_or_else(|| panic!("Subtype not found for ID: {subtype_id}"));
        let name = format!("#{}[]", subtype.name);
        let mut type_ = Self::new_base(AstTypeKind::Array, name);
        type_.id = calculate_hash(&type_.name);
        type_.subtype = subtype_id;
        type_.array_size = size;
        type_
    }

    pub fn new_fn(
        name: String,
        overload: u32,
        pre_args: Vec<AstNamedType>,
        su_args: Vec<AstNamedType>,
        ret_type: AstTypeId,
        parent_struct: Option<AstTypeId>,
        external: bool,
        db: &crate::type_db::AstTypeDb,
    ) -> Self {
        let mut type_ = Self::new_base(AstTypeKind::Function, name);
        type_.overload = overload;
        type_.parent = parent_struct;
        type_.ret_type = ret_type;
        type_.pre_args = pre_args;
        type_.su_args = su_args;
        type_.id = calculate_hash(&type_.get_fullname(db));
        type_.external = external;
        type_
    }

    pub fn new_module(name: String, parent: Option<&AstTypeId>, db: &AstTypeDb) -> Self {
        let mut type_ = Self::new_base(AstTypeKind::Module, name);
        type_.parent = parent.cloned();
        type_.id = calculate_hash(&type_.get_fullname(db));
        type_
    }

    // Getters
    pub fn get_id(&self) -> AstTypeId {
        self.id
    }
    pub fn has_parent(&self) -> bool {
        self.parent.is_some()
    }

    pub fn get_parent<'a>(&self, db: &'a crate::type_db::AstTypeDb) -> &'a AstType {
        let parent_id = self
            .parent
            .unwrap_or_else(|| panic!("Parent not found for type ID: {}", self.id));
        db.get_type(parent_id)
            .unwrap_or_else(|| panic!("Parent type not found in DB for ID: {parent_id}"))
    }
    pub fn get_parent_id(&self) -> Option<AstTypeId> {
        self.parent
    }
    pub fn set_parent_id(&mut self, id: AstTypeId) {
        self.parent = Some(id);
    }
    pub fn get_subtype(&self) -> AstTypeId {
        self.subtype
    }
    pub fn get_array_size(&self) -> usize {
        self.array_size
    }

    pub fn get_fullname(&self, db: &crate::type_db::AstTypeDb) -> String {
        let mut fullname = self.name.clone();
        if let Some(parent_id) = self.parent {
            let parent_ty = db
                .get_type(parent_id)
                .unwrap_or_else(|| panic!("Parent type not found in DB for ID: {parent_id}"));
            fullname = format!("{}_{}", parent_ty.get_fullname(db), fullname);
        }
        if self.overload > 0 {
            fullname = format!("{}_{}", fullname, self.overload);
        }
        fullname
    }
    pub fn get_name(&self) -> &str {
        &self.name
    }
    pub fn set_name(&mut self, name: String) {
        self.name = name;
    }
    pub fn get_fields(&self) -> &Vec<AstNamedType> {
        &self.fields
    }
    pub fn set_fields(&mut self, fields: Vec<AstNamedType>) {
        self.fields = fields;
    }
    pub fn set_methods(&mut self, methods: Vec<AstTypeId>) {
        self.methods = methods;
    }

    pub fn get_pre_args(&self) -> &Vec<AstNamedType> {
        &self.pre_args
    }
    pub fn get_su_args(&self) -> &Vec<AstNamedType> {
        &self.su_args
    }
    pub fn get_all_args(&self) -> Vec<AstNamedType> {
        [&self.get_pre_args()[..], &self.get_su_args()[..]].concat()
    }
    pub fn is_varadic(&self) -> bool {
        !self.su_args.is_empty() && self.su_args.last().is_some_and(|arg| arg.is_varadic)
    }
    pub fn get_return_type_id(&self) -> AstTypeId {
        self.ret_type
    }
    pub fn get_overload(&self) -> u32 {
        self.overload
    }

    // Type checks
    pub fn is_void(&self) -> bool {
        self.id == VOID_TYPE.get_id()
    }

    fn is_int(&self) -> bool {
        self.id == I8_TYPE.get_id()
            || self.id == I16_TYPE.get_id()
            || self.id == I32_TYPE.get_id()
            || self.id == I64_TYPE.get_id()
    }

    fn is_uint(&self) -> bool {
        self.id == U8_TYPE.get_id()
            || self.id == U16_TYPE.get_id()
            || self.id == U32_TYPE.get_id()
            || self.id == U64_TYPE.get_id()
    }

    fn get_int_size(&self) -> u32 {
        if self.id == I8_TYPE.get_id() || self.id == U8_TYPE.get_id() {
            return 8;
        }

        if self.id == I16_TYPE.get_id() || self.id == U16_TYPE.get_id() {
            return 16;
        }

        if self.id == I32_TYPE.get_id() || self.id == U32_TYPE.get_id() {
            return 32;
        }

        if self.id == I64_TYPE.get_id() || self.id == U64_TYPE.get_id() {
            return 64;
        }

        panic!("type {} is not an int", self.get_name());
    }

    fn is_float(&self) -> bool {
        self.id == F32_TYPE.get_id() || self.id == F64_TYPE.get_id()
    }

    fn get_float_size(&self) -> u32 {
        if self.id == F32_TYPE.get_id() {
            return 32;
        }

        if self.id == F64_TYPE.get_id() {
            return 64;
        }

        panic!("type {} is not a float", self.get_name());
    }

    pub fn is_primitive(&self) -> bool {
        self.kind == AstTypeKind::Primitive
    }
    pub fn is_struct(&self) -> bool {
        self.kind == AstTypeKind::Struct
    }
    pub fn is_fn(&self) -> bool {
        self.kind == AstTypeKind::Function
    }
    pub fn is_unit(&self) -> bool {
        self.is_struct() && self.fields.is_empty()
    }
    pub fn is_enum(&self) -> bool {
        self.kind == AstTypeKind::Enum
    }
    pub fn is_ref(&self) -> bool {
        self.kind == AstTypeKind::Reference
    }
    pub fn is_array(&self) -> bool {
        self.kind == AstTypeKind::Array
    }
    pub fn is_enum_member(&self, db: &crate::type_db::AstTypeDb) -> bool {
        if let Some(parent_id) = self.parent {
            db.get_type(parent_id)
                .is_some_and(|parent_ty| parent_ty.is_enum())
        } else {
            false
        }
    }
    pub fn is_module(&self) -> bool {
        self.kind == AstTypeKind::Module
    }
    pub fn is_type(&self) -> bool {
        self.kind == AstTypeKind::Type
    }

    pub fn is_external(&self) -> bool {
        self.external
    }

    // Field and method access
    pub fn get_field_type<'a>(
        &self,
        field_id: AstTypeId,
        db: &'a crate::type_db::AstTypeDb,
    ) -> &'a AstType {
        let field = self.get_field(field_id); // This will panic if field not found
        db.get_type(field.id)
            .unwrap_or_else(|| panic!("Type not found in DB for field ID: {}", field.id))
    }

    pub fn get_field_by_idx(&self, idx: usize) -> &AstNamedType {
        if !self.is_struct() && !self.is_enum() {
            panic!(
                "Trying to get field from a non-struct/enum type for type '{}'",
                self.name
            );
        }
        self.fields.get(idx).unwrap_or_else(|| {
            panic!(
                "Trying to get field out of bounds from type '{}' (index: {})",
                self.name, idx
            )
        })
    }

    pub fn get_field_by_name(&self, name: &str) -> Option<&AstNamedType> {
        if !self.is_struct() && !self.is_enum() {
            panic!(
                "Trying to get field from a non-struct/enum type for type '{}'",
                self.name
            );
        }
        self.fields.iter().find(|field| field.name == name)
    }

    pub fn get_field_index_by_name(&self, name: &str) -> usize {
        if !self.is_struct() && !self.is_enum() {
            panic!(
                "Trying to get field from a non-struct/enum type for type '{}'",
                self.name
            );
        }
        self.fields
            .iter()
            .position(|field| field.name == name)
            .unwrap_or_else(|| panic!("No field '{name}' found in type '{}'", self.name))
    }

    pub fn get_field(&self, ty_id: AstTypeId) -> &AstNamedType {
        if !self.is_struct() && !self.is_enum() {
            panic!(
                "Trying to get field from a non-struct/enum type for type '{}'",
                self.name
            );
        }
        self.fields
            .iter()
            .find(|field| field.id == ty_id)
            .unwrap_or_else(|| {
                panic!(
                    "No field with type ID '{ty_id}' found in type '{}'",
                    self.name
                )
            })
    }

    pub fn get_field_index_by_id(&self, ty_id: AstTypeId) -> usize {
        if !self.is_struct() && !self.is_enum() {
            panic!(
                "Trying to get field from a non-struct/enum type for type '{}'",
                self.name
            );
        }
        self.fields
            .iter()
            .position(|field| field.id == ty_id)
            .unwrap_or_else(|| {
                panic!(
                    "No field with type ID '{ty_id}' found in type '{}'",
                    self.name
                )
            })
    }

    pub fn get_methods(&self) -> &Vec<AstTypeId> {
        &self.methods
    }

    pub fn get_method_by_name(
        &self,
        name: &str,
        db: &crate::type_db::AstTypeDb,
    ) -> Option<AstTypeId> {
        if !self.is_struct() && !self.is_enum() && !self.is_fn() {
            panic!(
                "Trying to get method from a non-struct/enum/function type for type '{}'",
                self.name
            );
        }

        for method_id in &self.methods {
            let m_ty = db
                .get_type(*method_id)
                .unwrap_or_else(|| panic!("Type not found in DB for method ID: {method_id}"));
            if m_ty.get_name() == name {
                return Some(*method_id);
            }
        }

        None
    }
}

fn calculate_hash<T: Hash>(t: &T) -> AstTypeId {
    let mut s = DefaultHasher::new();
    t.hash(&mut s);
    s.finish() as AstTypeId
}

lazy_static! {
    pub static ref VOID_TYPE: AstType = AstType::new_primitive("void".to_string());
    pub static ref BOOL_TYPE: AstType = AstType::new_primitive("bool".to_string());
    pub static ref I8_TYPE: AstType = AstType::new_primitive("i8".to_string());
    pub static ref I16_TYPE: AstType = AstType::new_primitive("i16".to_string());
    pub static ref I32_TYPE: AstType = AstType::new_primitive("i32".to_string());
    pub static ref I64_TYPE: AstType = AstType::new_primitive("i64".to_string());
    pub static ref U8_TYPE: AstType = AstType::new_primitive("u8".to_string());
    pub static ref U16_TYPE: AstType = AstType::new_primitive("u16".to_string());
    pub static ref U32_TYPE: AstType = AstType::new_primitive("u32".to_string());
    pub static ref U64_TYPE: AstType = AstType::new_primitive("u64".to_string());
    pub static ref F32_TYPE: AstType = AstType::new_primitive("f32".to_string());
    pub static ref F64_TYPE: AstType = AstType::new_primitive("f64".to_string());
    pub static ref CSTRING_TYPE: AstType = AstType::new_primitive("cstring".to_string());
    pub static ref RAW_PTR_TYPE: AstType = AstType::new_primitive("rawptr".to_string());
    pub static ref TYPE_TYPE: AstType = AstType::new_type();
}
