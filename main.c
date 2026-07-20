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

#define BELT_LANE_COUNT 2
#define MAX_BELT_SLOTS 5
#define BELT_ITEM_SPACING 64.0f
#define BELT_LANE_LENGTH 256.0f
#define BELT_TURN_INNER_LENGTH 106.0f
#define BELT_TURN_OUTER_LENGTH 295.0f
#define DRILL_CYCLE_TICKS 120
#define TOOLBAR_SLOT_COUNT 10
#define ITEM_RADIUS_FACTOR 0.125f

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
    DIR_NORTH = 0,
    DIR_EAST = 90,
    DIR_SOUTH = 180,
    DIR_WEST = 270
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
    float belt_items[BELT_LANE_COUNT][MAX_BELT_SLOTS];
    item_type_e belt_item_types[BELT_LANE_COUNT][MAX_BELT_SLOTS];
    item_type_e output_type;
} building_t;

typedef struct {
    float x;
    float y;
    item_type_e type;
} item_t;

typedef struct {
    building_t* buildings;
    int building_count;
    int building_capacity;
    item_t* items;
    int item_count;
    int item_capacity;
    Camera2D camera;
    building_type_e current_building_idx;
    int current_held_rotation;
    item_type_e current_drill_output_mode;
    bool paused;
} game_state_t;

typedef struct {
    int origin_x;
    int origin_y;
    bool can_place;
} placement_preview_t;

static Color get_item_color(item_type_e type) {
    switch (type) {
        case ITEM_IRON_ORE: return (Color){ 100, 150, 200, 255 };
        case ITEM_COAL: return (Color){ 50, 50, 50, 255 };
        case ITEM_COPPER_ORE: return (Color){ 200, 150, 100, 255 };
        case ITEM_TYPE_COUNT: abort();
    }
    abort();
}

static Vector2 angle_to_dir(float angle_deg) {
    float rad = angle_deg * DEG2RAD;
    return (Vector2){ sinf(rad), -cosf(rad) };
}

static void direction_offset(int rotation, int* dx, int* dy) {
    Vector2 d = angle_to_dir((float)rotation);
    *dx = (int)roundf(d.x);
    *dy = (int)roundf(d.y);
}

static bool is_tile_occupied(int x, int y, building_t* buildings, int count) {
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

static int json_int(cJSON* obj, const char* key) {
    return cJSON_GetObjectItem(obj, key)->valueint;
}

static void json_get_float_opt(cJSON* root, const char* key, float* out) {
    cJSON* item = cJSON_GetObjectItem(root, key);
    if (item) *out = (float)item->valuedouble;
}

static cJSON* build_float_grid_array(float arr[BELT_LANE_COUNT][MAX_BELT_SLOTS]) {
    cJSON* array = cJSON_CreateArray();
    for (int l = 0; l < BELT_LANE_COUNT; l++)
        for (int j = 0; j < MAX_BELT_SLOTS; j++)
            cJSON_AddItemToArray(array, cJSON_CreateNumber(arr[l][j]));
    return array;
}

static cJSON* build_int_grid_array(item_type_e arr[BELT_LANE_COUNT][MAX_BELT_SLOTS]) {
    cJSON* array = cJSON_CreateArray();
    for (int l = 0; l < BELT_LANE_COUNT; l++)
        for (int j = 0; j < MAX_BELT_SLOTS; j++)
            cJSON_AddItemToArray(array, cJSON_CreateNumber(arr[l][j]));
    return array;
}

static void load_float_grid_array(cJSON* array, float dst[BELT_LANE_COUNT][MAX_BELT_SLOTS]) {
    for (int l = 0; l < BELT_LANE_COUNT; l++)
        for (int j = 0; j < MAX_BELT_SLOTS; j++)
            dst[l][j] = (float)cJSON_GetArrayItem(array, l * MAX_BELT_SLOTS + j)->valuedouble;
}

static void load_int_grid_array(cJSON* array, item_type_e dst[BELT_LANE_COUNT][MAX_BELT_SLOTS]) {
    for (int l = 0; l < BELT_LANE_COUNT; l++)
        for (int j = 0; j < MAX_BELT_SLOTS; j++)
            dst[l][j] = cJSON_GetArrayItem(array, l * MAX_BELT_SLOTS + j)->valueint;
}

static void save_world(const char* path, game_state_t* state) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);

    cJSON_AddNumberToObject(root, "camera_target_x", state->camera.target.x);
    cJSON_AddNumberToObject(root, "camera_target_y", state->camera.target.y);
    cJSON_AddNumberToObject(root, "camera_offset_x", state->camera.offset.x);
    cJSON_AddNumberToObject(root, "camera_offset_y", state->camera.offset.y);
    cJSON_AddNumberToObject(root, "camera_zoom", state->camera.zoom);

    cJSON* b_array = cJSON_CreateArray();
    for (int i = 0; i < state->building_count; i++) {
        building_t* b = &state->buildings[i];
        cJSON* b_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(b_obj, "x", b->x);
        cJSON_AddNumberToObject(b_obj, "y", b->y);
        cJSON_AddNumberToObject(b_obj, "origin_x", b->origin_x);
        cJSON_AddNumberToObject(b_obj, "origin_y", b->origin_y);
        cJSON_AddNumberToObject(b_obj, "size", b->size);
        cJSON_AddNumberToObject(b_obj, "type_idx", b->type_idx);
        cJSON_AddNumberToObject(b_obj, "rotation", b->rotation);
        cJSON_AddNumberToObject(b_obj, "progress", b->progress);
        cJSON_AddNumberToObject(b_obj, "output_type", b->output_type);

        cJSON_AddItemToObject(b_obj, "belt_items", build_float_grid_array(b->belt_items));
        cJSON_AddItemToObject(b_obj, "belt_item_types", build_int_grid_array(b->belt_item_types));

        cJSON_AddItemToArray(b_array, b_obj);
    }
    cJSON_AddItemToObject(root, "buildings", b_array);

    cJSON* item_array = cJSON_CreateArray();
    for (int i = 0; i < state->item_count; i++) {
        cJSON* item_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(item_obj, "x", (double)state->items[i].x);
        cJSON_AddNumberToObject(item_obj, "y", (double)state->items[i].y);
        cJSON_AddNumberToObject(item_obj, "type", state->items[i].type);
        cJSON_AddItemToArray(item_array, item_obj);
    }
    cJSON_AddItemToObject(root, "items", item_array);

    char* out = cJSON_Print(root);
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", out);
        fclose(f);
    }
    free(out);
    cJSON_Delete(root);
}

static void load_world(const char* path, game_state_t* state) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: --input-save could not open %s\n", path);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(data);
    if (!root) { free(data); return; }

    json_get_float_opt(root, "camera_target_x", &state->camera.target.x);
    json_get_float_opt(root, "camera_target_y", &state->camera.target.y);
    json_get_float_opt(root, "camera_offset_x", &state->camera.offset.x);
    json_get_float_opt(root, "camera_offset_y", &state->camera.offset.y);
    json_get_float_opt(root, "camera_zoom", &state->camera.zoom);

    cJSON* b_array = cJSON_GetObjectItem(root, "buildings");
    int count = cJSON_GetArraySize(b_array);

    state->building_count = count;
    state->building_capacity = count;
    state->buildings = realloc(state->buildings, sizeof(building_t) * state->building_capacity);

    for (int i = 0; i < count; i++) {
        cJSON* b_obj = cJSON_GetArrayItem(b_array, i);
        building_t* b = &state->buildings[i];
        b->x = json_int(b_obj, "x");
        b->y = json_int(b_obj, "y");
        b->origin_x = json_int(b_obj, "origin_x");
        b->origin_y = json_int(b_obj, "origin_y");
        b->size = json_int(b_obj, "size");
        b->type_idx = json_int(b_obj, "type_idx");
        b->rotation = json_int(b_obj, "rotation");
        b->progress = json_int(b_obj, "progress");
        b->output_type = json_int(b_obj, "output_type");

        load_float_grid_array(cJSON_GetObjectItem(b_obj, "belt_items"), b->belt_items);
        load_int_grid_array(cJSON_GetObjectItem(b_obj, "belt_item_types"), b->belt_item_types);
    }

    cJSON* item_array = cJSON_GetObjectItem(root, "items");
    if (item_array) {
        int item_count = cJSON_GetArraySize(item_array);
        state->item_count = item_count;
        state->item_capacity = item_count;
        state->items = realloc(state->items, sizeof(item_t) * state->item_capacity);
        for (int i = 0; i < item_count; i++) {
            cJSON* item_obj = cJSON_GetArrayItem(item_array, i);
            state->items[i].x = (float)cJSON_GetObjectItem(item_obj, "x")->valuedouble;
            state->items[i].y = (float)cJSON_GetObjectItem(item_obj, "y")->valuedouble;
            state->items[i].type = json_int(item_obj, "type");
        }
    }

    cJSON_Delete(root);
    free(data);
}

static int find_belt_at(int x, int y, building_t* buildings, int count) {
    for (int i = 0; i < count; i++) {
        if (buildings[i].type_idx == BUILDING_TRANSPORT_BELT && buildings[i].x == x && buildings[i].y == y) {
            return i;
        }
    }
    return -1;
}

static int find_belt_with_rotation(int x, int y, int rotation, building_t* buildings, int count) {
    for (int i = 0; i < count; i++) {
        if (buildings[i].type_idx == BUILDING_TRANSPORT_BELT &&
            buildings[i].x == x && buildings[i].y == y &&
            buildings[i].rotation == rotation) {
            return i;
        }
    }
    return -1;
}

static bool belt_faces_away_from(int belt_idx, int from_x, int from_y, building_t* buildings) {
    Vector2 t_dir = angle_to_dir((float)buildings[belt_idx].rotation);
    return (buildings[belt_idx].x + (int)t_dir.x == from_x && buildings[belt_idx].y + (int)t_dir.y == from_y);
}

static int get_forward_belt_target(building_t* belt, int self_x, int self_y, building_t* buildings, int count) {
    int dx, dy;
    direction_offset(belt->rotation, &dx, &dy);
    int tx = self_x + dx;
    int ty = self_y + dy;

    int idx = find_belt_at(tx, ty, buildings, count);
    if (idx != -1 && belt_faces_away_from(idx, self_x, self_y, buildings)) {
        idx = -1;
    }
    return idx;
}

static bool has_belt_feeding_from(int target_x, int target_y, int feeder_rot, building_t* buildings, int count) {
    if (!buildings) return false;
    int dx, dy;
    direction_offset(feeder_rot, &dx, &dy);
    int feeder_x = target_x - dx;
    int feeder_y = target_y - dy;
    return find_belt_with_rotation(feeder_x, feeder_y, feeder_rot, buildings, count) != -1;
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

static float get_belt_lane_length(building_t* belt, int lane, building_t* buildings, int building_count) {
    int in_rot = get_belt_input_rotation(belt, buildings, building_count);
    int out_rot = belt->rotation;
    if (in_rot == out_rot) {
        return BELT_LANE_LENGTH;
    }
    int rel_rot = (out_rot - in_rot + 360) % 360;
    if (rel_rot == ROT_CW) {
        return (lane == 1) ? BELT_TURN_INNER_LENGTH : BELT_TURN_OUTER_LENGTH;
    } else if (rel_rot == ROT_CCW) {
        return (lane == 0) ? BELT_TURN_INNER_LENGTH : BELT_TURN_OUTER_LENGTH;
    }
    return BELT_LANE_LENGTH;
}

static int find_last_occupied_slot(const float lane[MAX_BELT_SLOTS]) {
    int last = -1;
    for (int j = 0; j < MAX_BELT_SLOTS; j++) {
        if (lane[j] >= 0.0f) last = j;
    }
    return last;
}

static void append_item(game_state_t* state, item_t item) {
    if (state->item_count >= state->item_capacity) {
        state->item_capacity = (state->item_capacity == 0) ? 64 : state->item_capacity * 2;
        state->items = realloc(state->items, state->item_capacity * sizeof(item_t));
    }
    state->items[state->item_count++] = item;
}

static bool get_belt_transfer_info(building_t* source, building_t* target, int source_lane, building_t* buildings, int count, int* out_target_lane, float* out_entry_d) {
    if (!target) return false;

    int source_in_rot = get_belt_input_rotation(source, buildings, count);
    bool source_is_turning = (source_in_rot != source->rotation);

    int target_in_rot = get_belt_input_rotation(target, buildings, count);
    bool target_is_turning = (target_in_rot != target->rotation);

    int rel_rot = (target->rotation - source->rotation + 360) % 360;

    int target_lane = source_lane;
    float entry_d = 0.0f;

    if (rel_rot != 0) {
        if (!target_is_turning) {
            if (rel_rot == ROT_CW) {
                target_lane = 1;
                entry_d = (source_lane == 0) ? (0.25f * BELT_LANE_LENGTH) : (0.75f * BELT_LANE_LENGTH);
            } else if (rel_rot == ROT_CCW) {
                target_lane = 0;
                entry_d = (source_lane == 1) ? (0.25f * BELT_LANE_LENGTH) : (0.75f * BELT_LANE_LENGTH);
            } else {
                return false;
            }
        } else if (!source_is_turning && (rel_rot == ROT_CCW)) {
            target_lane = !target_lane;
        }
    }

    *out_target_lane = target_lane;
    *out_entry_d = entry_d;
    return true;
}

static void advance_belt_lane(building_t* buildings, int building_count, int belt_idx, int lane) {
    building_t* belt = &buildings[belt_idx];
    float lane_len = get_belt_lane_length(belt, lane, buildings, building_count);
    int target_idx = get_forward_belt_target(belt, belt->x, belt->y, buildings, building_count);

    float max_d_0 = lane_len;
    if (target_idx != -1) {
        int target_lane;
        float entry_d;
        if (get_belt_transfer_info(belt, &buildings[target_idx], lane, buildings, building_count, &target_lane, &entry_d)) {
            building_t* target = &buildings[target_idx];
            float target_lane_len = get_belt_lane_length(target, target_lane, buildings, building_count);

            if (entry_d == 0.0f) {
                int target_last_j = find_last_occupied_slot(target->belt_items[target_lane]);
                if (target_last_j != -1) {
                    float d_target_last = target->belt_items[target_lane][target_last_j] * target_lane_len;
                    max_d_0 = lane_len - (BELT_ITEM_SPACING - d_target_last);
                } else {
                    max_d_0 = lane_len + BELT_ITEM_SPACING;
                }
            } else {
                max_d_0 = lane_len;
            }
        }
    }

    for (int j = 0; j < MAX_BELT_SLOTS; j++) {
        if (belt->belt_items[lane][j] < 0.0f) continue;

        float current_d = belt->belt_items[lane][j] * lane_len;
        float limit_d = (j == 0) ? max_d_0 : (belt->belt_items[lane][j - 1] * lane_len - BELT_ITEM_SPACING);

        float new_d = current_d + (BELT_LANE_LENGTH / UPS);
        if (new_d > limit_d) new_d = limit_d;
        if (new_d < 0.0f) new_d = 0.0f;

        belt->belt_items[lane][j] = new_d / lane_len;
    }
}

static void transfer_belt_lane(building_t* buildings, int building_count, int belt_idx, int lane) {
    building_t* belt = &buildings[belt_idx];
    if (belt->belt_items[lane][0] < 1.0f) return;

    int target_idx = get_forward_belt_target(belt, belt->x, belt->y, buildings, building_count);
    if (target_idx == -1) {
        belt->belt_items[lane][0] = 1.0f;
        return;
    }

    int target_lane;
    float entry_d;
    if (!get_belt_transfer_info(belt, &buildings[target_idx], lane, buildings, building_count, &target_lane, &entry_d)) {
        belt->belt_items[lane][0] = 1.0f;
        return;
    }

    building_t* target = &buildings[target_idx];
    float target_lane_len = get_belt_lane_length(target, target_lane, buildings, building_count);
    float lane_len = get_belt_lane_length(belt, lane, buildings, building_count);

    float excess_d = (belt->belt_items[lane][0] * lane_len) - lane_len;
    if (excess_d < 0.0f) excess_d = 0.0f;

    float start_d_target = entry_d + excess_d;
    bool can_transfer = false;
    int insert_j = -1;

    if (entry_d == 0.0f) {
        int target_last_j = find_last_occupied_slot(target->belt_items[target_lane]);
        if (target_last_j == -1) {
            can_transfer = true;
            insert_j = 0;
        } else {
            float d_target_last = target->belt_items[target_lane][target_last_j] * target_lane_len;
            if (d_target_last >= BELT_ITEM_SPACING) {
                can_transfer = (target_last_j < MAX_BELT_SLOTS - 1);
                if (start_d_target > d_target_last - BELT_ITEM_SPACING) {
                    start_d_target = d_target_last - BELT_ITEM_SPACING;
                }
                insert_j = target_last_j + 1;
            }
        }
    } else {
        int occupied = 0;

        for (int j = 0; j < MAX_BELT_SLOTS; j++) {
            if (target->belt_items[target_lane][j] >= 0.0f) {
                occupied++;
            }
        }

        int rel_rot = (target->rotation - belt->rotation + 360) % 360;
        bool is_weird_third_item_sideload_case = occupied == 2 && rel_rot == ROT_CW && lane == 1;

        if (is_weird_third_item_sideload_case) {
            start_d_target = 0.5f * BELT_LANE_LENGTH;
        }

        can_transfer = (occupied < MAX_BELT_SLOTS);

        if (can_transfer) {
            for (int j = 0; j < MAX_BELT_SLOTS; j++) {
                if (target->belt_items[target_lane][j] < 0.0f) {
                    continue;
                }

                float d = target->belt_items[target_lane][j] * target_lane_len;
                if (fabsf(d - start_d_target) < BELT_ITEM_SPACING - 0.1f) {
                    can_transfer = false;
                    break;
                }
            }
        }

        if (can_transfer) {
            insert_j = 0;
            while (insert_j < MAX_BELT_SLOTS && target->belt_items[target_lane][insert_j] >= 0.0f) {
                float d = target->belt_items[target_lane][insert_j] * target_lane_len;
                if (d < start_d_target) {
                    break;
                }
                insert_j++;
            }
        }
    }

    if (!can_transfer) {
        belt->belt_items[lane][0] = 1.0f;
        return;
    }

    for (int j = MAX_BELT_SLOTS - 1; j > insert_j; j--) {
        target->belt_items[target_lane][j] = target->belt_items[target_lane][j - 1];
        target->belt_item_types[target_lane][j] = target->belt_item_types[target_lane][j - 1];
    }

    target->belt_items[target_lane][insert_j] = start_d_target / target_lane_len;
    target->belt_item_types[target_lane][insert_j] = belt->belt_item_types[lane][0];

    for (int j = 0; j < MAX_BELT_SLOTS - 1; j++) {
        belt->belt_items[lane][j] = belt->belt_items[lane][j + 1];
        belt->belt_item_types[lane][j] = belt->belt_item_types[lane][j + 1];
    }

    belt->belt_items[lane][MAX_BELT_SLOTS - 1] = -1.0f;
}

static void run_drill(game_state_t* state, int drill_idx, int tile_size) {
    building_t* buildings = state->buildings;
    int building_count = state->building_count;
    building_t* drill = &buildings[drill_idx];

    if (drill->progress < DRILL_CYCLE_TICKS) drill->progress++;
    if (drill->progress < DRILL_CYCLE_TICKS) return;

    int dx, dy;
    direction_offset(drill->rotation, &dx, &dy);
    int center_x = drill->origin_x + drill->size / 2;
    int center_y = drill->origin_y + drill->size / 2;
    int tx = center_x + dx * 2;
    int ty = center_y + dy * 2;

    int target_idx = find_belt_at(tx, ty, buildings, building_count);

    if (target_idx != -1) {
        building_t* target = &buildings[target_idx];
        int rel_rot = (target->rotation - drill->rotation + 360) % 360;
        int lane = (rel_rot == ROT_CCW) ? 0 : 1;

        float target_lane_len = get_belt_lane_length(target, lane, buildings, building_count);
        float insert_d = (rel_rot == 0) ? 32.0f : 103.0f;

        int last_item_idx = find_last_occupied_slot(target->belt_items[lane]);

        bool can_insert = false;
        if (last_item_idx == -1) {
            can_insert = true;
        } else if (last_item_idx < MAX_BELT_SLOTS - 2) {
            float d_last = target->belt_items[lane][last_item_idx] * target_lane_len;
            if (d_last >= insert_d + BELT_ITEM_SPACING) can_insert = true;
        }

        if (can_insert) {
            target->belt_items[lane][last_item_idx + 1] = insert_d / target_lane_len;
            target->belt_item_types[lane][last_item_idx + 1] = drill->output_type;
            drill->progress = 0;
        }
    } else {
        bool building_collision = is_tile_occupied(tx, ty, buildings, building_count);
        bool item_collision = false;
        for (int k = 0; k < state->item_count; k++) {
            if ((int)floorf(state->items[k].x / tile_size) == tx && (int)floorf(state->items[k].y / tile_size) == ty) {
                item_collision = true;
                break;
            }
        }

        if (!item_collision && !building_collision) {
            float item_x = (tx + 0.5f - (float)dx * 0.152f) * tile_size;
            float item_y = (ty + 0.5f - (float)dy * 0.152f) * tile_size;
            append_item(state, (item_t){ item_x, item_y, drill->output_type });
            drill->progress = 0;
        }
    }
}

static void game_logic_tick(game_state_t* state, int tile_size) {
    for (int i = 0; i < state->building_count; i++) {
        if (state->buildings[i].type_idx != BUILDING_TRANSPORT_BELT) continue;
        for (int l = 0; l < BELT_LANE_COUNT; l++) {
            advance_belt_lane(state->buildings, state->building_count, i, l);
        }
    }

    for (int i = 0; i < state->building_count; i++) {
        if (state->buildings[i].type_idx != BUILDING_TRANSPORT_BELT) continue;
        for (int l = 0; l < BELT_LANE_COUNT; l++) {
            transfer_belt_lane(state->buildings, state->building_count, i, l);
        }
    }

    for (int i = 0; i < state->building_count; i++) {
        if (state->buildings[i].type_idx == BUILDING_MINING_DRILL) {
            run_drill(state, i, tile_size);
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

static void draw_building_base(int origin_x, int origin_y, int size, int rotation, int tile_size, Color color) {
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
}

static bool clip_line_to_rect(Vector2* p0, Vector2* p1, float xmin, float xmax, float ymin, float ymax) {
    float dx = p1->x - p0->x;
    float dy = p1->y - p0->y;
    float t0 = 0.0f;
    float t1 = 1.0f;

    float p[4] = { -dx, dx, -dy, dy };
    float q[4] = { p0->x - xmin, xmax - p0->x, p0->y - ymin, ymax - p0->y };

    for (int i = 0; i < 4; i++) {
        if (fabsf(p[i]) < 1e-6f) {
            if (q[i] < 0.0f) return false;
        } else {
            float t = q[i] / p[i];
            if (p[i] < 0.0f) {
                if (t > t1) return false;
                if (t > t0) t0 = t;
            } else {
                if (t < t0) return false;
                if (t < t1) t1 = t;
            }
        }
    }

    if (t0 > t1) return false;

    Vector2 cp0 = { p0->x + t0 * dx, p0->y + t0 * dy };
    Vector2 cp1 = { p0->x + t1 * dx, p0->y + t1 * dy };
    *p0 = cp0;
    *p1 = cp1;
    return true;
}

static void draw_clipped_chevron(Vector2 tip, float arm_length, float angle_deg, float spread_deg, float thickness, Color color, float xmin, float xmax, float ymin, float ymax) {
    float back_angle = angle_deg + 180.0f;
    Vector2 arm1 = angle_to_dir(back_angle - spread_deg);
    Vector2 arm2 = angle_to_dir(back_angle + spread_deg);
    Vector2 p1 = { tip.x + arm1.x * arm_length, tip.y + arm1.y * arm_length };
    Vector2 p2 = { tip.x + arm2.x * arm_length, tip.y + arm2.y * arm_length };

    Vector2 l1_p0 = tip;
    Vector2 l1_p1 = p1;
    if (clip_line_to_rect(&l1_p0, &l1_p1, xmin, xmax, ymin, ymax)) {
        DrawLineEx(l1_p0, l1_p1, thickness, color);
    }

    Vector2 l2_p0 = tip;
    Vector2 l2_p1 = p2;
    if (clip_line_to_rect(&l2_p0, &l2_p1, xmin, xmax, ymin, ymax)) {
        DrawLineEx(l2_p0, l2_p1, thickness, color);
    }
}

static void draw_building_overlay(building_type_e type_idx, int origin_x, int origin_y, int size, int rotation, int tile_size, building_t* buildings, int building_count, int progress, item_type_e output_type, bool draw_overlay) {
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
        Color anim_chevron_color = (Color){ 230, 200, 40, 255 };

        bool has_feeder = has_belt_feeding_from(origin_x, origin_y, input_rot, buildings, building_count);

        float time_sec = (float)GetTime();
        float xmin = origin_px_x;
        float xmax = origin_px_x + tile_size;
        float ymin = origin_px_y;
        float ymax = origin_px_y + tile_size;

        for (int c = 0; c < 2; c++) {
            float t = fmodf(time_sec * 1.0f + c * 0.5f, 1.0f);
            Vector2 pos;
            float chev_angle;

            if (input_rot == rotation) {
                Vector2 start_pos = { tile_center.x - in_dir.x * tile_size * 0.5f, tile_center.y - in_dir.y * tile_size * 0.5f };
                Vector2 end_pos = { tile_center.x + dir.x * tile_size * 0.5f, tile_center.y + dir.y * tile_size * 0.5f };
                pos.x = start_pos.x + (end_pos.x - start_pos.x) * t;
                pos.y = start_pos.y + (end_pos.y - start_pos.y) * t;
                chev_angle = (float)rotation;
            } else {
                int rel_rot = (rotation - input_rot + 360) % 360;
                bool is_clockwise = (rel_rot == ROT_CW);

                int idx = (input_rot / 90) % 4;
                if (!is_clockwise) idx = (idx + 1) % 4;
                static const float pivot_xs[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                static const float pivot_ys[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
                float pivot_x = pivot_xs[idx];
                float pivot_y = pivot_ys[idx];

                float r = 0.5f * tile_size;

                float start_angle, end_angle;
                if (is_clockwise) {
                    start_angle = (float)((input_rot + 180) % 360);
                    end_angle = start_angle + 90.0f;
                } else {
                    start_angle = (float)input_rot;
                    end_angle = start_angle - 90.0f;
                }

                float a = start_angle + (end_angle - start_angle) * t;
                pos.x = (origin_px_x + pivot_x * tile_size) + cosf(a * DEG2RAD) * r;
                pos.y = (origin_px_y + pivot_y * tile_size) + sinf(a * DEG2RAD) * r;

                chev_angle = is_clockwise ? a + 180.0f : a;
            }

            if (has_feeder) {
                draw_chevron(pos, tile_size * 0.14f, chev_angle, 35.0f, 2.0f, anim_chevron_color);
            } else {
                draw_clipped_chevron(pos, tile_size * 0.14f, chev_angle, 35.0f, 2.0f, anim_chevron_color, xmin, xmax, ymin, ymax);
            }
        }
    } else if (type_idx == BUILDING_INSERTER) {
        Vector2 tile_center = { origin_px_x + tile_size / 2.0f, origin_px_y + tile_size / 2.0f };
        Vector2 edge_point = { tile_center.x + dir.x * tile_size * 0.5f, tile_center.y + dir.y * tile_size * 0.5f };
        draw_kinked_chevron(tile_center, edge_point, tile_size * 0.125f, 3.0f, chevron_color);
    }
}

static void draw_building(building_type_e type_idx, int origin_x, int origin_y, int size, int rotation, int tile_size, Color color, building_t* buildings, int building_count, int progress, item_type_e output_type, bool draw_overlay) {
    draw_building_base(origin_x, origin_y, size, rotation, tile_size, color);
    draw_building_overlay(type_idx, origin_x, origin_y, size, rotation, tile_size, buildings, building_count, progress, output_type, draw_overlay);
}

static Vector2 compute_belt_item_position(building_t* belt, building_t* buildings, int building_count, int lane, float prog, int tile_size) {
    int in_rot = get_belt_input_rotation(belt, buildings, building_count);
    int out_rot = belt->rotation;
    Vector2 tile_center = { (belt->x + 0.5f) * tile_size, (belt->y + 0.5f) * tile_size };
    float lane_offset = (lane == 0) ? -0.25f : 0.25f;

    if (in_rot == out_rot) {
        Vector2 dir = angle_to_dir((float)out_rot);
        Vector2 right = { -dir.y, dir.x };
        float p_offset = prog - 0.5f;
        return (Vector2){
            tile_center.x + right.x * lane_offset * tile_size + dir.x * p_offset * tile_size,
            tile_center.y + right.y * lane_offset * tile_size + dir.y * p_offset * tile_size
        };
    }

    int rel_rot = (out_rot - in_rot + 360) % 360;
    bool is_clockwise = (rel_rot == ROT_CW);

    int idx = (in_rot / 90) % 4;
    if (!is_clockwise) idx = (idx + 1) % 4;
    static const float pivot_xs[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    static const float pivot_ys[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
    float pivot_x = pivot_xs[idx];
    float pivot_y = pivot_ys[idx];

    float r;
    if (is_clockwise) r = (lane == 1) ? 0.25f * tile_size : 0.75f * tile_size;
    else r = (lane == 0) ? 0.25f * tile_size : 0.75f * tile_size;

    float start_angle, end_angle;
    if (is_clockwise) {
        start_angle = (in_rot == DIR_NORTH) ? 180.0f : (in_rot == DIR_EAST) ? 270.0f : (in_rot == DIR_SOUTH) ? 0.0f : 90.0f;
        end_angle = start_angle + 90.0f;
    } else {
        start_angle = (in_rot == DIR_NORTH) ? 0.0f : (in_rot == DIR_EAST) ? 90.0f : (in_rot == DIR_SOUTH) ? 180.0f : 270.0f;
        end_angle = start_angle - 90.0f;
    }

    float a = start_angle + (end_angle - start_angle) * prog;

    Vector2 tile_top_left = { (float)belt->x * tile_size, (float)belt->y * tile_size };
    return (Vector2){
        (tile_top_left.x + pivot_x * tile_size) + cosf(a * DEG2RAD) * r,
        (tile_top_left.y + pivot_y * tile_size) + sinf(a * DEG2RAD) * r
    };
}

static void render_grid(Camera2D camera, int screen_width, int screen_height, int tile_size) {
    Vector2 screen_top_left = GetScreenToWorld2D((Vector2){ 0, 0 }, camera);
    Vector2 screen_bottom_right = GetScreenToWorld2D((Vector2){ screen_width, screen_height }, camera);
    int start_x = (int)floorf(screen_top_left.x / tile_size) - 1;
    int end_x = (int)floorf(screen_bottom_right.x / tile_size) + 1;
    int start_y = (int)floorf(screen_top_left.y / tile_size) - 1;
    int end_y = (int)floorf(screen_bottom_right.y / tile_size) + 1;

    Color grid_color = (Color){ 45, 45, 45, 255 };
    for (int x = start_x; x <= end_x; x++) DrawLine(x * tile_size, start_y * tile_size, x * tile_size, end_y * tile_size, grid_color);
    for (int y = start_y; y <= end_y; y++) DrawLine(start_x * tile_size, y * tile_size, end_x * tile_size, y * tile_size, grid_color);
}

static void render_buildings_base(building_t* buildings, int count, int tile_size) {
    for (int i = 0; i < count; i++) {
        draw_building_base(buildings[i].origin_x, buildings[i].origin_y, buildings[i].size, buildings[i].rotation, tile_size, BUILDING_TYPES[buildings[i].type_idx].color);
    }
}

static void render_buildings_overlay(building_t* buildings, int count, int tile_size) {
    for (int i = 0; i < count; i++) {
        draw_building_overlay(buildings[i].type_idx, buildings[i].origin_x, buildings[i].origin_y, buildings[i].size, buildings[i].rotation, tile_size, buildings, count, buildings[i].progress, buildings[i].output_type, false);
    }
}

static void render_belt_items(building_t* buildings, int count, int tile_size) {
    for (int i = 0; i < count; i++) {
        if (buildings[i].type_idx != BUILDING_TRANSPORT_BELT) continue;

        for (int l = 0; l < BELT_LANE_COUNT; l++) {
            for (int j = 0; j < MAX_BELT_SLOTS; j++) {
                float prog = buildings[i].belt_items[l][j];
                if (prog < 0.0f) continue;

                Vector2 pos = compute_belt_item_position(&buildings[i], buildings, count, l, prog, tile_size);
                DrawCircleV(pos, tile_size * ITEM_RADIUS_FACTOR, get_item_color(buildings[i].belt_item_types[l][j]));
            }
        }
    }
}

static void render_ground_items(item_t* items, int count, int tile_size) {
    for (int i = 0; i < count; i++) {
        DrawCircleV((Vector2){ items[i].x, items[i].y }, tile_size * ITEM_RADIUS_FACTOR, get_item_color(items[i].type));
    }
}

static void render_placement_ghost(game_state_t* state, placement_preview_t preview, bool mouse_over_toolbar, int tile_size) {
    if (mouse_over_toolbar || state->current_building_idx == BUILDING_NONE) return;

    Color base = BUILDING_TYPES[state->current_building_idx].color;
    Color tint = preview.can_place ? (Color){ base.r, base.g, base.b, 150 } : (Color){ 255, 0, 0, 150 };
    draw_building(state->current_building_idx, preview.origin_x, preview.origin_y, BUILDING_TYPES[state->current_building_idx].size, state->current_held_rotation, tile_size, tint, state->buildings, state->building_count, 0, state->current_drill_output_mode, true);
}

static void render_hud(game_state_t* state, int grid_x, int grid_y, int screen_width, float dt, int ticks_this_frame) {
    DrawText(TextFormat("Building Coord: (%d, %d)", grid_x, grid_y), 10, 10, 20, RAYWHITE);
    DrawText(TextFormat("Total Buildings: %d", state->building_count), 10, 35, 20, GRAY);
    DrawText(TextFormat("Zoom: %.2fx", state->camera.zoom), 10, 60, 20, GRAY);

    float fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
    float ups = (dt > 0.0f) ? ((float)ticks_this_frame / dt) : 0.0f;
    const char* perf_text = TextFormat("FPS/UPS = %.1f/%.1f", fps, ups);
    DrawText(perf_text, screen_width - MeasureText(perf_text, 20) - 10, 10, 20, RAYWHITE);
}

static void render_hotbar(building_type_e current_building_idx, int screen_width, int screen_height) {
    int bar_w = 500;
    int bar_h = 50;
    int start_x_pos = (screen_width - bar_w) / 2;
    int start_y_pos = screen_height - bar_h - 10;

    int item_size = 40;
    int slot_width = bar_w / TOOLBAR_SLOT_COUNT;

    DrawRectangle(start_x_pos, start_y_pos, bar_w, bar_h, (Color){ 30, 30, 30, 255 });

    for (int i = 0; i < TOOLBAR_SLOT_COUNT; i++) {
        int x = start_x_pos + i * slot_width + (slot_width - item_size) / 2;
        int y = start_y_pos + (bar_h - item_size) / 2;

        Color slot_color = (BUILDING_TYPES[i].name != NULL) ? BUILDING_TYPES[i].color : (Color){ 45, 45, 45, 255 };
        DrawRectangle(x, y, item_size, item_size, slot_color);

        if (i == current_building_idx) {
            DrawRectangleLines(x - 2, y - 2, item_size + 4, item_size + 4, WHITE);

            const char* held_name = BUILDING_TYPES[i].name;
            int text_w = MeasureText(held_name, 14);
            DrawText(held_name, x + item_size / 2 - text_w / 2, y - 16, 14, RAYWHITE);
        }
    }
}

static bool remove_item_at_cursor(Vector2 mouse_world, item_t* items, int* item_count, int tile_size) {
    float pickup_radius = tile_size * ITEM_RADIUS_FACTOR;

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

static bool remove_building_at_cursor(int grid_x, int grid_y, building_t* buildings, int* building_count) {
    for (int i = 0; i < *building_count; i++) {
        building_t* b = &buildings[i];

        if (grid_x >= b->origin_x &&
            grid_x < b->origin_x + b->size &&
            grid_y >= b->origin_y &&
            grid_y < b->origin_y + b->size) {
            buildings[i] = buildings[*building_count - 1];
            (*building_count)--;
            return true;
        }
    }

    return false;
}

static void handle_f_key_deletion(game_state_t* state, Vector2 mouse_world, int tile_size) {
    if (!IsKeyDown(KEY_F)) return;

    float delete_radius = tile_size * 1.0f;
    float delete_radius_sq = delete_radius * delete_radius;

    for (int i = 0; i < state->item_count; i++) {
        float dx = state->items[i].x - mouse_world.x;
        float dy = state->items[i].y - mouse_world.y;
        if (dx * dx + dy * dy <= delete_radius_sq) {
            state->items[i] = state->items[state->item_count - 1];
            state->item_count--;
            i--;
        }
    }

    for (int i = 0; i < state->building_count; i++) {
        building_t* b = &state->buildings[i];
        if (b->type_idx != BUILDING_TRANSPORT_BELT) continue;

        for (int l = 0; l < BELT_LANE_COUNT; l++) {
            for (int j = 0; j < MAX_BELT_SLOTS; j++) {
                if (b->belt_items[l][j] < 0.0f) continue;

                Vector2 pos = compute_belt_item_position(b, state->buildings, state->building_count, l, b->belt_items[l][j], tile_size);
                float dx = pos.x - mouse_world.x;
                float dy = pos.y - mouse_world.y;

                if (dx * dx + dy * dy <= delete_radius_sq) {
                    for (int k = j; k < MAX_BELT_SLOTS - 1; k++) {
                        b->belt_items[l][k] = b->belt_items[l][k + 1];
                        b->belt_item_types[l][k] = b->belt_item_types[l][k + 1];
                    }
                    b->belt_items[l][MAX_BELT_SLOTS - 1] = -1.0f;
                    j--;
                }
            }
        }
    }
}

static placement_preview_t compute_placement_preview(game_state_t* state, Vector2 mouse_world, int tile_size) {
    placement_preview_t preview = { 0, 0, true };
    if (state->current_building_idx == BUILDING_NONE) return preview;

    int size = BUILDING_TYPES[state->current_building_idx].size;
    float offset = (size - 1) / 2.0f;
    preview.origin_x = (int)floorf((mouse_world.x / tile_size) - offset);
    preview.origin_y = (int)floorf((mouse_world.y / tile_size) - offset);

    for (int dx = 0; dx < size; dx++) {
        for (int dy = 0; dy < size; dy++) {
            int cx = preview.origin_x + dx;
            int cy = preview.origin_y + dy;
            if (is_tile_occupied(cx, cy, state->buildings, state->building_count)) {
                preview.can_place = false;
                break;
            }
        }
        if (!preview.can_place) break;
    }

    return preview;
}

static void try_place_building(game_state_t* state, placement_preview_t preview, int tile_size) {
    int size = BUILDING_TYPES[state->current_building_idx].size;

    if (state->building_count + 1 > state->building_capacity) {
        state->building_capacity = (state->building_capacity == 0) ? 1 : state->building_capacity * 2;
        state->buildings = realloc(state->buildings, state->building_capacity * sizeof(building_t));
    }

    for (int dx = 0; dx < size; dx++) {
        for (int dy = 0; dy < size; dy++) {
            int cx = preview.origin_x + dx;
            int cy = preview.origin_y + dy;
            for (int k = 0; k < state->item_count; k++) {
                if ((int)floorf(state->items[k].x / tile_size) == cx && (int)floorf(state->items[k].y / tile_size) == cy) {
                    for (int m = k; m < state->item_count - 1; m++) state->items[m] = state->items[m + 1];
                    state->item_count--;
                    k--;
                }
            }
        }
    }

    building_t new_b;
    memset(&new_b, 0, sizeof(building_t));

    new_b.x = preview.origin_x;
    new_b.y = preview.origin_y;
    new_b.origin_x = preview.origin_x;
    new_b.origin_y = preview.origin_y;
    new_b.size = size;
    new_b.type_idx = state->current_building_idx;
    new_b.rotation = state->current_held_rotation;
    new_b.output_type = state->current_drill_output_mode;

    for (int l = 0; l < BELT_LANE_COUNT; l++)
        for (int j = 0; j < MAX_BELT_SLOTS; j++)
            new_b.belt_items[l][j] = -1.0f;

    state->buildings[state->building_count++] = new_b;
}

static void handle_placement_input(game_state_t* state, placement_preview_t preview, bool mouse_over_toolbar, int tile_size) {
    if (!mouse_over_toolbar && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && state->current_building_idx != BUILDING_NONE && preview.can_place) {
        try_place_building(state, preview, tile_size);
    }
}

static void handle_removal_input(game_state_t* state, Vector2 mouse_world, int grid_x, int grid_y, int tile_size) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        if (!remove_item_at_cursor(mouse_world, state->items, &state->item_count, tile_size)) {
            remove_building_at_cursor(grid_x, grid_y, state->buildings, &state->building_count);
        }
    }
}

static void handle_rotation_input(game_state_t* state, int grid_x, int grid_y) {
    if (!IsKeyPressed(KEY_R)) return;

    int rot_mod = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? ROT_CCW : ROT_CW;
    if (state->current_building_idx != BUILDING_NONE) {
        state->current_held_rotation = (state->current_held_rotation + rot_mod) % 360;
        return;
    }

    for (int i = 0; i < state->building_count; i++) {
        building_t* b = &state->buildings[i];
        if (grid_x >= b->origin_x && grid_x < b->origin_x + b->size &&
            grid_y >= b->origin_y && grid_y < b->origin_y + b->size) {
            b->rotation = (b->rotation + rot_mod) % 360;
        }
    }
}

static void handle_pickup_input(game_state_t* state, int grid_x, int grid_y) {
    if (!IsKeyPressed(KEY_Q)) return;

    if (state->current_building_idx != BUILDING_NONE) {
        state->current_building_idx = BUILDING_NONE;
        return;
    }

    for (int i = 0; i < state->building_count; i++) {
        building_t* b = &state->buildings[i];
        if (grid_x >= b->origin_x && grid_x < b->origin_x + b->size &&
            grid_y >= b->origin_y && grid_y < b->origin_y + b->size) {
            state->current_building_idx = b->type_idx;
            break;
        }
    }
}

static void handle_hotbar_shortcuts(game_state_t* state) {
    for (int i = 0; i < 9; i++) {
        if (IsKeyPressed(KEY_ONE + i) && BUILDING_TYPES[i].name != NULL) state->current_building_idx = i;
    }
    if (IsKeyPressed(KEY_ZERO) && BUILDING_TYPES[9].name != NULL) state->current_building_idx = 9;
}

static void handle_output_mode_toggle(game_state_t* state) {
    if (IsKeyPressed(KEY_O)) {
        state->current_drill_output_mode = (state->current_drill_output_mode + 1) % ITEM_TYPE_COUNT;
    }
}

static void handle_camera_pan(game_state_t* state, float dt) {
    float move_speed = 500.0f / state->camera.zoom * dt;
    if (IsKeyDown(KEY_W)) state->camera.target.y -= move_speed;
    if (IsKeyDown(KEY_S)) state->camera.target.y += move_speed;
    if (IsKeyDown(KEY_A)) state->camera.target.x -= move_speed;
    if (IsKeyDown(KEY_D)) state->camera.target.x += move_speed;
    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
}

static void handle_camera_zoom(game_state_t* state) {
    float wheel = GetMouseWheelMove();
    if (wheel == 0.0f) return;

    Vector2 mouse_world_pos = GetScreenToWorld2D(GetMousePosition(), state->camera);
    state->camera.zoom += wheel * 0.125f;
    if (state->camera.zoom < 0.25f) state->camera.zoom = 0.25f;
    if (state->camera.zoom > 2.0f) state->camera.zoom = 2.0f;
    Vector2 mouse_world_pos_new = GetScreenToWorld2D(GetMousePosition(), state->camera);
    state->camera.target.x += (mouse_world_pos.x - mouse_world_pos_new.x);
    state->camera.target.y += (mouse_world_pos.y - mouse_world_pos_new.y);
}

static void parse_args(int argc, char** argv, game_state_t* state, char** output_save_path, int* run_ticks) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input-save") == 0) {
            if (i + 1 < argc) {
                load_world(argv[++i], state);
            } else {
                fprintf(stderr, "Error: --input-save requires a file path argument.\n");
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--output-save") == 0) {
            if (i + 1 < argc) {
                *output_save_path = argv[++i];
            } else {
                fprintf(stderr, "Error: --output-save requires a file path argument.\n");
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--ticks") == 0) {
            if (i + 1 < argc) {
                *run_ticks = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Error: --ticks requires an integer argument.\n");
                exit(EXIT_FAILURE);
            }
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }
}

static void run_headless(game_state_t* state, int run_ticks, int tile_size, const char* output_save_path) {
    for (int t = 0; t < run_ticks; t++) {
        game_logic_tick(state, tile_size);
    }
    if (output_save_path) save_world(output_save_path, state);
}

static void run_interactive(game_state_t* state, int tile_size, const char* output_save_path) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(0, 0, "grugtorio");

    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    if (state->camera.offset.x == 0) {
        state->camera.offset = (Vector2){ screen_width / 2.0f, screen_height / 2.0f };
    }

    SetTargetFPS(60);

    double last_time = get_time_ms();
    double accumulator = 0.0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        double now = get_time_ms();
        double elapsed = now - last_time;
        last_time = now;
        if (elapsed > MAX_ACCUMULATED_MS) elapsed = MAX_ACCUMULATED_MS;

        if (IsKeyPressed(KEY_P)) {
            state->paused = !state->paused;
        }

        int ticks_this_frame = 0;
        if (!state->paused) {
            accumulator += elapsed;
            while (accumulator >= UPDATE_DT_MS) {
                game_logic_tick(state, tile_size);
                accumulator -= UPDATE_DT_MS;
                ticks_this_frame++;
                if (ticks_this_frame >= MAX_TICKS_PER_FRAME) {
                    accumulator = 0.0;
                    break;
                }
            }
        } else {
            accumulator = 0.0;
        }

        handle_hotbar_shortcuts(state);
        handle_output_mode_toggle(state);
        handle_camera_pan(state, dt);
        handle_camera_zoom(state);

        Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), state->camera);
        int grid_x = (int)floorf(mouse_world.x / tile_size);
        int grid_y = (int)floorf(mouse_world.y / tile_size);

        bool mouse_over_toolbar = (GetMouseY() > screen_height - 70);
        placement_preview_t preview = compute_placement_preview(state, mouse_world, tile_size);

        handle_placement_input(state, preview, mouse_over_toolbar, tile_size);
        handle_removal_input(state, mouse_world, grid_x, grid_y, tile_size);
        handle_rotation_input(state, grid_x, grid_y);
        handle_pickup_input(state, grid_x, grid_y);
        handle_f_key_deletion(state, mouse_world, tile_size);

        BeginDrawing();
        ClearBackground((Color){ 20, 20, 20, 255 });

        BeginMode2D(state->camera);

        render_grid(state->camera, screen_width, screen_height, tile_size);
        render_buildings_base(state->buildings, state->building_count, tile_size);
        render_buildings_overlay(state->buildings, state->building_count, tile_size);
        render_belt_items(state->buildings, state->building_count, tile_size);
        render_ground_items(state->items, state->item_count, tile_size);
        render_placement_ghost(state, preview, mouse_over_toolbar, tile_size);

        EndMode2D();

        render_hud(state, grid_x, grid_y, screen_width, dt, ticks_this_frame);
        render_hotbar(state->current_building_idx, screen_width, screen_height);

        if (state->paused) {
            const char* pause_msg = "Press P to unpause";
            int text_w = MeasureText(pause_msg, 20);
            DrawText(pause_msg, (screen_width - text_w) / 2, 10, 20, RAYWHITE);
        }

        EndDrawing();
    }

    if (output_save_path != NULL) {
        save_world(output_save_path, state);
    }

    CloseWindow();
}

int main(int argc, char** argv) {
    const int tile_size = 64;

    game_state_t state = { 0 };
    state.current_building_idx = BUILDING_NONE;
    state.current_held_rotation = DIR_NORTH;
    state.current_drill_output_mode = ITEM_IRON_ORE;
    state.camera.zoom = 1.0f;
    state.paused = false;

    char* output_save_path = NULL;
    int run_ticks = -1;
    parse_args(argc, argv, &state, &output_save_path, &run_ticks);

    if (run_ticks > 0) {
        run_headless(&state, run_ticks, tile_size, output_save_path);
    } else {
        run_interactive(&state, tile_size, output_save_path);
    }

    free(state.buildings);
    free(state.items);
}
