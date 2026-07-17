#include "raylib.h"

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Raylib on Arch Linux");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        
        // Wipe the previous frame clean
        ClearBackground(RAYWHITE);

        // DrawRectangle(posX, posY, width, height, color)
        // Positioning the 100x100 square in the center of the 800x450 window
        DrawRectangle(350, 175, 100, 100, RED);
        
        DrawText("Raylib is running on Arch!", 10, 10, 20, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
}
