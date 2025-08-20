<img align="left" style="width:256px" src="https://github.com/luxgile/honey_lang/blob/main/logo/honey_logo.png">

### Honey Programming Language
Honey is a fast and simple programming language centered both on flexibility, simplicity and performance. 
It's aimed to those looking for a simple language like Odin with the features of Rust. 

[**Language Reference**](./docs/lang_reference.md) - All completed, WIP or planned features listed.

[**Quickstart**](./docs/quickstart.md) - How to install and setup Honey to work with it right away.

[**Examples**](./examples) - To check how Honey works.

<br/>

> [!WARNING]
The language is very much still work in progress. While a good amount of features are already working, expect bugs, a lot of edge cases that are not handled properly and
safety checks that are not yet implemented. Working with Honey at the moment can be very frustrating. Use at your own risk!

## A taste of Honey
**Hello world**
```honey
main :: fn(|) i32 {
  println "hello hun!"
  0
}
```

**Using [Raylib](https://github.com/raysan5/raylib/tree/master)**

_More details on how to interop with C [here](./examples/raylib_game)_
```honey
rl :: import "raylib.hun"

main :: fn(|) i32 {
  rl.InitWindow 800 450 "raylib on honey"
  defer rl.CloseWindow

  white := Color .{ .r = 255, .g = 255, .b = 255, .a = 255 }
  lightgray := Color .{ .r = 200, .g = 200, .b = 200, .a = 255 }

  loop ! rl.WindowShouldClose {
    rl.BeginDrawing
    rl.ClearBackground white
    rl.DrawText "Hello from Honey!" 190 200 20 lightgray
    rl.EndDrawing
  }

  0
}
```


