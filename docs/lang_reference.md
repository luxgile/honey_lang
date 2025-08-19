# Introduction
This document will explain all features of the Honey Programming Language. 
It's assumed you have some basic knowledge of programming on other similar languages like Rust, C or Odin.

If you just want to use Honey right away, check [Quickstart](quickstart.md).

Additionally, Honey is a language in a very early state, with some of its main features still work in progress or not even implemented yet. 
This reference will note all important features that are are currently missing.

# Hello hun!
As every language, here's the usual 'hello world' in Honey. Write the following into a .hun file and run `honeyc run <file>`:
```honey
main :: fn(|) i32 {
    println "hello hun!"
    0
}
```

Alternatively you can use `honeyc build <file>` to simply build it without running it.

# Basics
## Primitive types
You can expect the following primitives in Honey:

| Name | Details |
|------|---------|
| void | represents a lack of value |
| bool | Can be either `true` or `false` |
| i8, i16, i32, i64 | signed integers and their respective size in bits |
| u8, u16, u32, u64 | unsigned integers and their respective size in bits|
| f32, f64| floating points and their respective size in bits |
| cstring | a pointer to a list of characters, equivalent to `const char*` in C |
| rawptr | a raw pointer to any kind of type, equivalent to `void*` in C |

## Comments
You can comment a single line using `#`:
```honey
# This is a comment, it will be ignored by the compiler!
```

Or multiple lines using `#+` and closing with `+#`:
```honey
#+ 
This is a longer comment! I can take as 
much space as I need to.
+#
```

## CStrings
To define a `cstring`, simply use quoting marks like so:
```honey
println "hello again!"
```

The following escaping characters are supported:
- `\n` - newline
- `\t` - tab
- `\"` - double quote
- `\\` - backslash

>[!Warning] What about actual an 'String'?
> While `String` is a supported type. It's still in work in progress as it requires other language features to be developed before being actually useful.

# Global declaration
All global symbols are declared using `::`, for now this is only limited to [Functions](https://github.com/luxgile/honey_lang/edit/main/docs/lang_reference.md#function-declaration), [Structs](https://github.com/luxgile/honey_lang/edit/main/docs/lang_reference.md#structs), [Importing](https://github.com/luxgile/honey_lang/edit/main/docs/lang_reference.md#importing) and [Enums](https://github.com/luxgile/honey_lang/edit/main/docs/lang_reference.md#enums).
```honey
main :: fn(|) i32 { 0 }
```

# Variable declaration
A variable can be declared at any point using `:=`:
```honey
x := 10
```

Variables require a type given both zero-initialization is not supported and also to infer the type.

# Variable assignment
Similarly, to assign a new value to an existing variable you can use `=`:
```honey
x := 10
x = 5
```

Note that `:=` it's only used to declare a variable, while `=` to assign it to an already existing one.

# Importing
In Honey, each file has its own scope. This means that to use anything defined in another file it needs to be imported:
```honey
import "honey/math.hun"

main :: fn(|) i32 {
    pow 10 2
}

```

You can declare a name for the imported file to avoid redefinitions or simply to now clutter the namespace:
```
math :: import "honey/math.hun"

main :: fn(|) i32 {
    math.pow 10 2
}
```

Import paths are relative from the given file. Currently the only exception to this is honey defined files which are inside the `honey/` directory.

# Function declaration
As you might have already seen, functions are declared using `my_fn :: fn(|)` which is the simplest way to declare 
a function called `my_fn` with no arguments and `void` for return type.

The name of a function is not limited to characters and numbers, here are the special characters allowed as function names:

__+ - < > = _ / \ * ~ ! $ % ; ?__

To declare the arguments, you can provide a list of `name: Type` separated by commas like so:
```honey
create_file :: fn(|file_path: cstring) { <impl> }
to_uppercase :: fn(str: cstring|) cstring { <impl> }
```

You might have realized, the `|` that sometimes it's on the left or right side of the declaration. This bar is used
to define `prefixed` and `suffixed` arguments. This means **All functions in Honey are infixed**.
Any function can additionally have any number of `prefixed` and `suffixed` arguments.

Arguments `prefixed` are declared before the call expression, while `suffixed` arguments are called after the call. This will be more clear in the following section.

# Function calling
To call a function, simply write its name and the expected arguments:
```honey
add_one :: fn(|a: i32) i32 {
    a + 1
}

six := add_one 5 
```

Calling a function does not require `( )`. However, `( )` can still be used to group expressions,
so the following is equivalent:
```honey
println "hi!"
println ("hi!")
```

`prefixed` arguments are simply declared before the function like so:
```honey
plus :: fn(lhs: i32 | rhs: i32) i32 {
    lhs + rhs
}

x := 10 plus 7
```

In fact, all operators in Honey are infixed functions:
```honey
+ :: fn (lhs: i32 | rhs: i32) i32 {
  @+ lhs rhs
} 

- :: fn (lhs: i32 | rhs: i32) i32 {
  @- lhs rhs
} 

* :: fn (lhs: i32 | rhs: i32) i32 {
  @* lhs rhs
} 

/ :: fn (lhs: i32 | rhs: i32) i32 {
  @/ lhs rhs
} 
```

This also means **there's no operator precedence in Honey**. All expressions are evaluated from left to right.
So `10 + 5 * 3` evaluates to `45` and not `18`.

### Returning a value
To return a value from a function, simply define the expression at the end of the function or use `ret` to return it early:
```honey
fib :: fn(|n: i32) i32 {
	if n <= 1 {
		ret 1
    }
	fib (n - 1) + fib (n - 2)
}
```

# Control flow
### If statement
The `if` condition does not need to be surrounded by `( )`, but requires a body using `{ }`:
```honey
if age >= 21 {
    println "I'm an adult!"
}
```

### Loop statement
Looping in Honey works as a `while` in C. The condition is constantly checked for each iteration:
```honey
i := 0
loop i <= 5 {
    printf "iteration %d \n" i
    &i += 1
}
```

Given Honey does not yet support `traits`. Loops require more boilerplate to work as expected.

Additionally, `break` can be used to exit the loop at any point.
```honey
i := 0
loop true {
    printf "iteration %d \n" i
    &i += 1
    if i == 5 {
        break
    }
}

```

### Match statement
>[!WARNING]
> **Match statements are currently unsupported.**
> However a similar feature `single matching` does pattern matching against a singular expression.
> To see the details check the [Enums](https://github.com/luxgile/honey_lang/edit/main/docs/lang_reference.md#enums). 
> The below is the current proposal for `match` and `pattern matching`.
```honey
Message :: enum {
	Quit,
	TogglePause :: bool,
	Move :: struct { x: i32, y: i32 },
	Write :: cstring,
	ChangeColor :: (i32, i32, i32)
} 

process_message :: fn (| msg: Message) {
	match msg {
		Message.Quit: {
			println "The app is quitting."
		},
		Message.TogglePause b if b: {
			println "Pausing..."
		},
		Message.TogglePause b if ! b: {
			println "Resuming..."
		},
		Message.Move move: {
			printf "Moving to x: %d, y: %d" move.x move.y
		},
		Message.Write text: {
			printf "Writing message: %s \n" text
		},
		Message.ChangeColor {r, g, b}: {
			printf "Changing color to r:%d, g:%d, b:%d \n" r g b
		},
		_: {}
	}
}
```

### Defer statement
Any statement after `defer` will be executed at the end of the body it's declared.
```honey
get_number :: fn(|) i32 {
    i := 10
    defer i = 0
    i = 5
    i
}

# This will print '0'
printf "%d" i 
```

# Declaring types
### Structs
Structs are declared similar to functions like so:
```honey
Person :: struct {
    name: cstring,
    age: i32,
}

person := Person .{ .name = "Sancho", .age = "38" }
```

**Methods**

Functions can be declared inside structs with `self` as the **first suffixed argument** to declare a method:
```honey
Person :: struct {
    name: cstring,
    age: i32,

    age_up :: fn(|self) {
        &self.age += 1
    }
}

person := Person .{ .name = "Emilia", .age = 24 }
person.age_up # Age will now be '25'
```

>[!Warning]
> **Static methods are not yet supported**
> Declaring a method without `self` won't work as expected and it's not currently handled.

### Enums
Simple enums can be declared like so:
```honey
Animal :: enum {
    Dog,
    Cat,
    Crocodile,
    Doodoo,
}

my_animal := Animal.Dog
```

Enum variants can hold additional information or structs, a feature also called `tagged union`:
```honey
Animal :: enum {
    Chicken, 
    Cat :: struct { color: cstring },
    Shark,
}

my_cat := Animal.Cat .{ .color = "orange" }
```

**Pattern matching enums**

Currently `match` can only check against one condition with a syntax similar to the [If statements]():
```honey
my_animal := Animal.Cat .{ .color = "orange" }

match my_animal : Animal.Cat cat {
    printf "my animal is a cat with the color %s \n" cat.color
}
```

### Traits
>[!Warning]
> **Traits are currently not implemented.**
> The below is the current proposal, but there's no way to use them yet.

Traits are meant to be similar to interfaces in other languages or similar to traits in rust.

# Fixed arrays
Arrays are a number of expressions arranged inside `[ ]`:
```honey
names := ["Bobby", "Robby", "Floppy", "James"]
```

They can be indexed using `[<index>]`:
```
println names[1] # prints 'Robby'
```

# Dynamic arrays (Vectors)
>[!Warning]
> **Vectors are not yet implemented.**
> They require meta programing and traits for their implementation.

# Meta functions
These are predefined functions by the compiler that have a special functionality when transpiled to C. Here's a list of them:
|Name|Description|
|-|-|
| @+ @- @* @\ ... <lhs> <rhs> | Evaluates to the literal aritmetic operators in C |
| @cast <type> <value> | Converts a value to another type if supported. Equivalent to `(type)value` in C. |
| @include "<path>" | It's converted to a literal `#include <path>` in C. |

# Pointers & Addresses
To obtain the address of a value `&` it's used. To get the value of an address use `^` instead:
```honey
x := 5
x_ptr := &my_int
^x_ptr = 3
printf "%d" x # prints '3'
```

# Memory management
### Manual
Both `malloc` and `free` are available to allocate memory on the heap:
```honey
# Allocate a 4 bytes
ptr := malloc @cast u32 4

# Cast the rawptr into the expected value
i := @cast ^i32 ptr

# Assign a value to it
^i = 7

# Free the pointer to avoid leaked memory
free ptr
```

### Dynamic
>[!Warning]
> **Not yet supported. The below is only the proposal**

Honey will support a pointer type that checks if the given pointer has already been freed or where it has been allocated from. 
```honey
heap :: import "honey/heap.hun"

# Allocate an i32 with value 0
a := @alloc i32 0

# Assign a value to it
^a = 5

# Free its memory
@free a

# Will give a runtime error 'use after free'
^a = 9 

# Pointer a is now invalid
b := @move a

# Will give a runtime error 'use after move'
^a = 9 
```

### Allocators
>[!Warning]
> **Not yet supported. The below is only the proposal**

Custom allocators can be declared and created to better control how memory is allocated and freed:
```
heap :: import "honey/heap.hun"

# Heap is the default memory allocator equivalent to C malloc
player := heap.alloc Player .{ "player", 100 }
@free player

# Here we create and use a different allocator from the default one
arena := ArenaAllocator .{}
player_b := arena.alloc Player .{ "player_b", 50 } 

# @free checks where the pointer was created and frees it as expected
@free player_b 

# Which is equivalent to this:
# arena.free player_b
```

# Using Honey with C
>[!Warning]
> **This feature is currently work in progress.**

Both structs and functions can be declared using `extern` to denote these are actually defined in a C header file.
As an example, here's the definition of `printf`:
```honey
extern printf :: fn(|format: cstring, args: ...)
```

Note that varadic arguments are only allowed in external functions and are declared with `...`.
