// auto generated file from honey - don't modify manually
// file: main.hun

#include "main.h"

int32_t main(){
  int32_t __0;
  InitWindow(800, 450, "raylib on honey");
Color __2;
  __2.r = 255;
  __2.g = 255;
  __2.b = 255;
  __2.a = 255;
  Color white = __2;
Color __3;
  __3.r = 200;
  __3.g = 200;
  __3.b = 200;
  __3.a = 255;
  Color lightgray = __3;
  while (_33(WindowShouldClose())) {
    BeginDrawing();
    ClearBackground(white);
    DrawText("Hello from Honey!", 190, 200, 20, lightgray);
    EndDrawing();
  }
  CloseWindow();
  __0 = 0;
  return __0;
}

