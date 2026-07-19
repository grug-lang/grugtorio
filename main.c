#include "grug.h"
#include "raylib.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define UPS 60.0
#define UPDATE_DT_MS (1000.0 / UPS)
#define MAX_ACCUMULATED_MS 250.0
#define MAX_TICKS_PER_FRAME 5

typedef enum {
    BUILDING_NONE = -1,
    BUILDING_MINING_DRILL = 0,
    BUILDING_TRANSPORT_BELT = 1,
    BUILDING_INSERTER = 2,
    BUILDING_STONE_FURNACE = 3,
    BUILDING_ASSEMBLING_MACHINE_1 = 4
} BuildingTypeEnum;

typedef enum {
    ITEM_IRON_ORE = 0,
    ITEM_COAL = 1,
    ITEM_COPPER_ORE = 2,
    ITEM_TYPE_COUNT = 3
} ItemTypeEnum;

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT = 90,
    DIR_DOWN = 180,
    DIR_LEFT = 270
} Direction;

typedef enum {
    ROT_CW = 90,
    ROT_CCW = 270
} Rotation;

typedef struct {
    const char* name;
    Color color;
    int size;
} BuildingType;

static const BuildingType BUILDING_TYPES[10] = {
    { "Electric mining drill", { 130, 130, 130, 255 }, 3 },
    { "Transport belt", { 170, 160, 110, 255 }, 1 },
    { "Inserter", { 230, 200, 40, 255 }, 1 },
    { "Stone furnace", { 210, 180, 140, 255 }, 2 },
    { "Assembling machine 1", { 130, 105, 90, 255 }, 3 },
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
    BuildingTypeEnum typeIdx;
    int rotation;
    int progress;
    float belt_items[2][5];
    ItemTypeEnum belt_item_types[2][5];
    ItemTypeEnum outputType;
} Building;

typedef struct {
    float x;
    float y;
    ItemTypeEnum type;
} Item;

static Color GetItemColor(ItemTypeEnum type) {
    switch (type) {
        case ITEM_IRON_ORE: return (Color){ 100, 150, 200, 255 };
        case ITEM_COAL: return (Color){ 50, 50, 50, 255 };
        case ITEM_COPPER_ORE: return (Color){ 200, 150, 100, 255 };
        case ITEM_TYPE_COUNT: abort();
    }
    abort();
}

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int GetFeederRotation(Building* belt, Building* buildings, int buildingCount) {
    int dirs[4] = { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT };
    for (int i = 0; i < 4; i++) {
        int rot = dirs[i];
        int dx = (int)roundf(sinf(rot * DEG2RAD));
        int dy = (int)roundf(-cosf(rot * DEG2RAD));
        int checkX = belt->x - dx;
        int checkY = belt->y - dy;
        for (int b = 0; b < buildingCount; b++) {
            if (buildings[b].typeIdx == BUILDING_TRANSPORT_BELT && buildings[b].x == checkX && buildings[b].y == checkY && buildings[b].rotation == rot) {
                return rot;
            }
        }
    }
    return -1;
}

static bool HasBeltFeedingFrom(int targetX, int targetY, int feederRot, Building* buildings, int count) {
    if (!buildings) return false;
    int dx = (int)roundf(sinf(feederRot * DEG2RAD));
    int dy = (int)roundf(-cosf(feederRot * DEG2RAD));
    int feederX = targetX - dx;
    int feederY = targetY - dy;
    for (int i = 0; i < count; i++) {
        if (buildings[i].typeIdx == BUILDING_TRANSPORT_BELT && buildings[i].x == feederX && buildings[i].y == feederY && buildings[i].rotation == feederRot) {
            return true;
        }
    }
    return false;
}

static int GetBeltInputRotation(Building* belt, Building* buildings, int count) {
    int rot = belt->rotation;
    if (HasBeltFeedingFrom(belt->x, belt->y, rot, buildings, count)) return rot;
    bool hasLeft = HasBeltFeedingFrom(belt->x, belt->y, (rot + ROT_CW) % 360, buildings, count);
    bool hasRight = HasBeltFeedingFrom(belt->x, belt->y, (rot + ROT_CCW) % 360, buildings, count);
    if (hasLeft && !hasRight) return (rot + ROT_CW) % 360;
    if (hasRight && !hasLeft) return (rot + ROT_CCW) % 360;
    return rot;
}

static int GetBeltLaneCapacity(Building* belt, int lane, Building* buildings, int buildingCount) {
    int feederRot = GetFeederRotation(belt, buildings, buildingCount);
    if (feederRot != -1) {
        int relRot = (belt->rotation - feederRot + 360) % 360;
        if (relRot == 0) return 4;
        if (relRot == ROT_CW) return (lane == 1) ? 2 : 5;
        if (relRot == ROT_CCW) return (lane == 0) ? 2 : 5;
    }
    return 4;
}

static Vector2 AngleToDir(float angleDeg) {
    float rad = angleDeg * DEG2RAD;
    return (Vector2){ sinf(rad), -cosf(rad) };
}

static void game_logic_tick(Building* buildings, int buildingCount, Item** items, int* itemCount, int* itemCapacity, int tileSize) {
    for (int i = 0; i < buildingCount; i++) {
        if (buildings[i].typeIdx == BUILDING_TRANSPORT_BELT) {
            for (int l = 0; l < 2; l++) {
                int capacity = GetBeltLaneCapacity(&buildings[i], l, buildings, buildingCount);
                for (int j = 0; j < capacity; j++) {
                    if (buildings[i].belt_items[l][j] >= 0.0f) {
                        float max_p = 1.0f;
                        if (j > 0 && buildings[i].belt_items[l][j - 1] >= 0.0f) {
                            max_p = buildings[i].belt_items[l][j - 1] - 0.25f;
                        }
                        buildings[i].belt_items[l][j] += 1.0f / UPS;
                        if (buildings[i].belt_items[l][j] > max_p) {
                            buildings[i].belt_items[l][j] = max_p;
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < buildingCount; i++) {
        if (buildings[i].typeIdx == BUILDING_TRANSPORT_BELT) {
            for (int l = 0; l < 2; l++) {
                if (buildings[i].belt_items[l][0] >= 1.0f) {
                    int dirX = (int)roundf(sinf(buildings[i].rotation * DEG2RAD));
                    int dirY = (int)roundf(-cosf(buildings[i].rotation * DEG2RAD));
                    int tx = buildings[i].x + dirX;
                    int ty = buildings[i].y + dirY;

                    int targetBeltIdx = -1;
                    for (int k = 0; k < buildingCount; k++) {
                        if (buildings[k].typeIdx == BUILDING_TRANSPORT_BELT && buildings[k].x == tx && buildings[k].y == ty) {
                            targetBeltIdx = k;
                            break;
                        }
                    }

                    if (targetBeltIdx != -1) {
                        Vector2 tDir = AngleToDir((float)buildings[targetBeltIdx].rotation);
                        if (buildings[targetBeltIdx].x + (int)tDir.x == buildings[i].x && buildings[targetBeltIdx].y + (int)tDir.y == buildings[i].y) continue;

                        int targetLane = l;

                        int capacity = GetBeltLaneCapacity(&buildings[targetBeltIdx], targetLane, buildings, buildingCount);
                        int lastItemIdx = -1;
                        for (int j = 0; j < capacity; j++) {
                            if (buildings[targetBeltIdx].belt_items[targetLane][j] >= 0.0f) lastItemIdx = j;
                        }

                        if (lastItemIdx < capacity - 1 && (lastItemIdx == -1 || buildings[targetBeltIdx].belt_items[targetLane][lastItemIdx] >= 0.25f)) {
                            buildings[targetBeltIdx].belt_items[targetLane][lastItemIdx + 1] = 0.0f;
                            buildings[targetBeltIdx].belt_item_types[targetLane][lastItemIdx + 1] = buildings[i].belt_item_types[l][0];

                            for (int j = 0; j < 4; j++) {
                                buildings[i].belt_items[l][j] = buildings[i].belt_items[l][j + 1];
                                buildings[i].belt_item_types[l][j] = buildings[i].belt_item_types[l][j + 1];
                            }
                            buildings[i].belt_items[l][4] = -1.0f;
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < buildingCount; i++) {
        if (buildings[i].typeIdx == BUILDING_MINING_DRILL && buildings[i].x == buildings[i].originX && buildings[i].y == buildings[i].originY) {
            if (buildings[i].progress < 120) buildings[i].progress++;
            if (buildings[i].progress >= 120) {
                int dirX = (int)roundf(sinf(buildings[i].rotation * DEG2RAD));
                int dirY = (int)roundf(-cosf(buildings[i].rotation * DEG2RAD));
                int tx = buildings[i].originX + 1 + dirX * 2;
                int ty = buildings[i].originY + 1 + dirY * 2;

                int targetBeltIdx = -1;
                for (int j = 0; j < buildingCount; j++) {
                    if (buildings[j].typeIdx == BUILDING_TRANSPORT_BELT && buildings[j].x == tx && buildings[j].y == ty) {
                        targetBeltIdx = j;
                        break;
                    }
                }

                if (targetBeltIdx != -1) {
                    int relRot = (buildings[targetBeltIdx].rotation - buildings[i].rotation + 360) % 360;
                    int l = (relRot == ROT_CCW) ? 0 : 1;

                    float insertProgress = (relRot == 0) ? 0.25f : 0.5f;

                    int capacity = GetBeltLaneCapacity(&buildings[targetBeltIdx], l, buildings, buildingCount);
                    int lastItemIdx = -1;
                    for (int j = 0; j < capacity; j++) {
                        if (buildings[targetBeltIdx].belt_items[l][j] >= 0.0f)
                            lastItemIdx = j;
                    }

                    if (lastItemIdx < capacity - 1 &&
                        (lastItemIdx == -1 ||
                        buildings[targetBeltIdx].belt_items[l][lastItemIdx] >= insertProgress + 0.25f)) {

                        buildings[targetBeltIdx].belt_items[l][lastItemIdx + 1] = insertProgress;
                        buildings[targetBeltIdx].belt_item_types[l][lastItemIdx + 1] = buildings[i].outputType;
                        buildings[i].progress = 0;
                    }
                } else {
                    bool itemCollision = false;
                    for (int k = 0; k < *itemCount; k++) {
                        if ((int)floorf((*items)[k].x / tileSize) == tx && (int)floorf((*items)[k].y / tileSize) == ty) {
                            itemCollision = true; break;
                        }
                    }

                    if (!itemCollision) {
                        float itemX = (tx + 0.5f - (float)dirX * 0.152f) * tileSize;
                        float itemY = (ty + 0.5f - (float)dirY * 0.152f) * tileSize;
                        if (*itemCount >= *itemCapacity) {
                            *itemCapacity = (*itemCapacity == 0) ? 64 : *itemCapacity * 2;
                            *items = realloc(*items, *itemCapacity * sizeof(Item));
                        }
                        (*items)[(*itemCount)++] = (Item){ itemX, itemY, buildings[i].outputType };
                        buildings[i].progress = 0;
                    }
                }
            }
        }
    }
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

static void DrawBuilding(BuildingTypeEnum typeIdx, int originX, int originY, int size, int rotation, int tileSize, Color color, Building* buildings, int buildingCount, int progress, ItemTypeEnum outputType, bool drawOverlay) {
    Vector2 origin = { (float)tileSize / 2.0f, (float)tileSize / 2.0f };
    for (int dx = 0; dx < size; dx++) {
        for (int dy = 0; dy < size; dy++) {
            Rectangle dest = {
                (float)(originX + dx) * tileSize + (tileSize / 2.0f),
                (float)(originY + dy) * tileSize + (tileSize / 2.0f),
                (float)tileSize,
                (float)tileSize
            };
            DrawRectanglePro(dest, origin, (float)rotation, color);
        }
    }

    Vector2 dir = AngleToDir((float)rotation);
    float originPxX = (float)originX * tileSize;
    float originPxY = (float)originY * tileSize;
    float sizePx = (float)size * tileSize;
    Vector2 buildingCenter = { originPxX + sizePx / 2.0f, originPxY + sizePx / 2.0f };
    Color chevronColor = (Color){ 20, 20, 20, 220 };

    if (typeIdx == BUILDING_MINING_DRILL) {
        Vector2 tip = { buildingCenter.x + dir.x * tileSize, buildingCenter.y + dir.y * tileSize };
        DrawChevron(tip, tileSize * 0.18f, (float)rotation, 35.0f, 2.0f, chevronColor);
        DrawCircleV(buildingCenter, tileSize * 0.5f, (Color){ 30, 30, 30, 255 });

        if (drawOverlay) {
            DrawCircleV(buildingCenter, tileSize * 0.45f, GetItemColor(outputType));
        } else {
            DrawCircleSector(buildingCenter, tileSize * 0.45f, -90.0f, -90.0f + ((float)progress / 120.0f * 360.0f), 32, (Color){ 100, 150, 200, 255 });
        }
    } else if (typeIdx == BUILDING_TRANSPORT_BELT) {
        Building selfBelt = { .x = originX, .y = originY, .rotation = rotation };
        int inputRot = GetBeltInputRotation(&selfBelt, buildings, buildingCount);
        Vector2 inDir = AngleToDir((float)inputRot);
        Vector2 tileCenter = { originPxX + tileSize / 2.0f, originPxY + tileSize / 2.0f };
        Vector2 startTip = { tileCenter.x - inDir.x * tileSize * 0.22f, tileCenter.y - inDir.y * tileSize * 0.22f };
        Vector2 endTip = { tileCenter.x + dir.x * tileSize * 0.22f, tileCenter.y + dir.y * tileSize * 0.22f };

        DrawChevron(startTip, tileSize * 0.14f, (float)inputRot, 35.0f, 2.0f, chevronColor);
        DrawChevron(endTip, tileSize * 0.14f, (float)rotation, 35.0f, 2.0f, chevronColor);
    } else if (typeIdx == BUILDING_INSERTER) {
        Vector2 tileCenter = { originPxX + tileSize / 2.0f, originPxY + tileSize / 2.0f };
        Vector2 edgePoint = { tileCenter.x + dir.x * tileSize * 0.5f, tileCenter.y + dir.y * tileSize * 0.5f };
        DrawKinkedChevron(tileCenter, edgePoint, tileSize * 0.125f, 3.0f, chevronColor);
    }
}

static bool RemoveItemAtCursor(Vector2 mouseWorld, Item* items, int* itemCount, int tileSize) {
    float pickupRadius = tileSize * 0.125f;

    for (int i = 0; i < *itemCount; i++) {
        float dx = items[i].x - mouseWorld.x;
        float dy = items[i].y - mouseWorld.y;

        if (dx * dx + dy * dy <= pickupRadius * pickupRadius) {
            items[i] = items[*itemCount - 1];
            (*itemCount)--;

            return true;
        }
    }

    return false;
}

static bool RemoveBuildingAtCursor(int gridX, int gridY, Building* buildings, int* buildingCount) {
    int targetOriginX = -1;
    int targetOriginY = -1;
    int targetSize = -1;

    for (int i = 0; i < *buildingCount; i++) {
        if (buildings[i].x == gridX && buildings[i].y == gridY) {
            targetOriginX = buildings[i].originX;
            targetOriginY = buildings[i].originY;
            targetSize = buildings[i].size;
            break;
        }
    }

    if (targetSize == -1) {
        return false;
    }

    for (int i = *buildingCount - 1; i >= 0; i--) {
        if (buildings[i].originX == targetOriginX &&
            buildings[i].originY == targetOriginY &&
            buildings[i].size == targetSize) {

            buildings[i] = buildings[*buildingCount - 1];
            (*buildingCount)--;
        }
    }

    return true;
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

    Building* buildings = NULL;
    int buildingCount = 0;
    int buildingCapacity = 0;
    BuildingTypeEnum currentBuildingIdx = BUILDING_NONE;
    int currentHeldRotation = DIR_UP;
    ItemTypeEnum currentDrillOutputMode = ITEM_IRON_ORE;

    Item* items = NULL;
    int itemCount = 0;
    int itemCapacity = 0;

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
            game_logic_tick(buildings, buildingCount, &items, &itemCount, &itemCapacity, tileSize);
            accumulator -= UPDATE_DT_MS;
            ticks_this_frame++;
            if (ticks_this_frame >= MAX_TICKS_PER_FRAME) {
                accumulator = 0.0;
                break;
            }
        }

        for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i) && BUILDING_TYPES[i].name != NULL) currentBuildingIdx = i;
        if (IsKeyPressed(KEY_ZERO) && BUILDING_TYPES[9].name != NULL) currentBuildingIdx = 9;

        if (IsKeyPressed(KEY_O)) {
            currentDrillOutputMode = (currentDrillOutputMode + 1) % ITEM_TYPE_COUNT;
        }

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
        int originX = 0, originY = 0;

        if (currentBuildingIdx != BUILDING_NONE) {
            int size = BUILDING_TYPES[currentBuildingIdx].size;
            float offset = (size - 1) / 2.0f;
            originX = (int)floorf((mouseWorld.x / tileSize) - offset);
            originY = (int)floorf((mouseWorld.y / tileSize) - offset);

            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    int cx = originX + dx;
                    int cy = originY + dy;
                    for (int i = 0; i < buildingCount; i++) {
                        if (buildings[i].x == cx && buildings[i].y == cy) { canPlace = false; break; }
                    }
                    if (!canPlace) break;
                }
                if (!canPlace) break;
            }
        }

        if (!mouseOverToolbar && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && currentBuildingIdx != BUILDING_NONE && canPlace) {
            int size = BUILDING_TYPES[currentBuildingIdx].size;
            if (buildingCount + (size * size) > buildingCapacity) {
                buildingCapacity = (buildingCapacity == 0) ? (size * size) : buildingCapacity * 2;
                while (buildingCount + (size * size) > buildingCapacity) buildingCapacity *= 2;
                buildings = realloc(buildings, buildingCapacity * sizeof(Building));
            }
            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    int cx = originX + dx;
                    int cy = originY + dy;

                    for (int k = 0; k < itemCount; k++) {
                        if ((int)floorf(items[k].x / tileSize) == cx && (int)floorf(items[k].y / tileSize) == cy) {
                            for (int m = k; m < itemCount - 1; m++) items[m] = items[m+1];
                            itemCount--;
                            k--;
                        }
                    }

                    Building newB;
                    memset(&newB, 0, sizeof(Building));
                    newB.x = originX + dx;
                    newB.y = originY + dy;
                    newB.originX = originX;
                    newB.originY = originY;
                    newB.size = size;
                    newB.typeIdx = currentBuildingIdx;
                    newB.rotation = currentHeldRotation;
                    newB.outputType = currentDrillOutputMode;
                    for(int l=0; l<2; l++) for(int j=0; j<5; j++) newB.belt_items[l][j] = -1.0f;

                    buildings[buildingCount++] = newB;
                }
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            if (!RemoveItemAtCursor(mouseWorld, items, &itemCount, tileSize)) {
                RemoveBuildingAtCursor(gridX, gridY, buildings, &buildingCount);
            }
        }

        if (IsKeyPressed(KEY_R)) {
            int rotMod = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? ROT_CCW : ROT_CW;
            if (currentBuildingIdx != BUILDING_NONE) {
                currentHeldRotation = (currentHeldRotation + rotMod) % 360;
            } else {
                int targetOriginX = -1, targetOriginY = -1, targetSize = -1;
                for (int i = 0; i < buildingCount; i++) {
                    if (buildings[i].x == gridX && buildings[i].y == gridY) {
                        targetOriginX = buildings[i].originX;
                        targetOriginY = buildings[i].originY;
                        targetSize = buildings[i].size;
                        break;
                    }
                }
                if (targetSize != -1) {
                    for (int i = 0; i < buildingCount; i++) {
                        if (buildings[i].originX == targetOriginX && buildings[i].originY == targetOriginY && buildings[i].size == targetSize) {
                            buildings[i].rotation = (buildings[i].rotation + rotMod) % 360;
                        }
                    }
                }
            }
        }

        if (IsKeyPressed(KEY_Q)) {
            if (currentBuildingIdx != BUILDING_NONE) {
                currentBuildingIdx = BUILDING_NONE;
            } else {
                for (int i = 0; i < buildingCount; i++) {
                    if (buildings[i].x == gridX && buildings[i].y == gridY) {
                        currentBuildingIdx = buildings[i].typeIdx;
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

        for (int i = 0; i < buildingCount; i++) {
            if (buildings[i].x != buildings[i].originX || buildings[i].y != buildings[i].originY) continue;
            DrawBuilding(buildings[i].typeIdx, buildings[i].originX, buildings[i].originY, buildings[i].size, buildings[i].rotation, tileSize, BUILDING_TYPES[buildings[i].typeIdx].color, buildings, buildingCount, buildings[i].progress, buildings[i].outputType, false);
        }

        for (int i = 0; i < buildingCount; i++) {
            if (buildings[i].typeIdx == BUILDING_TRANSPORT_BELT) {
                int inRot = GetBeltInputRotation(&buildings[i], buildings, buildingCount);
                int outRot = buildings[i].rotation;
                Vector2 tileCenter = { (buildings[i].x + 0.5f) * tileSize, (buildings[i].y + 0.5f) * tileSize };

                for (int l = 0; l < 2; l++) {
                    float laneOffset = (l == 0) ? -0.25f : 0.25f;
                    for (int j = 0; j < 5; j++) {
                        float prog = buildings[i].belt_items[l][j];
                        if (prog >= 0.0f) {
                            Vector2 pos;
                            if (inRot == outRot) {
                                Vector2 dir = AngleToDir(outRot);
                                Vector2 right = { -dir.y, dir.x };
                                float pOffset = prog - 0.625f;
                                pos = (Vector2){ tileCenter.x + right.x * laneOffset * tileSize + dir.x * pOffset * tileSize, tileCenter.y + right.y * laneOffset * tileSize + dir.y * pOffset * tileSize };
                            } else {
                                int relRot = (outRot - inRot + 360) % 360;
                                bool isClockwise = (relRot == ROT_CW);

                                int idx = (inRot / 90) % 4;
                                if (!isClockwise) idx = (idx + 1) % 4;
                                static const float pivotXs[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                                static const float pivotYs[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
                                float pivotX = pivotXs[idx];
                                float pivotY = pivotYs[idx];

                                float r;
                                if (isClockwise) r = (l == 1) ? 0.25f * tileSize : 0.75f * tileSize;
                                else r = (l == 0) ? 0.25f * tileSize : 0.75f * tileSize;

                                float startAngle, endAngle;
                                if (isClockwise) {
                                    startAngle = (inRot == DIR_UP) ? 180.0f : (inRot == DIR_RIGHT) ? 270.0f : (inRot == DIR_DOWN) ? 0.0f : 90.0f;
                                    endAngle = startAngle + 90.0f;
                                } else {
                                    startAngle = (inRot == DIR_UP) ? 0.0f : (inRot == DIR_RIGHT) ? 90.0f : (inRot == DIR_DOWN) ? 180.0f : 270.0f;
                                    endAngle = startAngle - 90.0f;
                                }

                                float a = startAngle + (endAngle - startAngle) * prog;

                                Vector2 tileTopLeft = { (float)buildings[i].x * tileSize, (float)buildings[i].y * tileSize };
                                pos = (Vector2){ (tileTopLeft.x + pivotX * tileSize) + cosf(a * DEG2RAD) * r,
                                                (tileTopLeft.y + pivotY * tileSize) + sinf(a * DEG2RAD) * r };
                            }
                            DrawCircleV(pos, tileSize * 0.125f, GetItemColor(buildings[i].belt_item_types[l][j]));
                        }
                    }
                }
            }
        }

        for (int i = 0; i < itemCount; i++) {
            DrawCircleV((Vector2){ items[i].x, items[i].y }, tileSize * 0.125f, GetItemColor(items[i].type));
        }

        if (!mouseOverToolbar && currentBuildingIdx != BUILDING_NONE) {
            Color base = BUILDING_TYPES[currentBuildingIdx].color;
            Color tint = canPlace ? (Color){ base.r, base.g, base.b, 150 } : (Color){ 255, 0, 0, 150 };
            DrawBuilding(currentBuildingIdx, originX, originY, BUILDING_TYPES[currentBuildingIdx].size, currentHeldRotation, tileSize, tint, buildings, buildingCount, 0, currentDrillOutputMode, true);
        }

        EndMode2D();

        DrawText(TextFormat("Building Coord: (%d, %d)", gridX, gridY), 10, 10, 20, RAYWHITE);
        DrawText(TextFormat("Total Buildings: %d", buildingCount), 10, 35, 20, GRAY);
        DrawText(TextFormat("Zoom: %.2fx", camera.zoom), 10, 60, 20, GRAY);

        float fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
        float ups = (dt > 0.0f) ? ((float)ticks_this_frame / dt) : 0.0f;
        const char* perf_text = TextFormat("FPS/UPS = %.1f/%.1f", fps, ups);
        DrawText(perf_text, screenWidth - MeasureText(perf_text, 20) - 10, 10, 20, RAYWHITE);

        int barW = 500;
        int barH = 50;
        int startXPos = (screenWidth - barW) / 2;
        int startYPos = screenHeight - barH - 10;

        int itemSize = 40;
        int slotWidth = barW / 10;

        DrawRectangle(startXPos, startYPos, barW, barH, (Color){ 30, 30, 30, 255 });

        for (int i = 0; i < 10; i++) {
            int x = startXPos + i * slotWidth + (slotWidth - itemSize) / 2;
            int y = startYPos + (barH - itemSize) / 2;

            if (BUILDING_TYPES[i].name != NULL) {
                DrawRectangle(x, y, itemSize, itemSize, BUILDING_TYPES[i].color);
            } else {
                DrawRectangle(x, y, itemSize, itemSize, (Color){ 45, 45, 45, 255 });
            }

            if (i == currentBuildingIdx) {
                DrawRectangleLines(x - 2, y - 2, itemSize + 4, itemSize + 4, WHITE);

                const char* heldName = BUILDING_TYPES[i].name;
                int textW = MeasureText(heldName, 14);
                DrawText(heldName, x + itemSize / 2 - textW / 2, y - 16, 14, RAYWHITE);
            }
        }

        EndDrawing();
    }

    free(buildings);
    free(items);
    CloseWindow();
}
