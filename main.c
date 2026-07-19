#include "cJSON.h"
#include "grug.h"
#include "raylib.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
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
} building_type_e;

typedef enum {
    ITEM_IRON_ORE = 0,
    ITEM_COAL = 1,
    ITEM_COPPER_ORE = 2,
    ITEM_TYPE_COUNT = 3
} item_type_e;

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT = 90,
    DIR_DOWN = 180,
    DIR_LEFT = 270
} direction_e;

typedef enum {
    ROT_CW = 90,
    ROT_CCW = 270
} rotation_e;

typedef struct {
    const char* name;
    Color color;
    int size;
} building_type_def_t;

static const building_type_def_t BUILDING_TYPES[10] = {
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
    int origin_x;
    int origin_y;
    int size;
    building_type_e type_idx;
    int rotation;
    int progress;
    float belt_items[2][5];
    item_type_e belt_item_types[2][5];
    item_type_e output_type;
} building_t;

typedef struct {
    float x;
    float y;
    item_type_e type;
} item_t;

static Color get_item_color(item_type_e type) {
    switch (type) {
        case ITEM_IRON_ORE: return (Color){ 100, 150, 200, 255 };
        case ITEM_COAL: return (Color){ 50, 50, 50, 255 };
        case ITEM_COPPER_ORE: return (Color){ 200, 150, 100, 255 };
        case ITEM_TYPE_COUNT: abort();
    }
    abort();
}

static bool is_tile_occupied(int x, int y, building_t* buildings, int count)
{
    for (int i = 0; i < count; i++) {
        building_t* b = &buildings[i];

        if (x >= b->origin_x &&
            x < b->origin_x + b->size &&
            y >= b->origin_y &&
            y < b->origin_y + b->size) {
            return true;
        }
    }

    return false;
}

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

void save_world(const char* path, building_t* buildings, int building_count, item_t* items, int item_count, Camera2D camera) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);

    cJSON_AddNumberToObject(root, "camera_target_x", camera.target.x);
    cJSON_AddNumberToObject(root, "camera_target_y", camera.target.y);
    cJSON_AddNumberToObject(root, "camera_offset_x", camera.offset.x);
    cJSON_AddNumberToObject(root, "camera_offset_y", camera.offset.y);
    cJSON_AddNumberToObject(root, "camera_zoom", camera.zoom);

    cJSON *b_array = cJSON_CreateArray();
    for (int i = 0; i < building_count; i++) {
        cJSON *b_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(b_obj, "x", buildings[i].x);
        cJSON_AddNumberToObject(b_obj, "y", buildings[i].y);
        cJSON_AddNumberToObject(b_obj, "origin_x", buildings[i].origin_x);
        cJSON_AddNumberToObject(b_obj, "origin_y", buildings[i].origin_y);
        cJSON_AddNumberToObject(b_obj, "size", buildings[i].size);
        cJSON_AddNumberToObject(b_obj, "type_idx", buildings[i].type_idx);
        cJSON_AddNumberToObject(b_obj, "rotation", buildings[i].rotation);
        cJSON_AddNumberToObject(b_obj, "progress", buildings[i].progress);
        cJSON_AddNumberToObject(b_obj, "output_type", buildings[i].output_type);

        cJSON *items_array = cJSON_CreateArray();
        for (int l = 0; l < 2; l++) {
            for (int j = 0; j < 5; j++) {
                cJSON_AddItemToArray(items_array, cJSON_CreateNumber(buildings[i].belt_items[l][j]));
            }
        }
        cJSON_AddItemToObject(b_obj, "belt_items", items_array);

        cJSON *types_array = cJSON_CreateArray();
        for (int l = 0; l < 2; l++) {
            for (int j = 0; j < 5; j++) {
                cJSON_AddItemToArray(types_array, cJSON_CreateNumber(buildings[i].belt_item_types[l][j]));
            }
        }
        cJSON_AddItemToObject(b_obj, "belt_item_types", types_array);

        cJSON_AddItemToArray(b_array, b_obj);
    }
    cJSON_AddItemToObject(root, "buildings", b_array);

    cJSON *item_array = cJSON_CreateArray();
    for (int i = 0; i < item_count; i++) {
        cJSON *item_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(item_obj, "x", (double)items[i].x);
        cJSON_AddNumberToObject(item_obj, "y", (double)items[i].y);
        cJSON_AddNumberToObject(item_obj, "type", items[i].type);
        cJSON_AddItemToArray(item_array, item_obj);
    }
    cJSON_AddItemToObject(root, "items", item_array);

    char *out = cJSON_Print(root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", out);
        fclose(f);
    }
    free(out);
    cJSON_Delete(root);
}

void load_world(const char* path, building_t** buildings, int* building_count, int* building_capacity, item_t** items, int* item_count, int* item_capacity, Camera2D* camera) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: --input-save could not open %s\n", path);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(data);
    if (!root) { free(data); return; }

    cJSON *ct_x = cJSON_GetObjectItem(root, "camera_target_x");
    if (ct_x) camera->target.x = (float)ct_x->valuedouble;
    cJSON *ct_y = cJSON_GetObjectItem(root, "camera_target_y");
    if (ct_y) camera->target.y = (float)ct_y->valuedouble;
    cJSON *co_x = cJSON_GetObjectItem(root, "camera_offset_x");
    if (co_x) camera->offset.x = (float)co_x->valuedouble;
    cJSON *co_y = cJSON_GetObjectItem(root, "camera_offset_y");
    if (co_y) camera->offset.y = (float)co_y->valuedouble;
    cJSON *cz = cJSON_GetObjectItem(root, "camera_zoom");
    if (cz) camera->zoom = (float)cz->valuedouble;

    cJSON *b_array = cJSON_GetObjectItem(root, "buildings");
    int count = cJSON_GetArraySize(b_array);

    *building_count = count;
    *building_capacity = count;
    *buildings = realloc(*buildings, sizeof(building_t) * (*building_capacity));

    for (int i = 0; i < count; i++) {
        cJSON *b_obj = cJSON_GetArrayItem(b_array, i);
        (*buildings)[i].x = cJSON_GetObjectItem(b_obj, "x")->valueint;
        (*buildings)[i].y = cJSON_GetObjectItem(b_obj, "y")->valueint;
        (*buildings)[i].origin_x = cJSON_GetObjectItem(b_obj, "origin_x")->valueint;
        (*buildings)[i].origin_y = cJSON_GetObjectItem(b_obj, "origin_y")->valueint;
        (*buildings)[i].size = cJSON_GetObjectItem(b_obj, "size")->valueint;
        (*buildings)[i].type_idx = cJSON_GetObjectItem(b_obj, "type_idx")->valueint;
        (*buildings)[i].rotation = cJSON_GetObjectItem(b_obj, "rotation")->valueint;
        (*buildings)[i].progress = cJSON_GetObjectItem(b_obj, "progress")->valueint;
        (*buildings)[i].output_type = cJSON_GetObjectItem(b_obj, "output_type")->valueint;

        cJSON *items_array = cJSON_GetObjectItem(b_obj, "belt_items");
        for (int l = 0; l < 2; l++) {
            for (int j = 0; j < 5; j++) {
                (*buildings)[i].belt_items[l][j] = cJSON_GetArrayItem(items_array, l * 5 + j)->valuedouble;
            }
        }

        cJSON *types_array = cJSON_GetObjectItem(b_obj, "belt_item_types");
        for (int l = 0; l < 2; l++) {
            for (int j = 0; j < 5; j++) {
                (*buildings)[i].belt_item_types[l][j] = cJSON_GetArrayItem(types_array, l * 5 + j)->valueint;
            }
        }
    }

    cJSON *item_array = cJSON_GetObjectItem(root, "items");
    if (item_array) {
        int count = cJSON_GetArraySize(item_array);
        *item_count = count;
        *item_capacity = count;
        *items = realloc(*items, sizeof(item_t) * (*item_capacity));
        for (int i = 0; i < count; i++) {
            cJSON *item_obj = cJSON_GetArrayItem(item_array, i);
            (*items)[i].x = (float)cJSON_GetObjectItem(item_obj, "x")->valuedouble;
            (*items)[i].y = (float)cJSON_GetObjectItem(item_obj, "y")->valuedouble;
            (*items)[i].type = cJSON_GetObjectItem(item_obj, "type")->valueint;
        }
    }

    cJSON_Delete(root);
    free(data);
}

static int get_feeder_rotation(building_t* belt, building_t* buildings, int building_count) {
    int dirs[4] = { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT };
    for (int i = 0; i < 4; i++) {
        int rot = dirs[i];
        int dx = (int)roundf(sinf(rot * DEG2RAD));
        int dy = (int)roundf(-cosf(rot * DEG2RAD));
        int check_x = belt->x - dx;
        int check_y = belt->y - dy;
        for (int b = 0; b < building_count; b++) {
            if (buildings[b].type_idx == BUILDING_TRANSPORT_BELT && buildings[b].x == check_x && buildings[b].y == check_y && buildings[b].rotation == rot) {
                return rot;
            }
        }
    }
    return -1;
}

static bool has_belt_feeding_from(int target_x, int target_y, int feeder_rot, building_t* buildings, int count) {
    if (!buildings) return false;
    int dx = (int)roundf(sinf(feeder_rot * DEG2RAD));
    int dy = (int)roundf(-cosf(feeder_rot * DEG2RAD));
    int feeder_x = target_x - dx;
    int feeder_y = target_y - dy;
    for (int i = 0; i < count; i++) {
        if (buildings[i].type_idx == BUILDING_TRANSPORT_BELT && buildings[i].x == feeder_x && buildings[i].y == feeder_y && buildings[i].rotation == feeder_rot) {
            return true;
        }
    }
    return false;
}

static int get_belt_input_rotation(building_t* belt, building_t* buildings, int count) {
    int rot = belt->rotation;
    if (has_belt_feeding_from(belt->x, belt->y, rot, buildings, count)) return rot;
    bool has_left = has_belt_feeding_from(belt->x, belt->y, (rot + ROT_CW) % 360, buildings, count);
    bool has_right = has_belt_feeding_from(belt->x, belt->y, (rot + ROT_CCW) % 360, buildings, count);
    if (has_left && !has_right) return (rot + ROT_CW) % 360;
    if (has_right && !has_left) return (rot + ROT_CCW) % 360;
    return rot;
}

static int get_belt_lane_capacity(building_t* belt, int lane, building_t* buildings, int building_count) {
    int feeder_rot = get_feeder_rotation(belt, buildings, building_count);
    if (feeder_rot != -1) {
        int rel_rot = (belt->rotation - feeder_rot + 360) % 360;
        if (rel_rot == 0) return 4;
        if (rel_rot == ROT_CW) return (lane == 1) ? 2 : 5;
        if (rel_rot == ROT_CCW) return (lane == 0) ? 2 : 5;
    }
    return 4;
}

static Vector2 angle_to_dir(float angle_deg) {
    float rad = angle_deg * DEG2RAD;
    return (Vector2){ sinf(rad), -cosf(rad) };
}

static void game_logic_tick(building_t* buildings, int building_count, item_t** items, int* item_count, int* item_capacity, int tile_size) {
    for (int i = 0; i < building_count; i++) {
        if (buildings[i].type_idx == BUILDING_TRANSPORT_BELT) {
            for (int l = 0; l < 2; l++) {
                int capacity = get_belt_lane_capacity(&buildings[i], l, buildings, building_count);
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

    for (int i = 0; i < building_count; i++) {
        if (buildings[i].type_idx == BUILDING_TRANSPORT_BELT) {
            for (int l = 0; l < 2; l++) {
                if (buildings[i].belt_items[l][0] >= 1.0f) {
                    int dir_x = (int)roundf(sinf(buildings[i].rotation * DEG2RAD));
                    int dir_y = (int)roundf(-cosf(buildings[i].rotation * DEG2RAD));
                    int tx = buildings[i].x + dir_x;
                    int ty = buildings[i].y + dir_y;

                    int target_belt_idx = -1;
                    for (int k = 0; k < building_count; k++) {
                        if (buildings[k].type_idx == BUILDING_TRANSPORT_BELT && buildings[k].x == tx && buildings[k].y == ty) {
                            target_belt_idx = k;
                            break;
                        }
                    }

                    if (target_belt_idx != -1) {
                        Vector2 t_dir = angle_to_dir((float)buildings[target_belt_idx].rotation);
                        if (buildings[target_belt_idx].x + (int)t_dir.x == buildings[i].x && buildings[target_belt_idx].y + (int)t_dir.y == buildings[i].y) continue;

                        int target_lane = l;

                        int capacity = get_belt_lane_capacity(&buildings[target_belt_idx], target_lane, buildings, building_count);
                        int last_item_idx = -1;
                        for (int j = 0; j < capacity; j++) {
                            if (buildings[target_belt_idx].belt_items[target_lane][j] >= 0.0f) last_item_idx = j;
                        }

                        if (last_item_idx < capacity - 1 && (last_item_idx == -1 || buildings[target_belt_idx].belt_items[target_lane][last_item_idx] >= 0.25f)) {
                            buildings[target_belt_idx].belt_items[target_lane][last_item_idx + 1] = 0.0f;
                            buildings[target_belt_idx].belt_item_types[target_lane][last_item_idx + 1] = buildings[i].belt_item_types[l][0];

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

    for (int i = 0; i < building_count; i++) {
        if (buildings[i].type_idx == BUILDING_MINING_DRILL) {
            if (buildings[i].progress < 120) buildings[i].progress++;
            if (buildings[i].progress >= 120) {
                int dir_x = (int)roundf(sinf(buildings[i].rotation * DEG2RAD));
                int dir_y = (int)roundf(-cosf(buildings[i].rotation * DEG2RAD));
                int center_x = buildings[i].origin_x + buildings[i].size / 2;
                int center_y = buildings[i].origin_y + buildings[i].size / 2;

                int tx = center_x + dir_x * 2;
                int ty = center_y + dir_y * 2;

                int target_belt_idx = -1;
                for (int j = 0; j < building_count; j++) {
                    if (buildings[j].type_idx == BUILDING_TRANSPORT_BELT && buildings[j].x == tx && buildings[j].y == ty) {
                        target_belt_idx = j;
                        break;
                    }
                }

                if (target_belt_idx != -1) {
                    int rel_rot = (buildings[target_belt_idx].rotation - buildings[i].rotation + 360) % 360;
                    int l = (rel_rot == ROT_CCW) ? 0 : 1;

                    float insert_progress = (rel_rot == 0) ? 0.25f : 0.5f;

                    int capacity = get_belt_lane_capacity(&buildings[target_belt_idx], l, buildings, building_count);
                    int last_item_idx = -1;
                    for (int j = 0; j < capacity; j++) {
                        if (buildings[target_belt_idx].belt_items[l][j] >= 0.0f)
                            last_item_idx = j;
                    }

                    if (last_item_idx < capacity - 1 &&
                        (last_item_idx == -1 ||
                        buildings[target_belt_idx].belt_items[l][last_item_idx] >= insert_progress + 0.25f)) {

                        buildings[target_belt_idx].belt_items[l][last_item_idx + 1] = insert_progress;
                        buildings[target_belt_idx].belt_item_types[l][last_item_idx + 1] = buildings[i].output_type;
                        buildings[i].progress = 0;
                    }
                } else {
                    bool building_collision = is_tile_occupied(tx, ty, buildings, building_count);
                    bool item_collision = false;
                    for (int k = 0; k < *item_count; k++) {
                        if ((int)floorf((*items)[k].x / tile_size) == tx && (int)floorf((*items)[k].y / tile_size) == ty) {
                            item_collision = true; break;
                        }
                    }

                    if (!item_collision && !building_collision) {
                        float item_x = (tx + 0.5f - (float)dir_x * 0.152f) * tile_size;
                        float item_y = (ty + 0.5f - (float)dir_y * 0.152f) * tile_size;
                        if (*item_count >= *item_capacity) {
                            *item_capacity = (*item_capacity == 0) ? 64 : *item_capacity * 2;
                            *items = realloc(*items, *item_capacity * sizeof(item_t));
                        }
                        (*items)[(*item_count)++] = (item_t){ item_x, item_y, buildings[i].output_type };
                        buildings[i].progress = 0;
                    }
                }
            }
        }
    }
}

static void draw_chevron(Vector2 tip, float arm_length, float angle_deg, float spread_deg, float thickness, Color color) {
    float back_angle = angle_deg + 180.0f;
    Vector2 arm1 = angle_to_dir(back_angle - spread_deg);
    Vector2 arm2 = angle_to_dir(back_angle + spread_deg);
    Vector2 p1 = { tip.x + arm1.x * arm_length, tip.y + arm1.y * arm_length };
    Vector2 p2 = { tip.x + arm2.x * arm_length, tip.y + arm2.y * arm_length };
    DrawLineEx(tip, p1, thickness, color);
    DrawLineEx(tip, p2, thickness, color);
}

static void draw_kinked_chevron(Vector2 a, Vector2 c, float kink_offset, float thickness, Color color) {
    Vector2 d = { c.x - a.x, c.y - a.y };
    Vector2 perp = { -d.y, d.x };
    float len = sqrtf(perp.x * perp.x + perp.y * perp.y);
    if (len > 0.0f) {
        perp.x /= len;
        perp.y /= len;
    }
    Vector2 mid = { (a.x + c.x) / 2.0f + perp.x * kink_offset, (a.y + c.y) / 2.0f + perp.y * kink_offset };
    DrawLineEx(a, mid, thickness, color);
    DrawLineEx(mid, c, thickness, color);
}

static void draw_building(building_type_e type_idx, int origin_x, int origin_y, int size, int rotation, int tile_size, Color color, building_t* buildings, int building_count, int progress, item_type_e output_type, bool draw_overlay) {
    Vector2 origin = { (float)tile_size / 2.0f, (float)tile_size / 2.0f };
    for (int dx = 0; dx < size; dx++) {
        for (int dy = 0; dy < size; dy++) {
            Rectangle dest = {
                (float)(origin_x + dx) * tile_size + (tile_size / 2.0f),
                (float)(origin_y + dy) * tile_size + (tile_size / 2.0f),
                (float)tile_size,
                (float)tile_size
            };
            DrawRectanglePro(dest, origin, (float)rotation, color);
        }
    }

    Vector2 dir = angle_to_dir((float)rotation);
    float origin_px_x = (float)origin_x * tile_size;
    float origin_px_y = (float)origin_y * tile_size;
    float size_px = (float)size * tile_size;
    Vector2 building_center = { origin_px_x + size_px / 2.0f, origin_px_y + size_px / 2.0f };
    Color chevron_color = (Color){ 20, 20, 20, 220 };

    if (type_idx == BUILDING_MINING_DRILL) {
        Vector2 tip = { building_center.x + dir.x * tile_size, building_center.y + dir.y * tile_size };
        draw_chevron(tip, tile_size * 0.18f, (float)rotation, 35.0f, 2.0f, chevron_color);
        DrawCircleV(building_center, tile_size * 0.5f, (Color){ 30, 30, 30, 255 });

        if (draw_overlay) {
            DrawCircleV(building_center, tile_size * 0.45f, get_item_color(output_type));
        } else {
            DrawCircleSector(building_center, tile_size * 0.45f, -90.0f, -90.0f + ((float)progress / 120.0f * 360.0f), 32, (Color){ 100, 150, 200, 255 });
        }
    } else if (type_idx == BUILDING_TRANSPORT_BELT) {
        building_t self_belt = { .x = origin_x, .y = origin_y, .rotation = rotation };
        int input_rot = get_belt_input_rotation(&self_belt, buildings, building_count);
        Vector2 in_dir = angle_to_dir((float)input_rot);
        Vector2 tile_center = { origin_px_x + tile_size / 2.0f, origin_px_y + tile_size / 2.0f };
        Vector2 start_tip = { tile_center.x - in_dir.x * tile_size * 0.22f, tile_center.y - in_dir.y * tile_size * 0.22f };
        Vector2 end_tip = { tile_center.x + dir.x * tile_size * 0.22f, tile_center.y + dir.y * tile_size * 0.22f };

        draw_chevron(start_tip, tile_size * 0.14f, (float)input_rot, 35.0f, 2.0f, chevron_color);
        draw_chevron(end_tip, tile_size * 0.14f, (float)rotation, 35.0f, 2.0f, chevron_color);
    } else if (type_idx == BUILDING_INSERTER) {
        Vector2 tile_center = { origin_px_x + tile_size / 2.0f, origin_px_y + tile_size / 2.0f };
        Vector2 edge_point = { tile_center.x + dir.x * tile_size * 0.5f, tile_center.y + dir.y * tile_size * 0.5f };
        draw_kinked_chevron(tile_center, edge_point, tile_size * 0.125f, 3.0f, chevron_color);
    }
}

static bool remove_item_at_cursor(Vector2 mouse_world, item_t* items, int* item_count, int tile_size) {
    float pickup_radius = tile_size * 0.125f;

    for (int i = 0; i < *item_count; i++) {
        float dx = items[i].x - mouse_world.x;
        float dy = items[i].y - mouse_world.y;

        if (dx * dx + dy * dy <= pickup_radius * pickup_radius) {
            items[i] = items[*item_count - 1];
            (*item_count)--;

            return true;
        }
    }

    return false;
}

static bool remove_building_at_cursor(
    int grid_x,
    int grid_y,
    building_t* buildings,
    int* building_count)
{
    for (int i = 0; i < *building_count; i++) {

        building_t* b = &buildings[i];

        if (grid_x >= b->origin_x &&
            grid_x < b->origin_x + b->size &&
            grid_y >= b->origin_y &&
            grid_y < b->origin_y + b->size)
        {
            buildings[i] = buildings[*building_count - 1];
            (*building_count)--;
            return true;
        }
    }

    return false;
}

int main(int argc, char** argv) {
    const int tile_size = 64;
    building_t* buildings = NULL;
    int building_count = 0;
    int building_capacity = 0;
    building_type_e current_building_idx = BUILDING_NONE;
    int current_held_rotation = DIR_UP;
    item_type_e current_drill_output_mode = ITEM_IRON_ORE;
    item_t* items = NULL;
    int item_count = 0;
    int item_capacity = 0;

    Camera2D camera = { 0 };
    camera.target = (Vector2){ 0.0f, 0.0f };
    camera.offset = (Vector2){ 0, 0 };
    camera.zoom = 1.0f;

    char* output_save_path = NULL;
    int run_ticks = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input-save") == 0 && i + 1 < argc) {
            load_world(argv[++i], &buildings, &building_count, &building_capacity, &items, &item_count, &item_capacity, &camera);
        } else if (strcmp(argv[i], "--output-save") == 0 && i + 1 < argc) {
            output_save_path = argv[++i];
        } else if (strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            run_ticks = atoi(argv[++i]);
        }
    }

    if (run_ticks > 0) {
        for (int t = 0; t < run_ticks; t++) {
            game_logic_tick(buildings, building_count, &items, &item_count, &item_capacity, tile_size);
        }
        if (output_save_path) save_world(output_save_path, buildings, building_count, items, item_count, camera);
        goto cleanup;
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(0, 0, "grugtorio");

    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    if (camera.offset.x == 0) camera.offset = (Vector2){ screen_width / 2.0f, screen_height / 2.0f };

    SetTargetFPS(60);

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
            game_logic_tick(buildings, building_count, &items, &item_count, &item_capacity, tile_size);
            accumulator -= UPDATE_DT_MS;
            ticks_this_frame++;
            if (ticks_this_frame >= MAX_TICKS_PER_FRAME) {
                accumulator = 0.0;
                break;
            }
        }

        for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i) && BUILDING_TYPES[i].name != NULL) current_building_idx = i;
        if (IsKeyPressed(KEY_ZERO) && BUILDING_TYPES[9].name != NULL) current_building_idx = 9;

        if (IsKeyPressed(KEY_O)) {
            current_drill_output_mode = (current_drill_output_mode + 1) % ITEM_TYPE_COUNT;
        }

        float move_speed = 500.0f / camera.zoom * dt;
        if (IsKeyDown(KEY_W)) camera.target.y -= move_speed;
        if (IsKeyDown(KEY_S)) camera.target.y += move_speed;
        if (IsKeyDown(KEY_A)) camera.target.x -= move_speed;
        if (IsKeyDown(KEY_D)) camera.target.x += move_speed;
        if (IsKeyPressed(KEY_F)) ToggleFullscreen();

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            Vector2 mouse_world_pos = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.zoom += wheel * 0.125f;
            if (camera.zoom < 0.25f) camera.zoom = 0.25f;
            if (camera.zoom > 2.0f) camera.zoom = 2.0f;
            Vector2 mouse_world_pos_new = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.target.x += (mouse_world_pos.x - mouse_world_pos_new.x);
            camera.target.y += (mouse_world_pos.y - mouse_world_pos_new.y);
        }

        Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), camera);
        int grid_x = (int)floorf(mouse_world.x / tile_size);
        int grid_y = (int)floorf(mouse_world.y / tile_size);

        bool mouse_over_toolbar = (GetMouseY() > screen_height - 70);
        bool can_place = true;
        int origin_x = 0, origin_y = 0;

        if (current_building_idx != BUILDING_NONE) {
            int size = BUILDING_TYPES[current_building_idx].size;
            float offset = (size - 1) / 2.0f;
            origin_x = (int)floorf((mouse_world.x / tile_size) - offset);
            origin_y = (int)floorf((mouse_world.y / tile_size) - offset);

            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    int cx = origin_x + dx;
                    int cy = origin_y + dy;
                    if (is_tile_occupied(cx, cy, buildings, building_count)) {
                        can_place = false; break;
                    }
                }
                if (!can_place) break;
            }
        }

        if (!mouse_over_toolbar && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && current_building_idx != BUILDING_NONE && can_place) {
            int size = BUILDING_TYPES[current_building_idx].size;
            if (building_count + 1 > building_capacity) {
                building_capacity = (building_capacity == 0) ? 1 : building_capacity * 2;
                buildings = realloc(buildings, building_capacity * sizeof(building_t));
            }

            for (int dx = 0; dx < size; dx++) {
                for (int dy = 0; dy < size; dy++) {
                    int cx = origin_x + dx;
                    int cy = origin_y + dy;
                    for (int k = 0; k < item_count; k++) {
                        if ((int)floorf(items[k].x / tile_size) == cx && (int)floorf(items[k].y / tile_size) == cy) {
                            for (int m = k; m < item_count - 1; m++) items[m] = items[m+1];
                            item_count--;
                            k--;
                        }
                    }
                }
            }

            building_t new_b;
            memset(&new_b, 0, sizeof(building_t));

            new_b.x = origin_x;
            new_b.y = origin_y;
            new_b.origin_x = origin_x;
            new_b.origin_y = origin_y;
            new_b.size = size;
            new_b.type_idx = current_building_idx;
            new_b.rotation = current_held_rotation;
            new_b.output_type = current_drill_output_mode;

            for (int l = 0; l < 2; l++)
                for (int j = 0; j < 5; j++)
                    new_b.belt_items[l][j] = -1.0f;

            buildings[building_count++] = new_b;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            if (!remove_item_at_cursor(mouse_world, items, &item_count, tile_size)) {
                remove_building_at_cursor(grid_x, grid_y, buildings, &building_count);
            }
        }

        if (IsKeyPressed(KEY_R)) {
            int rot_mod = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? ROT_CCW : ROT_CW;
            if (current_building_idx != BUILDING_NONE) {
                current_held_rotation = (current_held_rotation + rot_mod) % 360;
            } else {
                for (int i = 0; i < building_count; i++) {
                    if (grid_x >= buildings[i].origin_x && grid_x < buildings[i].origin_x + buildings[i].size &&
                        grid_y >= buildings[i].origin_y && grid_y < buildings[i].origin_y + buildings[i].size) {
                            buildings[i].rotation = (buildings[i].rotation + rot_mod) % 360;
                    }
                }
            }
        }

        if (IsKeyPressed(KEY_Q)) {
            if (current_building_idx != BUILDING_NONE) {
                current_building_idx = BUILDING_NONE;
            } else {
                for (int i = 0; i < building_count; i++) {
                    if (grid_x >= buildings[i].origin_x && grid_x < buildings[i].origin_x + buildings[i].size &&
                        grid_y >= buildings[i].origin_y && grid_y < buildings[i].origin_y + buildings[i].size) {
                        current_building_idx = buildings[i].type_idx;
                        break;
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground((Color){ 20, 20, 20, 255 });

        BeginMode2D(camera);

        Vector2 screen_top_left = GetScreenToWorld2D((Vector2){ 0, 0 }, camera);
        Vector2 screen_bottom_right = GetScreenToWorld2D((Vector2){ screen_width, screen_height }, camera);
        int start_x = (int)floorf(screen_top_left.x / tile_size) - 1;
        int end_x = (int)floorf(screen_bottom_right.x / tile_size) + 1;
        int start_y = (int)floorf(screen_top_left.y / tile_size) - 1;
        int end_y = (int)floorf(screen_bottom_right.y / tile_size) + 1;

        for (int x = start_x; x <= end_x; x++) DrawLine(x * tile_size, start_y * tile_size, x * tile_size, end_y * tile_size, (Color){ 45, 45, 45, 255 });
        for (int y = start_y; y <= end_y; y++) DrawLine(start_x * tile_size, y * tile_size, end_x * tile_size, y * tile_size, (Color){ 45, 45, 45, 255 });

        for (int i = 0; i < building_count; i++) {
            draw_building(
                buildings[i].type_idx,
                buildings[i].origin_x,
                buildings[i].origin_y,
                buildings[i].size,
                buildings[i].rotation,
                tile_size,
                BUILDING_TYPES[buildings[i].type_idx].color,
                buildings,
                building_count,
                buildings[i].progress,
                buildings[i].output_type,
                false
            );
        }

        for (int i = 0; i < building_count; i++) {
            if (buildings[i].type_idx == BUILDING_TRANSPORT_BELT) {
                int in_rot = get_belt_input_rotation(&buildings[i], buildings, building_count);
                int out_rot = buildings[i].rotation;
                Vector2 tile_center = { (buildings[i].x + 0.5f) * tile_size, (buildings[i].y + 0.5f) * tile_size };

                for (int l = 0; l < 2; l++) {
                    float lane_offset = (l == 0) ? -0.25f : 0.25f;
                    for (int j = 0; j < 5; j++) {
                        float prog = buildings[i].belt_items[l][j];
                        if (prog >= 0.0f) {
                            Vector2 pos;
                            if (in_rot == out_rot) {
                                Vector2 dir = angle_to_dir(out_rot);
                                Vector2 right = { -dir.y, dir.x };
                                float p_offset = prog - 0.625f;
                                pos = (Vector2){ tile_center.x + right.x * lane_offset * tile_size + dir.x * p_offset * tile_size, tile_center.y + right.y * lane_offset * tile_size + dir.y * p_offset * tile_size };
                            } else {
                                int rel_rot = (out_rot - in_rot + 360) % 360;
                                bool is_clockwise = (rel_rot == ROT_CW);

                                int idx = (in_rot / 90) % 4;
                                if (!is_clockwise) idx = (idx + 1) % 4;
                                static const float pivot_xs[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                                static const float pivot_ys[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
                                float pivot_x = pivot_xs[idx];
                                float pivot_y = pivot_ys[idx];

                                float r;
                                if (is_clockwise) r = (l == 1) ? 0.25f * tile_size : 0.75f * tile_size;
                                else r = (l == 0) ? 0.25f * tile_size : 0.75f * tile_size;

                                float start_angle, end_angle;
                                if (is_clockwise) {
                                    start_angle = (in_rot == DIR_UP) ? 180.0f : (in_rot == DIR_RIGHT) ? 270.0f : (in_rot == DIR_DOWN) ? 0.0f : 90.0f;
                                    end_angle = start_angle + 90.0f;
                                } else {
                                    start_angle = (in_rot == DIR_UP) ? 0.0f : (in_rot == DIR_RIGHT) ? 90.0f : (in_rot == DIR_DOWN) ? 180.0f : 270.0f;
                                    end_angle = start_angle - 90.0f;
                                }

                                float a = start_angle + (end_angle - start_angle) * prog;

                                Vector2 tile_top_left = { (float)buildings[i].x * tile_size, (float)buildings[i].y * tile_size };
                                pos = (Vector2){ (tile_top_left.x + pivot_x * tile_size) + cosf(a * DEG2RAD) * r,
                                                (tile_top_left.y + pivot_y * tile_size) + sinf(a * DEG2RAD) * r };
                            }
                            DrawCircleV(pos, tile_size * 0.125f, get_item_color(buildings[i].belt_item_types[l][j]));
                        }
                    }
                }
            }
        }

        for (int i = 0; i < item_count; i++) {
            DrawCircleV((Vector2){ items[i].x, items[i].y }, tile_size * 0.125f, get_item_color(items[i].type));
        }

        if (!mouse_over_toolbar && current_building_idx != BUILDING_NONE) {
            Color base = BUILDING_TYPES[current_building_idx].color;
            Color tint = can_place ? (Color){ base.r, base.g, base.b, 150 } : (Color){ 255, 0, 0, 150 };
            draw_building(current_building_idx, origin_x, origin_y, BUILDING_TYPES[current_building_idx].size, current_held_rotation, tile_size, tint, buildings, building_count, 0, current_drill_output_mode, true);
        }

        EndMode2D();

        DrawText(TextFormat("Building Coord: (%d, %d)", grid_x, grid_y), 10, 10, 20, RAYWHITE);
        DrawText(TextFormat("Total Buildings: %d", building_count), 10, 35, 20, GRAY);
        DrawText(TextFormat("Zoom: %.2fx", camera.zoom), 10, 60, 20, GRAY);

        float fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
        float ups = (dt > 0.0f) ? ((float)ticks_this_frame / dt) : 0.0f;
        const char* perf_text = TextFormat("FPS/UPS = %.1f/%.1f", fps, ups);
        DrawText(perf_text, screen_width - MeasureText(perf_text, 20) - 10, 10, 20, RAYWHITE);

        int bar_w = 500;
        int bar_h = 50;
        int start_x_pos = (screen_width - bar_w) / 2;
        int start_y_pos = screen_height - bar_h - 10;

        int item_size = 40;
        int slot_width = bar_w / 10;

        DrawRectangle(start_x_pos, start_y_pos, bar_w, bar_h, (Color){ 30, 30, 30, 255 });

        for (int i = 0; i < 10; i++) {
            int x = start_x_pos + i * slot_width + (slot_width - item_size) / 2;
            int y = start_y_pos + (bar_h - item_size) / 2;

            if (BUILDING_TYPES[i].name != NULL) {
                DrawRectangle(x, y, item_size, item_size, BUILDING_TYPES[i].color);
            } else {
                DrawRectangle(x, y, item_size, item_size, (Color){ 45, 45, 45, 255 });
            }

            if (i == current_building_idx) {
                DrawRectangleLines(x - 2, y - 2, item_size + 4, item_size + 4, WHITE);

                const char* held_name = BUILDING_TYPES[i].name;
                int text_w = MeasureText(held_name, 14);
                DrawText(held_name, x + item_size / 2 - text_w / 2, y - 16, 14, RAYWHITE);
            }
        }

        EndDrawing();
    }

    if (output_save_path != NULL) {
        save_world(output_save_path, buildings, building_count, items, item_count, camera);
    }

    CloseWindow();

cleanup:
    free(buildings);
    free(items);
}
