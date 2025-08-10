mod common;

use common::assert_src;

#[test]
fn enum_declaration() {
    assert_src(
        "enum_declaration",
        "
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: cstring },
      Spider,
    }

    main :: fn(|) i32 {0}
  ",
        0,
    );
}

#[test]
fn enum_expression() {
    assert_src(
        "enum_expression",
        "
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: cstring },
      Spider,
    }

    main :: fn(|) i32 {
      animal := AnimalKind.Dog 
      0
    }
  ",
        0,
    );
}

#[test]
fn enum_specialization() {
    assert_src(
        "enum_specialization",
        "
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: cstring, },
      Spider,
    }

    main :: fn(|) i32 {
      animal := AnimalKind.Cat .{ .color = \"red\" } 
      0
    }
  ",
        0,
    );
}

#[test]
fn single_match_expression() {
    assert_src(
        "single_match_expression",
        "
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: cstring, },
      Spider,
    }

    main :: fn(|) i32 {
      animal := AnimalKind.Cat .{ .color = \"red\" } 
      match animal : AnimalKind.Cat cat {
        ret 0
      }
      1
    }
  ",
        0,
    );
}
