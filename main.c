#include "raylib.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "grug.h"

typedef struct {
    int x;
    int y;
} Tile;

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

int main(void) {
    grug_default_settings();

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(0, 0, "grug-factory engine");

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    Camera2D camera = { 0 };
    camera.target = (Vector2){ 0.0f, 0.0f };
    camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
    camera.zoom = 1.0f;

    SetTargetFPS(60);
    const int tileSize = 64;

    Tile* tiles = NULL;
    int tileCount = 0;
    int tileCapacity = 0;

    double logic_time = 0.0;
    double render_time = 0.0;

    while (!WindowShouldClose()) {
        double frame_start = get_time_ms();
        float dt = GetFrameTime();

        float moveSpeed = 500.0f / camera.zoom * dt;
        if (IsKeyDown(KEY_W)) camera.target.y -= moveSpeed;
        if (IsKeyDown(KEY_S)) camera.target.y += moveSpeed;
        if (IsKeyDown(KEY_A)) camera.target.x -= moveSpeed;
        if (IsKeyDown(KEY_D)) camera.target.x += moveSpeed;
        if (IsKeyPressed(KEY_F)) ToggleFullscreen();

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.zoom += wheel * 0.125f;
            if (camera.zoom < 0.25f) camera.zoom = 0.25f;
            if (camera.zoom > 2.0f) camera.zoom = 2.0f;
            Vector2 mouseWorldPosNew = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.target.x += (mouseWorldPos.x - mouseWorldPosNew.x);
            camera.target.y += (mouseWorldPos.y - mouseWorldPosNew.y);
        }

        Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
        int gridX = (int)floorf(mouseWorld.x / tileSize);
        int gridY = (int)floorf(mouseWorld.y / tileSize);

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            bool exists = false;
            for (int i = 0; i < tileCount; i++) {
                if (tiles[i].x == gridX && tiles[i].y == gridY) { exists = true; break; }
            }
            if (!exists) {
                if (tileCount == tileCapacity) {
                    tileCapacity = (tileCapacity == 0) ? 10 : tileCapacity * 2;
                    tiles = realloc(tiles, tileCapacity * sizeof(Tile));
                }
                tiles[tileCount++] = (Tile){ gridX, gridY };
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            for (int i = 0; i < tileCount; i++) {
                if (tiles[i].x == gridX && tiles[i].y == gridY) {
                    tiles[i] = tiles[tileCount - 1];
                    tileCount--;
                    break;
                }
            }
        }
        
        double input_end = get_time_ms();
        logic_time = input_end - frame_start;

        BeginDrawing();
        ClearBackground((Color){ 20, 20, 20, 255 });

        BeginMode2D(camera);

        Vector2 screenTopLeft = GetScreenToWorld2D((Vector2){ 0, 0 }, camera);
        Vector2 screenBottomRight = GetScreenToWorld2D((Vector2){ screenWidth, screenHeight }, camera);
        int startX = (int)floorf(screenTopLeft.x / tileSize) - 1;
        int endX = (int)floorf(screenBottomRight.x / tileSize) + 1;
        int startY = (int)floorf(screenTopLeft.y / tileSize) - 1;
        int endY = (int)floorf(screenBottomRight.y / tileSize) + 1;

        for (int x = startX; x <= endX; x++) DrawLine(x * tileSize, startY * tileSize, x * tileSize, endY * tileSize, (Color){ 45, 45, 45, 255 });
        for (int y = startY; y <= endY; y++) DrawLine(startX * tileSize, y * tileSize, endX * tileSize, y * tileSize, (Color){ 45, 45, 45, 255 });

        for (int i = 0; i < tileCount; i++) {
            DrawRectangle(tiles[i].x * tileSize, tiles[i].y * tileSize, tileSize, tileSize, RED);
        }

        EndMode2D();

        DrawText(TextFormat("Tile Coord: (%d, %d)", gridX, gridY), 10, 10, 20, RAYWHITE);
        DrawText(TextFormat("Total Tiles: %d", tileCount), 10, 35, 20, GRAY);
        DrawText(TextFormat("Zoom: %.2fx", camera.zoom), 10, 60, 20, GRAY);

        const char* perf_text1 = TextFormat("Logic: %.3f ms", logic_time);
        const char* perf_text2 = TextFormat("Render: %.3f ms", render_time);
        
        DrawText(perf_text1, screenWidth - MeasureText(perf_text1, 20) - 10, 10, 20, SKYBLUE);
        DrawText(perf_text2, screenWidth - MeasureText(perf_text2, 20) - 10, 35, 20, SKYBLUE);

        EndDrawing();
        
        render_time = get_time_ms() - input_end;
    }

    free(tiles);
    CloseWindow();
}
