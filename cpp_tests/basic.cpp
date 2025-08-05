#include "testing.h"

TEST_CASE("main function return value") {
  SOURCE(R"( main :: fn(|) Int { 0 })", 0);
  SOURCE(R"( main :: fn(|) Int { 1 })", 1);
}

TEST_CASE("var declaration") {
  SOURCE(R"( main :: fn(|) Int {
    var := 3
    0
  })",
         0);

  SOURCE(R"( main :: fn(|) Int {
    var := 2.5
    0
  })",
         0);

  SOURCE(R"( main :: fn(|) Int {
    var := true
    0
  })",
         0);

  SOURCE(R"( main :: fn(|) Int {
    var := false
    0
  })",
         0);

  SOURCE(R"( main :: fn(|) Int {
    var := "hi!"
    0
  })",
         0);
}

TEST_CASE("if statement") {
  SOURCE(R"( main :: fn(|) Int {
    var := 3
    if @i== var 3 {
      ret 0
    }
    1
  })",
         0);

  SOURCE(R"( main :: fn(|) Int {
    var := true
    if var {
      ret 0
    }
    1
  })",
         0);

  SOURCE(R"( main :: fn(|) Int {
    var := true
    if var {
      0
    } else {
      1
    }
  })",
         0);

  SOURCE(R"( main :: fn(|) Int {
    var := false
    if var {
      1
    } else {
      0
    }
  })",
         0);
}

TEST_CASE("var assigment") {
  SOURCE(R"( main :: fn(|) Int {
    var := 3
    var = 5
    if @i== var 5 {
      ret 0
    }
    1
  })",
         0);

  SOURCE(R"( main :: fn(|) Int {
    var := false
    var = true
    if var {
      ret 0
    }
    1
  })",
         0);
}

TEST_CASE("loop") {
  SOURCE(R"( main :: fn(|) Int {
    i := 0
    loop @i< i 10 {
      i = @i+ i 1
    }
    i
  })",
         10);
}

TEST_CASE("math") {
  SOURCE(R"( main :: fn(|) Int {
    @i+ @i* 3 5 3
  })",
         18);

  SOURCE(R"( main :: fn(|) Int {
    val := @f- @f+ 3.2 5.5 17.6
    if @fo< val 17.61 { # to account for floating point precission
      ret 0
    }
    1
  })",
         0);
}

TEST_CASE("pointers") {
  SOURCE(R"(main :: fn(|) Int {
    i := 5
    p := &i
    ^p = 2
    ^p
  })",
         2);

  FILE("ptr.hun", 7);
}

TEST_CASE("arrays") {
  SOURCE(R"(main :: fn(|) Int {
    array := [8.3, 1.2, 0.4]
    0
  })",
         0);

  SOURCE(R"(main :: fn(|) Int {
    array := [8, 1, 0, 6, 10, 2]
    a := array[3]
    a
  })",
         6);

  SOURCE(R"(main :: fn(|) Int {
    array := [8, 1, 0, 6, 10, 2]
    array[3]
  })",
         6);
}

TEST_CASE("calling functions") {
  SOURCE(R"(
    foo :: fn(|) {}
    main :: fn(|) Int {
      foo
      0
    }
  )",
         0);

  SOURCE(R"(
    foo :: fn(|a: Int) Int {
      @i+ a 1
    }

    main :: fn(|) Int {
      foo 4
    }
  )",
         5);

  SOURCE(R"(
    foo :: fn(l:Int | r: Int) Int {
      @i+ l r
    }

    main :: fn(|) Int {
      8 foo 4
    }
  )",
         12);
}

TEST_CASE("struct decl") {
  SOURCE(R"(
    Person :: struct {
      name: RawString,
      gender: Bool,
      age: Int,
    }

    main :: fn (|) Int { 0 }
  )",
         0);
}

TEST_CASE("struct expr") {
  SOURCE(R"(
    Person :: struct {
      name: RawString,
      gender: Bool,
      age: Int,
    }

    main :: fn (|) Int {
      a := Person .{ .name = "El Pepe", .gender = true, .age = 30}
      a.age
    }
  )",
         30);
}

TEST_CASE("struct methods decl") {
  SOURCE(R"(
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
  )",
         0);
}

TEST_CASE("struct methods call") {
  SOURCE(R"(
    Person :: struct {
      name: RawString,
      gender: Bool,
      age: Int,

      age_up :: fn(|self) {
        self.age = @i+ self.age 1
      }
    }

    main :: fn (|) Int {
      a := Person .{ .name = "El Pepe", .gender = true, .age = 30}
      a.age_up
      a.age
    }
  )",
         31);
}

TEST_CASE("enum declaration") {
  SOURCE(R"(
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: RawString },
      Spider,
    }

    main :: fn(|) Int {0}
  )",
         0);
}

TEST_CASE("enum expression") {
  SOURCE(R"(
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: RawString },
      Spider,
    }

    main :: fn(|) Int {
      animal := AnimalKind.Dog 
      0
    }
  )",
         0);
}

TEST_CASE("enum specialization") {
  SOURCE(R"(
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: RawString },
      Spider,
    }

    main :: fn(|) Int {
      animal := AnimalKind.Cat .{ .color = "red" } 
      0
    }
  )",
         0);
}

TEST_CASE("single match expression") {
  SOURCE(R"(
    AnimalKind :: enum {
      Dog,
      Cat :: struct { color: RawString },
      Spider,
    }

    main :: fn(|) Int {
      animal := AnimalKind.Cat .{ .color = "red" } 
      match animal : AnimalKind.Cat cat {
        ret 0
      }
      1
    }
  )",
         0);
}

TEST_CASE("local context") {
  SOURCE(R"(
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
  )",
         7);
}
