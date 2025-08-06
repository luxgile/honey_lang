use std::{fs::read_to_string, process::ExitStatus};

use honey_bootstrapper_lib::Compiler;

fn assert_src(name: impl Into<String>, src: impl Into<String>, code: i32) {
    assert_eq!(
        Compiler::run_src(src.into(), name.into())
            .unwrap()
            .code()
            .unwrap(),
        code
    )
}

#[test]
fn main_func() {
    assert_src("main", "main :: fn(|) Int { 0 }", 0);
    assert_src("main", "main :: fn(|) Int { 1 }", 1);
}

#[test]
fn var_declaration() {
    assert_src(
        "var_declaration",
        "main :: fn(|) Int {
            var := 3
            0
        }",
        0,
    );

    assert_src(
        "var_declaration",
        "main :: fn(|) Int {
            var := 2.5
            0
        }",
        0,
    );

    assert_src(
        "var_declaration",
        "main :: fn(|) Int {
            var := true
            0
        }",
        0,
    );

    assert_src(
        "var_declaration",
        "main :: fn(|) Int {
            var := false
            0
        }",
        0,
    );

    assert_src(
        "var_declaration",
        "main :: fn(|) Int {
            var := \"hi!\"
            0
        }",
        0,
    );
}

#[test]
fn if_statements() {
    assert_src(
        "if_statements",
        "main :: fn(|) Int {
            var := 3
            if @i== var 3 {
                ret 0
            }
            1
        }",
        0,
    );

    assert_src(
        "if_statements",
        "main :: fn(|) Int {
            var := true
            if var {
                ret 0
            }
            1
        }",
        0,
    );

    assert_src(
        "if_statements",
        "main :: fn(|) Int {
            var := true
            if var {
                0
            } else {
                1
            }
        }",
        0,
    );

    assert_src(
        "if_statements",
        "main :: fn(|) Int {
            var := false
            if var {
                1
            } else {
                0
            }
        }",
        0,
    );
}

#[test]
fn var_assigment() {
    assert_src(
        "var_assigment",
        " main :: fn(|) Int {
    var := 3
    var = 5
    if @i== var 5 {
      ret 0
    }
    1
  }",
        0,
    );

    assert_src(
        "var_assigment",
        " main :: fn(|) Int {
    var := false
    var = true
    if var {
      ret 0
    }
    1
  }",
        0,
    );
}

#[test]
fn loops() {
    assert_src(
        "loops",
        "main :: fn(|) Int {
        i := 0
        loop @i< i 10 {
          i = @i+ i 1
        }
        i
      }",
        10,
    );
}

#[test]
fn math() {
    assert_src(
        "maths",
        "main :: fn(|) Int {
    @i+ @i* 3 5 3
  }",
        18,
    );

    assert_src(
        "maths",
        "main :: fn(|) Int {
    val := @f- @f+ 3.2 5.5 17.6
    if @f< val 17.61 { # to account for floating point precission
      ret 0
    }
    1
  }",
        0,
    );
}

#[test]
fn pointers() {
    assert_src(
        "pointers",
        "main :: fn(|) Int {
    i := 5
    p := &i
    ^p = 2
    ^p
  }",
        2,
    );

    assert_src("pointers", read_to_string("tests/ptr.hun").expect("issue opening ptr.hun"), 7);
}

#[test]
fn arrays() {
    assert_src(
        "arrays",
        "main :: fn(|) Int {
    array := [8.3, 1.2, 0.4]
    0
  }",
        0,
    );

    assert_src(
        "arrays",
        "main :: fn(|) Int {
    array := [8, 1, 0, 6, 10, 2]
    a := array[3]
    a
  }",
        6,
    );

    assert_src(
        "arrays",
        "main :: fn(|) Int {
    array := [8, 1, 0, 6, 10, 2]
    array[3]
  }",
        6,
    );
}

#[test]
fn calling_functions() {
    assert_src(
        "calling_functions",
        "
    foo :: fn(|) {}
    main :: fn(|) Int {
      foo
      0
    }
  ",
        0,
    );

    assert_src(
        "calling_functions",
        "
    foo :: fn(|a: Int) Int {
      @i+ a 1
    }

    main :: fn(|) Int {
      foo 4
    }
  ",
        5,
    );

    assert_src(
        "calling_functions",
        "
    foo :: fn(l:Int | r: Int) Int {
      @i+ l r
    }

    main :: fn(|) Int {
      8 foo 4
    }
  ",
        12,
    );
}

#[test]
fn struct_decl() {
    assert_src(
        "struct_decl",
        "
    Person :: struct {
      name: RawString,
      gender: Bool,
      age: Int,
    }

    main :: fn (|) Int { 0 }
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
      name: RawString,
      gender: Bool,
      age: Int,
    }

    main :: fn (|) Int {
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
      name: RawString,
      gender: Bool,
      age: Int,

      age_up :: fn(|self) {
        self.age = @i+ self.age 1
      }
    }

    main :: fn (|) Int {
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
      name: RawString,
      gender: Bool,
      age: Int,

      age_up :: fn(|self) {
        self.age = @i+ self.age 1
      }
    }

    main :: fn (|) Int {
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
        "struct_methods_call",
        "
    Person :: struct {
      name: RawString,
      gender: Bool,
      age: Int,

      age_up :: fn(|self) {
        self.age = @i+ self.age 1
      }
    }

    age_up :: fn(|p: ^Person) {
      p.age = @i+ p.age 1
    }

    main :: fn (|) Int {
      a := Person .{ .name = \"El Pepe\", .gender = true, .age = 30}
      a.age_up
      a.age
    }
  ",
        31,
    );
}

#[test]
fn enum_declaration() {
    assert_src(
        "enum_declaration",
        "
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: RawString },
      Spider,
    }

    main :: fn(|) Int {0}
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
      Cat :: struct { color: RawString },
      Spider,
    }

    main :: fn(|) Int {
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
      Cat :: struct { color: RawString, },
      Spider,
    }

    main :: fn(|) Int {
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
      Cat :: struct { color: RawString, },
      Spider,
    }

    main :: fn(|) Int {
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

#[test]
fn local_context() {
    assert_src(
        "local_context",
        "
  foo :: fn(|a: Int) Int {
    a
  }

  bar :: fn(|a: Float) Float {
    a
  }
  
  main :: fn(|) Int {
    a := foo 7
    b := bar 5.0
    a
  }
  ",
        7,
    );
}
