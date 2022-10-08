#pragma once

#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <stdlib.h>
#include <stdbool.h>

#include "walk_sprites.h"

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
#define NUM_ROWS(array_2d) ARRAY_LEN(array_2d)
#define NUM_COLS(array_2d) ARRAY_LEN(array_2d[0])

#define DEBUG_TEXT 0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define START_X 40
#define START_Y 30
#define PLAYER_SPEED 2
#define PROJECTILE_SPEED 5
#define PROJECTILE_W 2
#define PROJECTILE_H 2

#define PLAYER_W 16
#define PLAYER_H 16
#define PLAYER_FRAMES 3

#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3


// 0= clock tick 1= key press
typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

// struct to hold events, to be put in event queue
typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    uint8_t x, y, speed, dir;
    bool visible;

} Projectile;

typedef struct {
    uint8_t x, y, speed, dir;
    bool is_moving;
    uint8_t frame;
    // uint8_t sprite[][PLAYER_H][PLAYER_W];
    uint8_t * sprite;
    Projectile projectile;
} Player;

// mutex to hold global states one might want to fuck with
typedef struct {
    Player player;

} PluginState;

static void draw_player(PluginState* const plugin_state, Canvas* const canvas, uint8_t sprite[][PLAYER_H][PLAYER_W]) {
    for(size_t row=0; row<PLAYER_H; row++) {
        for(size_t col=0; col<PLAYER_W; col++) {
            uint8_t f = plugin_state->player.frame;
            if(sprite[f][row][col]) {
                canvas_draw_dot(canvas, col + plugin_state->player.x, row + plugin_state->player.y);
            }
        }
    }
}

static void draw_projectile(PluginState* const plugin_state, Canvas* const canvas) {
    if(plugin_state->player.projectile.x < 0 || plugin_state->player.projectile.x > SCREEN_WIDTH
    || plugin_state->player.projectile.y < 0 || plugin_state->player.projectile.y > SCREEN_HEIGHT) {
        plugin_state->player.projectile.visible = false;
    }
    if(plugin_state->player.projectile.visible) {
        canvas_draw_box(canvas, plugin_state->player.projectile.x, plugin_state->player.projectile.y, PROJECTILE_W, PROJECTILE_H);
    }
}

static void shoot(PluginState* const plugin_state) {
    if(!plugin_state->player.projectile.visible) {
        plugin_state->player.projectile.dir = plugin_state->player.dir;
        uint8_t dir = plugin_state->player.projectile.dir;
        switch(dir) {
            case UP:
                plugin_state->player.projectile.x = plugin_state->player.x + (PLAYER_W / 2);
                plugin_state->player.projectile.y = plugin_state->player.y - PROJECTILE_H;
                break;
            case DOWN:
                plugin_state->player.projectile.x = plugin_state->player.x + (PLAYER_W / 2);
                plugin_state->player.projectile.y = plugin_state->player.y + PLAYER_H;
                break;
            case LEFT:
                plugin_state->player.projectile.x = plugin_state->player.x/* - PROJECTILE_W*/;
                plugin_state->player.projectile.y = plugin_state->player.y + (PLAYER_H / 2);
                break;
            case RIGHT:
                plugin_state->player.projectile.x = plugin_state->player.x + PLAYER_W - PROJECTILE_W;
                plugin_state->player.projectile.y = plugin_state->player.y + (PLAYER_H / 2);
                break;
        }
        plugin_state->player.projectile.visible = true;
    }
}

const NotificationSequence sequence_player_score = {
    &message_vibro_on,
    &message_green_255,
    &message_note_c4,
    &message_delay_50,
    &message_note_e4,
    &message_delay_50,
    &message_blue_255,
    &message_note_g5,
    &message_delay_50,
    &message_note_a5,
    &message_delay_100,
    &message_note_c6,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

const NotificationSequence sequence_cpu_score = {
    &message_vibro_on,
    &message_green_255,
    &message_note_ds4,
    &message_delay_50,
    &message_note_d4,
    &message_delay_50,
    &message_red_255,
    &message_note_a2,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

const NotificationSequence sequence_blip = {
    &message_vibro_on,
    &message_note_ds4,
    &message_delay_10,
    &message_note_f4,
    &message_delay_10,
    &message_note_g4,
    &message_delay_10,
    &message_sound_off,
    &message_vibro_off,
    NULL,
};

static void draw_all(PluginState* const plugin_state, Canvas* const canvas) {

    // player
    uint8_t dir = plugin_state->player.dir;
    switch(dir) {
        case UP:
            draw_player(plugin_state, canvas, up_array);
            break;
        case DOWN:
            draw_player(plugin_state, canvas, down_array);
            break;
        case LEFT:
            draw_player(plugin_state, canvas, left_array);
            break;
        case RIGHT:
            draw_player(plugin_state, canvas, right_array);
            break;
    }


    // projectile
    draw_projectile(plugin_state, canvas);
}

// pass plugin state pointer to have its x,y set to default
static void walk_state_init(PluginState* const plugin_state) {
    // player walk stuff init
    plugin_state->player.x = 30;
    plugin_state->player.y = 30;
    plugin_state->player.speed = 4;
    plugin_state->player.dir = DOWN;
    plugin_state->player.is_moving = false;
    // player shoot stuff init
    plugin_state->player.projectile.visible = false;
    plugin_state->player.projectile.dir = plugin_state->player.dir;
    plugin_state->player.projectile.speed = PROJECTILE_SPEED;

}


static void process_step(PluginState* const plugin_state, NotificationApp* notify) {
    UNUSED(notify);

    // player move logic
    if(plugin_state->player.is_moving) {
        // iterate frame
        plugin_state->player.frame = (plugin_state->player.frame + 1) % 3;
        if(!plugin_state->player.frame) plugin_state->player.frame++;


        uint8_t d = plugin_state->player.dir;
        switch(d) {
            case UP:
                plugin_state->player.y -= plugin_state->player.speed;
                break;
            case DOWN:
                plugin_state->player.y += plugin_state->player.speed;
                break;
            case LEFT:
                plugin_state->player.x -= plugin_state->player.speed;
                break;
            case RIGHT:
                plugin_state->player.x += plugin_state->player.speed;
                break;
        }
    } else {
        plugin_state->player.frame = 0;
    }

    // player projectile logic
    if(plugin_state->player.projectile.visible) {
        uint8_t d = plugin_state->player.projectile.dir;
        switch(d) {
            case UP:
                plugin_state->player.projectile.y -= plugin_state->player.projectile.speed;
                break;
            case DOWN:
                plugin_state->player.projectile.y += plugin_state->player.projectile.speed;
                break;
            case LEFT:
                plugin_state->player.projectile.x -= plugin_state->player.projectile.speed;
                break;
            case RIGHT:
                plugin_state->player.projectile.x += plugin_state->player.projectile.speed;
                break;
        }
    }

}