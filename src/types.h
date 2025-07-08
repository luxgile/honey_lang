#pragma once

#include <cstddef>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <vector>

using AstTypeId = std::size_t;
struct AstType;
class AstTypeDb;

struct AstTypeField {
  std::string name;
  AstTypeId type;
};

enum struct AstTypeKind {
  Primitive,
  Reference,
  Array,
  Vector,
  Alias,
  Struct,
  Enum,
};

// TODO: Create a type registry...

struct AstType {
private:
  // Reference to facilitate getting type info
  AstTypeDb *db;

  // Used to differenciate between main type categories
  AstTypeKind kind;

  // Unique ID the type needs
  AstTypeId id;

  // Subtype used for references and arrays
  AstTypeId subtype;
  //
  // Subtype used for references and arrays
  int array_size;

  // User defined name. Final name might not be this as it needs to be mangled.
  std::string name;

  // Fields holded by enums or structs.
  std::vector<AstTypeField> fields;

  // Scoped types like structs inside structs or variants inside enums
  std::optional<AstTypeId> parent;

  AstType() {}

public:
  static AstType new_primitive(std::string name, AstTypeDb *db);
  static AstType new_reference(AstTypeId subtype, AstTypeDb *db);

  static AstType new_struct(std::string name, std::vector<AstTypeField> fields,
                            AstTypeDb *db, std::optional<AstTypeId> parent_id);

  static AstType new_enum(std::string name, std::vector<AstTypeField> fields,
                          AstTypeDb *db, std::optional<AstTypeId> parent_id);

  static AstType new_array(AstTypeId subtype, int size, AstTypeDb *db);

  AstTypeId get_id() const { return id; }

  bool has_parent() const { return parent.has_value(); }
  const AstType *get_parent() const;
  AstTypeId get_parent_id() const { return *parent; }
  void set_parent_id(AstTypeId id) { this->parent = id; }
  AstTypeId get_subtype() const { return this->subtype; }
  int get_array_size() const { return this->array_size; }
  std::string get_fullname() const;
  std::string get_name() const { return name; }
  void set_name(std::string name) { this->name = name; }
  std::vector<AstTypeField> get_fields() const { return fields; }
  void set_fields(std::vector<AstTypeField> fields) { this->fields = fields; }

  bool operator==(const AstType &rhs) const { return id == rhs.id; }

  bool is_void() const;
  bool is_primitive() const { return kind == AstTypeKind::Primitive; }
  bool is_struct() const { return kind == AstTypeKind::Struct; }
  bool is_unit() const { return is_struct() && fields.size() == 0; }
  bool is_enum() const { return kind == AstTypeKind::Enum; }
  bool is_ref() const { return kind == AstTypeKind::Reference; }
  bool is_enum_member() const;

  std::expected<const AstType *, std::string>
  get_field_type(AstTypeId id) const;

  std::expected<const AstTypeField *, std::string>
  get_field_by_idx(int idx) const {
    if (!is_struct() && !is_enum())
      return std::unexpected("trying to get field from a non-struct type");

    if (idx < 0 || idx >= (int)fields.size())
      return std::unexpected("trying to get field out of bounds from a type");

    return &fields[idx];
  }

  std::expected<const AstTypeField *, std::string>
  get_field_by_name(std::string name) const {
    if (!is_struct() && !is_enum())
      return std::unexpected("trying to get field from a non-struct type");

    for (auto &field : fields) {
      if (field.name == name)
        return &field;
    }

    return std::unexpected(
        std::format("no field '{}' found in type '{}'", name, get_name()));
  }

  std::expected<int, std::string>
  get_field_index_by_name(std::string name) const {
    if (!is_struct() && !is_enum())
      return std::unexpected("trying to get field from a non-struct type");

    int idx = 0;
    for (auto &field : fields) {
      if (field.name == name)
        return idx;
      idx += 1;
    }

    return std::unexpected(
        std::format("no field '{}' found in type '{}'", name, get_name()));
  }

  std::expected<const AstTypeField *, std::string>
  get_field(AstTypeId id) const {
    if (!is_struct() && !is_enum())
      return std::unexpected("trying to get field from a non-struct type");

    for (auto &field : fields) {
      if (field.type == id)
        return &field;
    }

    return std::unexpected(
        std::format("no field '{}' found in type '{}'", id, get_name()));
  }

  std::expected<int, std::string> get_field_index_by_id(AstTypeId id) const {
    if (!is_struct() && !is_enum())
      return std::unexpected("trying to get field from a non-struct type");

    int idx = 0;
    for (auto &field : fields) {
      if (field.type == id)
        return idx;
      idx += 1;
    }

    return std::unexpected(
        std::format("no field '{}' found in type '{}'", id, get_name()));
  }
};

static AstType VOID_TYPE = AstType::new_primitive("Void", nullptr);
static AstType BOOL_TYPE = AstType::new_primitive("Bool", nullptr);
static AstType INT_TYPE = AstType::new_primitive("Int", nullptr);
static AstType FLOAT_TYPE = AstType::new_primitive("Float", nullptr);
static AstType RAW_STRING_TYPE = AstType::new_primitive("RawString", nullptr);

class AstTypeDb {
  std::map<AstTypeId, AstType> types;

  void add_type(AstType type) {
    auto n_type = AstType::new_primitive(type.get_name(), this);
    types.insert({type.get_id(), type});
  }

public:
  AstTypeDb() {
    add_type(VOID_TYPE);
    add_type(BOOL_TYPE);
    add_type(INT_TYPE);
    add_type(FLOAT_TYPE);
    add_type(RAW_STRING_TYPE);
  }

  AstTypeId new_struct(std::string name, std::vector<AstTypeField> fields,
                       std::optional<AstTypeId> parent_id);

  AstTypeId new_enum(std::string name, std::vector<AstTypeField> fields,
                     std::optional<AstTypeId> parent_id);

  AstTypeId new_ref(AstTypeId subtype);
  AstTypeId new_array(AstTypeId subtype, int size);

  std::optional<AstTypeId> get_id_by_name(std::string name);
  std::optional<const AstType *> get_type_by_name(std::string name);
  std::optional<const AstType *> get_type(AstTypeId id);
  std::optional<AstType *> get_type_mut(AstTypeId id);
};
