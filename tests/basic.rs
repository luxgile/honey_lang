mod common;
use std::fs::read_to_string;

use common::assert_src;

#[test]
fn main_func() {
    assert_src("main0", "main :: fn(|) i32 { 0 }", 0);
    assert_src("main1", "main :: fn(|) i32 { 1 }", 1);
}

#[test]
fn var_declaration() {
    assert_src(
        "var_declaration1",
        "main :: fn(|) i32 {
            var := 3
            0
        }",
        0,
    );

    assert_src(
        "var_declaration2",
        "main :: fn(|) i32 {
            var := 2.5
            0
        }",
        0,
    );

    assert_src(
        "var_declaration3",
        "main :: fn(|) i32 {
            var := true
            0
        }",
        0,
    );

    assert_src(
        "var_declaration4",
        "main :: fn(|) i32 {
            var := false
            0
        }",
        0,
    );

    assert_src(
        "var_declaration5",
        "main :: fn(|) i32 {
            var := \"hi!\"
            0
        }",
        0,
    );
}

#[test]
fn ints_decl() {
    assert_src("i8 decl", "main :: fn(|) i8 { 0 }", 0);
    assert_src("i16 decl", "main :: fn(|) i16 { 0 }", 0);
    assert_src("i32 decl", "main :: fn(|) i32 { 0 }", 0);
    assert_src("i64 decl", "main :: fn(|) i64 { 0 }", 0);
}

#[test]
fn if_statements() {
    assert_src(
        "if_statements0",
        "main :: fn(|) i32 {
            var := 3
            if @== var 3 {
                ret 0
            }
            1
        }",
        0,
    );

    assert_src(
        "if_statements1",
        "main :: fn(|) i32 {
            var := true
            if var {
                ret 0
            }
            1
        }",
        0,
    );

    assert_src(
        "if_statements2",
        "main :: fn(|) i32 {
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
        "if_statements3",
        "main :: fn(|) i32 {
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
        "var_assigment0",
        " main :: fn(|) i32 {
    var := 3
    var = 5
    if @== var 5 {
      ret 0
    }
    1
  }",
        0,
    );

    assert_src(
        "var_assigment1",
        " main :: fn(|) i32 {
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
        "main :: fn(|) i32 {
        i := 0
        loop @< i 10 {
          i = @+ i 1
        }
        i
      }",
        10,
    );
}

#[test]
fn math() {
    assert_src(
        "maths0",
        "main :: fn(|) i32 {
    @+ @* 3 5 3
  }",
        18,
    );

    assert_src(
        "maths1",
        "main :: fn(|) i32 {
    val := @- @+ 3.2 5.5 17.6
    if @< val 17.61 { # to account for floating point precission
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
        "main :: fn(|) i32 {
    i := 5
    p := &i
    ^p = 2
    ^p
  }",
        2,
    );
}

#[test]
fn pointer_file() {
    assert_src(
        "pointers file",
        read_to_string("tests/ptr.hun").expect("issue opening ptr.hun"),
        7,
    );
}

#[test]
fn pointer_with_meta_fn() {
    assert_src(
        "pointers_with_meta_fn",
        "
        += :: fn (l: ^i32 | r: i32) {
          ^l = @+ ^l r 
        }
        main :: fn(|) i32 {
            a := 5
            &a += 1
            a
        }
        ",
        6,
    );
}

#[test]
fn arrays() {
    assert_src(
        "arrays0",
        "main :: fn(|) i32 {
    array := [8.3, 1.2, 0.4]
    0
  }",
        0,
    );

    assert_src(
        "arrays1",
        "main :: fn(|) i32 {
    array := [8, 1, 0, 6, 10, 2]
    a := array[3]
    a
  }",
        6,
    );

    assert_src(
        "arrays2",
        "main :: fn(|) i32 {
    array := [8, 1, 0, 6, 10, 2]
    array[3]
  }",
        6,
    );
}

#[test]
fn calling_functions() {
    assert_src(
        "calling_functions0",
        "
    foo :: fn(|) {}
    main :: fn(|) i32 {
      foo
      0
    }
  ",
        0,
    );

    assert_src(
        "calling_functions1",
        "
    foo :: fn(|a: i32) i32 {
      @+ a 1
    }

    main :: fn(|) i32 {
      foo 4
    }
  ",
        5,
    );

    assert_src(
        "calling_functions2",
        "
    foo :: fn(l: i32 | r: i32) i32 {
      @+ l r
    }

    main :: fn(|) i32 {
      8 foo 4
    }
  ",
        12,
    );
}

#[test]
fn calling_void() {
    assert_src("calling_void", "
        do_nothing :: fn(|) {

        }

        main :: fn(|) i32 {
            do_nothing
            0
        }
        ", 0);
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

#[test]
fn local_context() {
    assert_src(
        "local_context",
        "
  foo :: fn(|a: i32) i32 {
    a
  }

  bar :: fn(|a: cstring) cstring {
    a
  }
  
  main :: fn(|) i32 {
    a := foo 7
    a
  }
  ",
        7,
    );
}

#[test]
fn casting_ints() {
    assert_src("casting_ints", "
        get_u32 :: fn(|) u32 {
            20
        }

        main :: fn(|) i32 {
            @cast i32 get_u32
        }
        ", 20);
}

#[test]
fn casting_ints_02() {
    assert_src("casting_ints_02", "
        power_of_two :: fn(|val: u32) u32 {
            @* val val
        }

        main :: fn(|) i32 {
            @cast i32 power_of_two @cast u32 10
        }
        ", 100);
}
