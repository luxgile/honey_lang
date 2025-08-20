use crate::common::assert_src;

mod common;

#[test]
fn struct_empty_decl() {
    assert_src(
        "struct_empty_decl",
        "
    Person :: struct {}

    main :: fn (|) i32 { 0 }
  ",
        0,
    );
}

#[test]
fn struct_decl() {
    assert_src(
        "struct_decl",
        "
    Person :: struct {
      name: cstring,
      gender: bool,
      age: i32,
    }

    main :: fn (|) i32 { 0 }
  ",
        0,
    );
}

#[test]
fn struct_expr() {
    assert_src(
        "struct_expr",
        "
    Person :: struct {
      name: cstring,
      gender: bool,
      age: i32,
    }

    main :: fn (|) i32 {
      a := Person .{ .name = \"El Pepe\", .gender = true, .age = 30}
      a.age
    }
  ",
        30,
    );
}

#[test]
fn struct_methods_decl() {
    assert_src(
        "struct_method_decl",
        "
    Person :: struct {
      name: cstring,
      gender: bool,
      age: i32,

      age_up :: fn(|self) {
        self.age = @+ self.age 1
      }
    }

    main :: fn (|) i32 {
      0
    }
  ",
        0,
    );
}

#[test]
fn struct_methods_call() {
    assert_src(
        "struct_methods_call",
        "
    Person :: struct {
      name: cstring,
      gender: bool,
      age: i32,

      age_up :: fn(|self) {
        self.age = @+ self.age 1
      }
    }

    main :: fn (|) i32 {
      a := Person .{ .name = \"El Pepe\", .gender = true, .age = 30}
      a.age_up
      a.age
    }
  ",
        31,
    );
}

#[test]
fn method_call_fn_same_name() {
    assert_src(
        "method_call_fn_same_name",
        "
    Person :: struct {
      name: cstring,
      gender: bool,
      age: i32,

      age_up :: fn(|self) {
        self.age = @+ self.age 1
      }
    }

    age_up :: fn(|p: ^Person) {
      p.age = @+ p.age 1
    }

    main :: fn (|) i32 {
      a := Person .{ .name = \"El Pepe\", .gender = true, .age = 30}
      a.age_up
      a.age
    }
  ",
        31,
    );
}
