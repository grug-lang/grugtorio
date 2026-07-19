#include "grug.h"
#include "raylib.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#define UPS 60.0
#define UPDATE_DT_MS (1000.0 / UPS)
#define MAX_ACCUMULATED_MS 250.0
#define MAX_TICKS_PER_FRAME 5

typedef struct {
    const char* name;
    Color color;
    int size;
} TileType;

static const TileType TILE_TYPES[10] = {
    { "Electric mining drill", { 130, 130, 130, 255 }, 3 },
    { "Transport belt", { 170, 160, 110, 255 }, 1 },
    { "Inserter", { 230, 200, 40, 255 }, 1 },
    { "Assembling machine 1", { 130, 105, 90, 255 }, 3 },
    { NULL, { 0, 0, 0, 0 }, 0 },
    { NULL, { 0, 0, 0, 0 }, 0 },
    { NULL, { 0, 0, 0, 0 }, 0 },
    { NULL, { 0, 0, 0, 0 }, 0 },
    { NULL, { 0, 0, 0, 0 }, 0 },
    { NULL, { 0, 0, 0, 0 }, 0 },
};

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
    // printf("tick\n");
}

static Vector2 AngleToDir(float angleDeg) {
    float rad = angleDeg * DEG2RAD;
    return (Vector2){ sinf(rad), -cosf(rad) };
}

static void DrawChevron(Vector2 tip, float armLength, float angleDeg, float spreadDeg, float thickness, Color color) {
    float backAngle = angleDeg + 180.0f;
    Vector2 arm1 = AngleToDir(backAngle - spreadDeg);
    Vector2 arm2 = AngleToDir(backAngle + spreadDeg);
    Vector2 p1 = { tip.x + arm1.x * armLength, tip.y + arm1.y * armLength };
    Vector2 p2 = { tip.x + arm2.x * armLength, tip.y + arm2.y * armLength };
    DrawLineEx(tip, p1, thickness, color);
    DrawLineEx(tip, p2, thickness, color);
}

static void DrawKinkedChevron(Vector2 a, Vector2 c, float kinkOffset, float thickness, Color color) {
    Vector2 d = { c.x - a.x, c.y - a.y };
    Vector2 perp = { -d.y, d.x };
    float len = sqrtf(perp.x * perp.x + perp.y * perp.y);
    if (len > 0.0f) {
        perp.x /= len;
        perp.y /= len;
    }
    Vector2 mid = { (a.x + c.x) / 2.0f + perp.x * kinkOffset, (a.y + c.y) / 2.0f + perp.y * kinkOffset };
    DrawLineEx(a, mid, thickness, color);
    DrawLineEx(mid, c, thickness, color);
}

int main(void) {
    grug_default_settings();

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(0, 0, "grugtorio");

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
    int currentTexIndex = -1;
    int currentHeldRotation = 0;

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

        for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i) && TILE_TYPES[i].name != NULL) currentTexIndex = i;
        if (IsKeyPressed(KEY_ZERO) && TILE_TYPES[9].name != NULL) currentTexIndex = 9;

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
            int size = TILE_TYPES[currentTexIndex].size;
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
            int size = TILE_TYPES[currentTexIndex].size;
            if (tileCount + (size * size) > tileCapacity) {
                tileCapacity = (tileCapacity == 0) ? (size * size) : tileCapacity * 2;
                while (tileCount + (size * size) > tileCapacity) tileCapacity *= 2;
                tiles = realloc(tiles, tileCapacity * sizeof(Tile));
            }
            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    tiles[tileCount++] = (Tile){ gridX + dx, gridY + dy, gridX, gridY, size, currentTexIndex, currentHeldRotation };
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
            if (currentTexIndex != -1) {
                currentHeldRotation = (currentHeldRotation + 90) % 360;
            } else {
                for (int i = 0; i < tileCount; i++) {
                    if (tiles[i].x == gridX && tiles[i].y == gridY) {
                        tiles[i].rotation = (tiles[i].rotation + 90) % 360;
                        break;
                    }
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
            Rectangle dest = {
                (float)tiles[i].x * tileSize + (tileSize / 2.0f),
                (float)tiles[i].y * tileSize + (tileSize / 2.0f),
                (float)tileSize,
                (float)tileSize
            };

            Vector2 origin = { (float)tileSize / 2.0f, (float)tileSize / 2.0f };

            DrawRectanglePro(dest, origin, (float)tiles[i].rotation, TILE_TYPES[tiles[i].texIdx].color);
        }

        for (int i = 0; i < tileCount; i++) {
            if (tiles[i].x != tiles[i].originX || tiles[i].y != tiles[i].originY) continue;

            Vector2 dir = AngleToDir((float)tiles[i].rotation);
            float originPxX = (float)tiles[i].originX * tileSize;
            float originPxY = (float)tiles[i].originY * tileSize;
            float sizePx = (float)tiles[i].size * tileSize;
            Vector2 buildingCenter = { originPxX + sizePx / 2.0f, originPxY + sizePx / 2.0f };
            Color chevronColor = (Color){ 20, 20, 20, 220 };

            if (tiles[i].texIdx == 0) {
                Vector2 tip = { buildingCenter.x + dir.x * tileSize, buildingCenter.y + dir.y * tileSize };
                DrawChevron(tip, tileSize * 0.18f, (float)tiles[i].rotation, 35.0f, 2.0f, chevronColor);
            } else if (tiles[i].texIdx == 1) {
                Vector2 tileCenter = { originPxX + tileSize / 2.0f, originPxY + tileSize / 2.0f };
                Vector2 startTip = { tileCenter.x - dir.x * tileSize * 0.22f, tileCenter.y - dir.y * tileSize * 0.22f };
                Vector2 endTip = { tileCenter.x + dir.x * tileSize * 0.22f, tileCenter.y + dir.y * tileSize * 0.22f };
                DrawChevron(startTip, tileSize * 0.14f, (float)tiles[i].rotation, 35.0f, 2.0f, chevronColor);
                DrawChevron(endTip, tileSize * 0.14f, (float)tiles[i].rotation, 35.0f, 2.0f, chevronColor);
            } else if (tiles[i].texIdx == 2) {
                Vector2 tileCenter = { originPxX + tileSize / 2.0f, originPxY + tileSize / 2.0f };
                Vector2 edgePoint = { tileCenter.x + dir.x * tileSize * 0.5f, tileCenter.y + dir.y * tileSize * 0.5f };
                DrawKinkedChevron(tileCenter, edgePoint, tileSize * 0.12f, 3.0f, chevronColor);
            }
        }

        if (!mouseOverToolbar && currentTexIndex != -1) {
            int size = TILE_TYPES[currentTexIndex].size;
            Vector2 origin = { (float)tileSize / 2.0f, (float)tileSize / 2.0f };
            Color base = TILE_TYPES[currentTexIndex].color;
            Color tint = canPlace ? (Color){ base.r, base.g, base.b, 150 } : (Color){ 255, 0, 0, 150 };
            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    Rectangle dest = {
                        (float)(gridX + dx) * tileSize + (tileSize / 2.0f),
                        (float)(gridY + dy) * tileSize + (tileSize / 2.0f),
                        (float)tileSize,
                        (float)tileSize
                    };
                    DrawRectanglePro(dest, origin, (float)currentHeldRotation, tint);
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
            if (TILE_TYPES[i].name != NULL) {
                DrawRectangle(x, y, itemSize, itemSize, TILE_TYPES[i].color);
            } else {
                DrawRectangle(x, y, itemSize, itemSize, (Color){ 45, 45, 45, 255 });
            }
            if (i == currentTexIndex) {
                DrawRectangleLines(x - 2, y - 2, itemSize + 4, itemSize + 4, WHITE);
                const char* heldName = TILE_TYPES[i].name;
                int textW = MeasureText(heldName, 14);
                DrawText(heldName, x + itemSize / 2 - textW / 2, y - 16, 14, RAYWHITE);
            }
        }

        EndDrawing();
    }

    free(tiles);
    CloseWindow();
}
