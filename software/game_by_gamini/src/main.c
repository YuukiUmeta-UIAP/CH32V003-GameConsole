#include "driver.h"
#include "spritebank.h"
#include <stdbool.h>
#include <string.h> // For memset

// ===================================================================================
// --- Constants & Globals ---
// ===================================================================================

#define ANIMATION_FRAMES 4 // Animate over 4 frames

// Tile data structure
typedef struct {
    uint16_t value;
    uint8_t x, y;           // Current grid position
    uint8_t prev_x, prev_y; // Position before move (for animation)
    bool is_new;
    bool is_merged;
    bool to_remove;      // Flag for removal after merge
} Tile;

Tile tiles[16];
uint8_t tile_count = 0;

// Screen buffer
uint8_t screen_buffer[128 * 8];

// Animation state
bool is_animating = false;
uint8_t animation_frame = 0;

// --- Function Prototypes ---
void init_game();
bool add_random_tile();
void draw_frame();
bool move(uint8_t direction);
bool is_game_over();
void update_display();
void cleanup_tiles();
void draw_number_to_buffer(int16_t x, int16_t y, uint16_t num, bool color);
void draw_char_to_buffer(int16_t x, int16_t y, char c, bool color);

// ===================================================================================
// --- Graphics Functions ---
// ===================================================================================

void clear_buffer() { memset(screen_buffer, 0, sizeof(screen_buffer)); }

void set_pixel(int16_t x, int16_t y, bool color) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    uint16_t index = x + (y / 8) * 128;
    uint8_t bit_pos = y % 8;
    if (color) screen_buffer[index] |= (1 << bit_pos);
    else screen_buffer[index] &= ~(1 << bit_pos);
}

void update_display() {
    for (uint8_t y = 0; y < 8; y++) {
        OLED_setpos(0, y);
        OLED_data_start();
        for (uint8_t x = 0; x < 128; x++) {
            I2C_write(screen_buffer[x + y * 128]);
        }
        I2C_stop();
    }
}

void draw_rect(int16_t x, int16_t y, uint8_t w, uint8_t h, bool color) {
    for(int16_t i = x; i < x+w; i++) {
        for(int16_t j = y; j < y+h; j++) {
            set_pixel(i, j, color);
        }
    }
}

void draw_char_to_buffer(int16_t x, int16_t y, char c, bool color) {
    uint8_t digit = c - '0';
    if (digit > 9) return;
    uint8_t font_width = font_4x8[0];
    const uint8_t* font_data = &font_4x8[2 + digit * font_width];
    for (uint8_t col = 0; col < font_width; col++) {
        for (uint8_t row = 0; row < 8; row++) {
            if ((font_data[col] >> row) & 1) {
                set_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_number_to_buffer(int16_t x, int16_t y, uint16_t num, bool color) {
    char str[5];
    uint8_t len = 0;
    if (num == 0) { str[0] = '0'; len = 1; }
    else {
        uint16_t temp = num;
        while (temp > 0 && len < 5) {
            str[len++] = (temp % 10) + '0';
            temp /= 10;
        }
    }
    uint8_t font_width = font_4x8[0];
    uint8_t total_width = len * (font_width + 1) - 1;
    int16_t current_x = x - total_width / 2;
    for (int8_t i = len - 1; i >= 0; i--) {
        draw_char_to_buffer(current_x, y, str[i], color);
        current_x += font_width + 1;
    }
}

void draw_frame() {
    clear_buffer();
    int8_t progress = is_animating ? animation_frame : ANIMATION_FRAMES;

    // Draw background grid
    for(int i=0; i<=4; i++) {
        draw_rect(32 + i*16, 0, 1, 64, true);
        draw_rect(32, i*16, 65, 1, true);
    }

    for (uint8_t i = 0; i < tile_count; i++) {
        Tile* t = &tiles[i];
        if (t->value == 0) continue;

        int16_t render_x = (t->prev_x * (ANIMATION_FRAMES - progress) + t->x * progress) * 16 / ANIMATION_FRAMES;
        int16_t render_y = (t->prev_y * (ANIMATION_FRAMES - progress) + t->y * progress) * 16 / ANIMATION_FRAMES;

        uint8_t screen_x = 32 + render_x;
        uint8_t screen_y = render_y;

        uint8_t tile_size = 14;
        uint8_t offset = 1;

        if (t->is_new && is_animating) {
            tile_size = (progress * 14) / ANIMATION_FRAMES;
            offset = (16 - tile_size) / 2;
        }

        // Draw tile background (white)
        draw_rect(screen_x + offset, screen_y + offset, tile_size, tile_size, true);
        
        // Draw number (black) only when tile is full size
        if (!is_animating || !t->is_new || progress >= ANIMATION_FRAMES) {
             draw_number_to_buffer(screen_x + 8, screen_y + 4, t->value, false);
        }
    }
    update_display();
}

// ===================================================================================
// --- Game Logic ---
// ===================================================================================

void cleanup_tiles() {
    uint8_t new_count = 0;
    for (uint8_t i = 0; i < tile_count; i++) {
        if (!tiles[i].to_remove) {
            if (i != new_count) {
                tiles[new_count] = tiles[i];
            }
            new_count++;
        }
    }
    tile_count = new_count;
}

void prepare_for_move() {
    for (uint8_t i = 0; i < tile_count; i++) {
        tiles[i].prev_x = tiles[i].x;
        tiles[i].prev_y = tiles[i].y;
        tiles[i].is_merged = false;
        tiles[i].is_new = false;
        tiles[i].to_remove = false;
    }
}

void init_game() {
    tile_count = 0;
    add_random_tile();
    add_random_tile();
}

bool add_random_tile() {
    if (tile_count >= 16) return false;

    uint8_t grid[4][4] = {0};
    for(uint8_t i=0; i<tile_count; i++) grid[tiles[i].y][tiles[i].x] = 1;

    uint8_t empty_cells[16][2];
    uint8_t count = 0;
    for (uint8_t y = 0; y < 4; y++) {
        for (uint8_t x = 0; x < 4; x++) {
            if (grid[y][x] == 0) {
                empty_cells[count][0] = y;
                empty_cells[count][1] = x;
                count++;
            }
        }
    }

    if (count > 0) {
        uint16_t index = JOY_random() % count;
        uint8_t y = empty_cells[index][0];
        uint8_t x = empty_cells[index][1];
        
        tiles[tile_count].value = (JOY_random() % 10 == 0) ? 4 : 2;
        tiles[tile_count].x = x; tiles[tile_count].y = y;
        tiles[tile_count].prev_x = x; tiles[tile_count].prev_y = y;
        tiles[tile_count].is_new = true; tiles[tile_count].is_merged = false; tiles[tile_count].to_remove = false;
        tile_count++;
        return true;
    }
    return false;
}

bool move(uint8_t direction) {
    bool moved = false;
    prepare_for_move();

    int8_t vec_x = (direction == 0) ? -1 : (direction == 2) ? 1 : 0;
    int8_t vec_y = (direction == 1) ? -1 : (direction == 3) ? 1 : 0;

    Tile* grid[4][4] = {NULL};
    for(uint8_t i=0; i<tile_count; i++) grid[tiles[i].y][tiles[i].x] = &tiles[i];

    int8_t x_start = (vec_x > 0) ? 3 : 0, x_end = (vec_x > 0) ? -1 : 4, x_step = (vec_x > 0) ? -1 : 1;
    int8_t y_start = (vec_y > 0) ? 3 : 0, y_end = (vec_y > 0) ? -1 : 4, y_step = (vec_y > 0) ? -1 : 1;

    for (int y = y_start; y != y_end; y += y_step) {
        for (int x = x_start; x != x_end; x += x_step) {
            Tile* tile = grid[y][x];
            if (tile == NULL) continue;

            int8_t farthest_y = y, farthest_x = x;
            int8_t test_y = y + vec_y, test_x = x + vec_x;

            while (test_x >= 0 && test_x < 4 && test_y >= 0 && test_y < 4) {
                if (grid[test_y][test_x] != NULL) {
                    if (grid[test_y][test_x]->value == tile->value && !grid[test_y][test_x]->is_merged) {
                        farthest_y = test_y; farthest_x = test_x;
                    }
                    break;
                }
                farthest_y = test_y; farthest_x = test_x;
                test_y += vec_y; test_x += vec_x;
            }

            if (farthest_y != y || farthest_x != x) {
                Tile* target_tile = grid[farthest_y][farthest_x];
                if (target_tile != NULL) { // Merge
                    target_tile->value *= 2;
                    target_tile->is_merged = true;
                    tile->to_remove = true;
                } else { // Move
                    grid[farthest_y][farthest_x] = tile;
                }
                tile->y = farthest_y; tile->x = farthest_x;
                grid[y][x] = NULL;
                moved = true;
            }
        }
    }
    return moved;
}

bool is_game_over() {
    if (tile_count < 16) return false;
    Tile* grid[4][4] = {NULL};
    for(uint8_t i=0; i<tile_count; i++) grid[tiles[i].y][tiles[i].x] = &tiles[i];

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            Tile* tile = grid[y][x];
            if (tile == NULL) return false; // Should not happen if tile_count is 16
            if (x < 3 && grid[y][x+1] != NULL && tile->value == grid[y][x+1]->value) return false;
            if (y < 3 && grid[y+1][x] != NULL && tile->value == grid[y+1][x]->value) return false;
        }
    }
    return true;
}

// ===================================================================================
// --- Main Loop ---
// ===================================================================================

int main(void) {
    JOY_init();
    init_game();
    draw_frame();

    while(1) {
        if (is_animating) {
            draw_frame();
            animation_frame++;
            if (animation_frame > ANIMATION_FRAMES) {
                is_animating = false;
                cleanup_tiles();
                draw_frame(); // Draw final, clean state
                if (is_game_over()) {
                    // Game over logic here
                    while(1);
                }
            }
        } else {
            uint8_t direction = 5; // 5 = no move
            if (JOY_up_pressed()) { direction = 1; while(JOY_up_pressed()); }
            else if (JOY_down_pressed()) { direction = 3; while(JOY_down_pressed()); }
            else if (JOY_left_pressed()) { direction = 0; while(JOY_left_pressed()); }
            else if (JOY_right_pressed()) { direction = 2; while(JOY_right_pressed()); }

            if(direction != 5) {
                if (move(direction)) {
                    add_random_tile();
                    is_animating = true;
                    animation_frame = 0;
                }
            }
        }
        JOY_DLY_ms(30);
    }
}