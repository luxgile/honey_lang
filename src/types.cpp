#include "types.h"

bool AstType::is_void() const { return *this == VOID_TYPE; }

AstType AstType::new_primitive(std::string name, AstTypeDb *db) {
  auto type = AstType{};
  type.db = db;
  type.kind = AstTypeKind::Primitive;
  type.name = name;
  type.id = std::hash<std::string>{}(name);
  type.parent = {};
  return type;
}

AstType AstType::new_struct(std::string name, std::vector<AstTypeField> fields,
                            AstTypeDb *db, std::optional<AstTypeId> parent_id) {
  auto type = AstType{};
  type.kind = AstTypeKind::Struct;
  type.name = name;
  type.id = std::hash<std::string>{}(name);
  type.fields = fields;
  type.parent = parent_id;
  type.db = db;
  return type;
}
AstType AstType::new_enum(std::string name, std::vector<AstTypeField> fields,
                          AstTypeDb *db, std::optional<AstTypeId> parent_id) {
  auto type = AstType{};
  type.kind = AstTypeKind::Enum;
  type.name = name;
  type.id = std::hash<std::string>{}(name);
  type.fields = fields;
  type.parent = parent_id;
  type.db = db;
  return type;
}

AstTypeId AstTypeDb::new_struct(std::string name,
                                std::vector<AstTypeField> fields,
                                std::optional<AstTypeId> parent_id) {
  auto type = AstType::new_struct(name, fields, this, parent_id);
  types.insert({type.get_id(), type});
  return type.get_id();
}

AstTypeId AstTypeDb::new_enum(std::string name,
                              std::vector<AstTypeField> fields,
                              std::optional<AstTypeId> parent_id) {
  auto type = AstType::new_enum(name, fields, this, parent_id);
  types.insert({type.get_id(), type});
  return type.get_id();
}

std::optional<const AstType *> AstTypeDb::get_type(AstTypeId id) {
  if (types.contains(id))
    return &types.at(id);
  return {};
}

std::optional<AstType *> AstTypeDb::get_type_mut(AstTypeId id) {
  if (types.contains(id))
    return &types.at(id);
  return {};
}

std::string AstType::get_fullname() const {
  auto fullname = get_name();
  if (parent.has_value())
    fullname = db->get_type(*parent).value()->get_fullname() + "_" + fullname;
  return fullname;
}
std::optional<AstTypeId> AstTypeDb::get_id_by_name(std::string name) {
  for (auto &pair : types) {
    if (pair.second.get_name() == name)
      return pair.first;
  }
  return {};
}
