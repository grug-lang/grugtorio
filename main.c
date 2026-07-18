#include "grug.h"
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define UPS 60.0
#define UPDATE_DT_MS (1000.0 / UPS)
#define MAX_ACCUMULATED_MS 250.0
#define MAX_TICKS_PER_FRAME 5

typedef struct {
    int x;
    int y;
    int originX;
    int originY;
    int size;
    int texIdx;
    int rotation;
} Tile;

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void game_logic_tick(void) {
    printf("foo\n");
}

int main(void) {
    grug_default_settings();

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(0, 0, "grug-factory engine");

    HideCursor();

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    Camera2D camera = { 0 };
    camera.target = (Vector2){ 0.0f, 0.0f };
    camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
    camera.zoom = 1.0f;

    SetTargetFPS(60);
    const int tileSize = 64;

    Texture2D textures[10];
    for (int i = 0; i < 10; i++) {
        int num = (i == 9) ? 0 : i + 1;
        textures[i] = LoadTexture(TextFormat("textures/tiles/%d.png", num));
    }

    Texture2D cursorTex = LoadTexture("textures/cursor.png");

    Tile* tiles = NULL;
    int tileCount = 0;
    int tileCapacity = 0;
    int currentTexIndex = -1;

    double last_time = get_time_ms();
    double accumulator = 0.0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        double now = get_time_ms();
        double elapsed = now - last_time;
        last_time = now;
        if (elapsed > MAX_ACCUMULATED_MS) elapsed = MAX_ACCUMULATED_MS;
        accumulator += elapsed;

        int ticks_this_frame = 0;
        while (accumulator >= UPDATE_DT_MS) {
            game_logic_tick();
            accumulator -= UPDATE_DT_MS;
            ticks_this_frame++;
            if (ticks_this_frame >= MAX_TICKS_PER_FRAME) {
                accumulator = 0.0;
                break;
            }
        }

        for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) currentTexIndex = i;
        if (IsKeyPressed(KEY_ZERO)) currentTexIndex = 9;

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

        bool mouseOverToolbar = (GetMouseY() > screenHeight - 70);
        bool canPlace = true;

        if (currentTexIndex != -1) {
            int size = currentTexIndex + 1;
            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    int cx = gridX + dx;
                    int cy = gridY + dy;
                    for (int i = 0; i < tileCount; i++) {
                        if (tiles[i].x == cx && tiles[i].y == cy) { canPlace = false; break; }
                    }
                    if (!canPlace) break;
                }
                if (!canPlace) break;
            }
        }

        if (!mouseOverToolbar && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && currentTexIndex != -1 && canPlace) {
            int size = currentTexIndex + 1;
            if (tileCount + (size * size) > tileCapacity) {
                tileCapacity = (tileCapacity == 0) ? (size * size) : tileCapacity * 2;
                while (tileCount + (size * size) > tileCapacity) tileCapacity *= 2;
                tiles = realloc(tiles, tileCapacity * sizeof(Tile));
            }
            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    tiles[tileCount++] = (Tile){ gridX + dx, gridY + dy, gridX, gridY, size, currentTexIndex, 0 };
                }
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            int targetOriginX = -1, targetOriginY = -1, targetSize = -1;
            for (int i = 0; i < tileCount; i++) {
                if (tiles[i].x == gridX && tiles[i].y == gridY) {
                    targetOriginX = tiles[i].originX;
                    targetOriginY = tiles[i].originY;
                    targetSize = tiles[i].size;
                    break;
                }
            }
            if (targetSize != -1) {
                for (int i = tileCount - 1; i >= 0; i--) {
                    if (tiles[i].originX == targetOriginX && tiles[i].originY == targetOriginY && tiles[i].size == targetSize) {
                        tiles[i] = tiles[tileCount - 1];
                        tileCount--;
                    }
                }
            }
        }

        if (IsKeyPressed(KEY_R)) {
            for (int i = 0; i < tileCount; i++) {
                if (tiles[i].x == gridX && tiles[i].y == gridY) {
                    tiles[i].rotation = (tiles[i].rotation + 90) % 360;
                    break;
                }
            }
        }

        if (IsKeyPressed(KEY_Q)) {
            if (currentTexIndex != -1) {
                currentTexIndex = -1;
            } else {
                for (int i = 0; i < tileCount; i++) {
                    if (tiles[i].x == gridX && tiles[i].y == gridY) {
                        currentTexIndex = tiles[i].texIdx;
                        break;
                    }
                }
            }
        }

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
            Texture2D tex = textures[tiles[i].texIdx];
            Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };

            Rectangle dest = {
                (float)tiles[i].x * tileSize + (tileSize / 2.0f),
                (float)tiles[i].y * tileSize + (tileSize / 2.0f),
                (float)tileSize,
                (float)tileSize
            };

            Vector2 origin = { (float)tileSize / 2.0f, (float)tileSize / 2.0f };

            DrawTexturePro(tex, src, dest, origin, (float)tiles[i].rotation, WHITE);
        }

        if (!mouseOverToolbar && currentTexIndex != -1) {
            int size = currentTexIndex + 1;
            Texture2D tex = textures[currentTexIndex];
            Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
            Vector2 origin = { (float)tileSize / 2.0f, (float)tileSize / 2.0f };
            Color tint = canPlace ? (Color){ 0, 255, 0, 128 } : (Color){ 255, 0, 0, 128 };
            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    Rectangle dest = {
                        (float)(gridX + dx) * tileSize + (tileSize / 2.0f),
                        (float)(gridY + dy) * tileSize + (tileSize / 2.0f),
                        (float)tileSize,
                        (float)tileSize
                    };
                    DrawTexturePro(tex, src, dest, origin, 0.0f, tint);
                }
            }
        }

        EndMode2D();

        DrawText(TextFormat("Tile Coord: (%d, %d)", gridX, gridY), 10, 10, 20, RAYWHITE);
        DrawText(TextFormat("Total Tiles: %d", tileCount), 10, 35, 20, GRAY);
        DrawText(TextFormat("Zoom: %.2fx", camera.zoom), 10, 60, 20, GRAY);

        float fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
        float ups = (dt > 0.0f) ? ((float)ticks_this_frame / dt) : 0.0f;
        const char* perf_text = TextFormat("FPS/UPS = %.1f/%.1f", fps, ups);
        DrawText(perf_text, screenWidth - MeasureText(perf_text, 20) - 10, 10, 20, RAYWHITE);

        int barW = 500;
        int barH = 50;
        int startXPos = (screenWidth - barW) / 2;
        int startYPos = screenHeight - barH - 10;

        DrawRectangle(startXPos, startYPos, barW, barH, (Color){ 30, 30, 30, 255 });
        for (int i = 0; i < 10; i++) {
            int itemSize = 40;
            int x = startXPos + 5 + (i * 49);
            int y = startYPos + 5;
            DrawTexturePro(textures[i], (Rectangle){0, 0, (float)textures[i].width, (float)textures[i].height}, (Rectangle){(float)x, (float)y, (float)itemSize, (float)itemSize}, (Vector2){0,0}, 0, WHITE);
            if (i == currentTexIndex) DrawRectangleLines(x - 2, y - 2, itemSize + 4, itemSize + 4, WHITE);
        }

        DrawTextureV(cursorTex, GetMousePosition(), WHITE);

        EndDrawing();
    }

    for (int i = 0; i < 10; i++) UnloadTexture(textures[i]);
    UnloadTexture(cursorTex);
    free(tiles);
    CloseWindow();
}
