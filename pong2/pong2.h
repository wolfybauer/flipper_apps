#pragma once

#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
//#include <dolphin/dolphin.h>
#include <stdlib.h>
#include <stdbool.h>

#define DEBUG_TEXT 1

#define PADDLE_W 2
#define PADDLE_H 12
#define BALL_W 2
#define SCREEN_WIDTH 126
#define SCREEN_HEIGHT 62
#define CPU_X 5
#define PLAYER_X 122
#define PLAYER_SCORE_X 115
#define CPU_SCORE_X 15
#define SCORE_Y 10


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

// mutex to hold global states one might want to fuck with
typedef struct {
    uint8_t ball_x, ball_y;
    int8_t ball_xspeed, ball_yspeed;
    float ball_slope, actual_y;
    uint8_t cpu_score, player_score;
    uint8_t cpu_y, player_y;
    uint8_t player_speed, cpu_speed;

    bool is_muted;

} PluginState;

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
    // draw the scores
    char buf_c[6];
    char buf_p[6];
    snprintf(buf_c, sizeof(buf_c), "%u", plugin_state->cpu_score);
    snprintf(buf_p, sizeof(buf_p), "%u", plugin_state->player_score);
    canvas_draw_str_aligned(canvas, CPU_SCORE_X, SCORE_Y, AlignRight, AlignBottom, buf_c);
    canvas_draw_str_aligned(canvas, PLAYER_SCORE_X, SCORE_Y, AlignRight, AlignBottom, buf_p);

    // put any debug text to be seen here
    if(DEBUG_TEXT) {
        char x_buf[6];
        char y_buf[6];
        snprintf(x_buf, sizeof(x_buf), "%.2f", plugin_state->actual_y);
        snprintf(y_buf, sizeof(y_buf), "%d", plugin_state->ball_y);
        canvas_draw_str_aligned(canvas, 30, 20, AlignRight, AlignBottom, x_buf);
        canvas_draw_str_aligned(canvas, 50, 20, AlignRight, AlignBottom, y_buf);
    }

    // draw the paddles
    canvas_draw_box(canvas, CPU_X, plugin_state->cpu_y, PADDLE_W, PADDLE_H);
    canvas_draw_box(canvas, PLAYER_X, plugin_state->player_y, PADDLE_W, PADDLE_H);

    //draw the ball
    canvas_draw_box(canvas, plugin_state->ball_x, plugin_state->ball_y, BALL_W, BALL_W);
}

static void reset_ball(PluginState* const plugin_state) {
    plugin_state->ball_x = 64;
    plugin_state->ball_y = 32;
    plugin_state->ball_xspeed = (uint8_t)((furi_hal_random_get() % (5 - 2 + 1)) + 2);
    plugin_state->ball_yspeed = (uint8_t)((furi_hal_random_get() % (5 - 2 + 1)) + 2);
    uint8_t xr = (uint8_t)(furi_hal_random_get() % 2);
    uint8_t yr = (uint8_t)(furi_hal_random_get() % 2);
    if(xr) plugin_state->ball_xspeed *= -1;
    if(yr) plugin_state->ball_yspeed *= -1;
    
}

// pass plugin state pointer to have its x,y set to default
static void pong_state_init(PluginState* const plugin_state) {
    plugin_state->is_muted = false;
    reset_ball(plugin_state);
    plugin_state->cpu_y = 32 - (PADDLE_H / 2);
    plugin_state->player_y = 32 - (PADDLE_H / 2);
    plugin_state->cpu_score = 0;
    plugin_state->player_score = 0;
    plugin_state->player_speed = 4;
    plugin_state->cpu_speed = 4;
}


static void process_step(PluginState* const plugin_state, NotificationApp* notify) {

    // ball wall collision checking
    if(plugin_state->ball_y >= SCREEN_HEIGHT || plugin_state->ball_y <= 2) {
        if(!plugin_state->is_muted) {
            notification_message(notify, &sequence_blip);
        }
        plugin_state->ball_yspeed *= -1;
    }

    plugin_state->ball_slope = (float)(plugin_state->ball_yspeed) / (float)(plugin_state->ball_xspeed);

    // player collision check
    if((plugin_state->ball_y + BALL_W) <= (plugin_state->player_y + PADDLE_H) && plugin_state->ball_y >= plugin_state->player_y) {
        if((plugin_state->ball_x + BALL_W + plugin_state->ball_xspeed) > PLAYER_X) {
            // correct ball pos if necessary
            plugin_state->actual_y = plugin_state->ball_y + ((float)(plugin_state->ball_x)/(float)(PLAYER_X + BALL_W)) * plugin_state->ball_yspeed;
            plugin_state->ball_x = PLAYER_X - BALL_W;
            uint8_t loc = plugin_state->ball_y - plugin_state->player_y;
            if(loc < 2) {
                plugin_state->ball_xspeed = -2;
                plugin_state->ball_yspeed = -6;
            } else if(loc >= 2 && loc < 4) {
                plugin_state->ball_xspeed = -3;
                plugin_state->ball_yspeed = -4;
            } else if(loc >= 4 && loc < 8) {
                plugin_state->ball_xspeed = -4;
                if(plugin_state->ball_yspeed > 0) {
                    plugin_state->ball_yspeed = -4;
                } else {
                    plugin_state->ball_yspeed = 4;
                }
            } else if(loc >= 8 && loc < 10) {
                plugin_state->ball_xspeed = -3;
                plugin_state->ball_yspeed = 4;
            } else if(loc >= 10 && loc < 12) {
                plugin_state->ball_xspeed = -2;
                plugin_state->ball_yspeed = -6;
            }
            // do alert
            if(!plugin_state->is_muted) {
                notification_message(notify, &sequence_blip);
            }
        }
    }

    // cpu collision check
    if((plugin_state->ball_y + BALL_W) <= (plugin_state->cpu_y + PADDLE_H) && plugin_state->ball_y >= plugin_state->cpu_y) {
        if((plugin_state->ball_x + plugin_state->ball_xspeed) < CPU_X + PADDLE_W) {
            // correct ball pos
            plugin_state->actual_y = plugin_state->ball_y + ((float)(plugin_state->ball_x)/(float)(CPU_X + PADDLE_W)) * plugin_state->ball_yspeed;
            plugin_state->ball_x = CPU_X + PADDLE_W;
            uint8_t loc = plugin_state->ball_y - plugin_state->cpu_y;
            if(loc < 2) {
                plugin_state->ball_xspeed = 2;
                plugin_state->ball_yspeed = -6;
            } else if(loc >= 2 && loc < 4) {
                plugin_state->ball_xspeed = 3;
                plugin_state->ball_yspeed = -4;
            } else if(loc >= 4 && loc < 8) {
                plugin_state->ball_xspeed = 4;
                if(plugin_state->ball_yspeed > 0) {
                    plugin_state->ball_yspeed = -4;
                } else {
                    plugin_state->ball_yspeed = 4;
                }
            } else if(loc >= 8 && loc < 10) {
                plugin_state->ball_xspeed = 3;
                plugin_state->ball_yspeed = 4;
            } else if(loc >= 10 && loc < 12) {
                plugin_state->ball_xspeed = 2;
                plugin_state->ball_yspeed = 6;
            }

            // do alert
            if(!plugin_state->is_muted) {
                notification_message(notify, &sequence_blip);
            }
        }
    }

    // cpu score
    if(plugin_state->ball_x >= SCREEN_WIDTH) {
        plugin_state->cpu_score += 1;
        if(!plugin_state->is_muted) {
            notification_message(notify, &sequence_cpu_score);
        }
        reset_ball(plugin_state);
    }
    // player score
    if(plugin_state->ball_x <= 2) {
        plugin_state->player_score += 1;
        if(!plugin_state->is_muted) {
            notification_message(notify, &sequence_player_score);
        }
        reset_ball(plugin_state);
    }

    // cpu ai
    if((plugin_state->cpu_y + PADDLE_H/2) < (plugin_state->ball_y + BALL_W/2)) {
        if((plugin_state->cpu_y + PADDLE_H + plugin_state->cpu_speed) > SCREEN_HEIGHT) {
            plugin_state->cpu_y = SCREEN_HEIGHT - PADDLE_H;
        } else {
            plugin_state->cpu_y += plugin_state->cpu_speed;
        }
    }
    if((plugin_state->cpu_y + PADDLE_H/2) > (plugin_state->ball_y + BALL_W/2)) {
        if(plugin_state->cpu_y - plugin_state->cpu_speed < 2) {
            plugin_state->cpu_y = 2;
        } else {
            plugin_state->cpu_y -= plugin_state->cpu_speed;
        }
    }

    // updte ball position
    plugin_state->ball_x += plugin_state->ball_xspeed;
    // else += distance to paddle
    plugin_state->ball_y += plugin_state->ball_yspeed;
    // else += d 
}