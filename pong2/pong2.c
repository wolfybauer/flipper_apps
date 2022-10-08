#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#include "pong2.h"

static void render_callback(Canvas* const canvas, void* ctx) {
    // try to grab the x,y state if resource available
    const PluginState* plugin_state = acquire_mutex((ValueMutex*)ctx, 25);
    // return if not available
    if(plugin_state == NULL) {
        return;
    }
    // draw border, font. see canvas.h
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);

    draw_all(plugin_state, canvas);

    // release resource
    release_mutex((ValueMutex*)ctx, plugin_state);
}

static void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    // DEBUG MODE ONLY: crash program if unable to access message queue
    furi_assert(event_queue);
    // make event for button press, add to queue
    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void timer_callback(FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PluginEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}


// aka main() . follow int32_t <yourappname>_app() format
int32_t pong_app() {
    // build message queue of length 8, for PluginEvents
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));
    // build plugin state
    PluginState* plugin_state = malloc(sizeof(PluginState));
    // set init values
    pong_state_init(plugin_state);
    // build mutex to hold
    ValueMutex state_mutex;
    // pass ref to the mutex, the data, size of data's type. see valuemutex.h
    if(!init_mutex(&state_mutex, plugin_state, sizeof(PluginState))) {
        // if fail, release resources and exit w return code 255
        FURI_LOG_E("Hello_world", "cannot create mutex\r\n");
        free(plugin_state);
        return 255;
    }

    // build the viewport
    ViewPort* view_port = view_port_alloc();
    // pass render callback, mutex variables that will affect drawing
    view_port_draw_callback_set(view_port, render_callback, &state_mutex);
    // pass input callback, event queue pointer to use as input for viewport
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // build the timer
    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 4);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    //build event
    PluginEvent event;

    // MAIN LOOP
    for(bool processing = true; processing;) {
        // check if message waiting
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);
        // wait until state data mutex is available
        PluginState* plugin_state = (PluginState*)acquire_mutex_block(&state_mutex);
        // if event
        if(event_status == FuriStatusOk) {
            // key press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        if(plugin_state->player_y > 2) {
                            if(plugin_state->player_y - plugin_state->player_speed < 2) {
                                plugin_state->player_y = 2;
                            } else {
                                plugin_state->player_y -= plugin_state->player_speed;
                            }
                        }
                        break;
                    case InputKeyDown:
                        if((plugin_state->player_y + PADDLE_H) < SCREEN_HEIGHT) {
                            if((plugin_state->player_y + PADDLE_H + plugin_state->player_speed) > SCREEN_HEIGHT) {
                                plugin_state->player_y = SCREEN_HEIGHT - PADDLE_H;
                            } else {
                                plugin_state->player_y += plugin_state->player_speed;
                            }
                        }
                        break;
                    case InputKeyRight:
                        break;
                    case InputKeyLeft:
                        break;
                    case InputKeyOk:
                        //plugin_state->ball_speed = (plugin_state->ball_speed + 2) % 10;
                        //plugin_state->player_speed = (plugin_state->player_speed + 2) % 10;
                        plugin_state->is_muted = !plugin_state->is_muted;
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    }
                }
            } else if(event.type == EventTypeTick) {
                process_step(plugin_state, notification);
            }
        } else {
            FURI_LOG_D("Pong", "FuriMessageQueue: event timeout");
            // event timeout
        }
        // after getting input + updating data, update screen
        view_port_update(view_port);
        // release state data resource
        release_mutex(&state_mutex, plugin_state);
    }
    // free the timer
    furi_timer_free(timer);
    // stop the viewport
    view_port_enabled_set(view_port, false);
    // remove viewport from gui
    gui_remove_view_port(gui, view_port);
    // close the gui
    furi_record_close(RECORD_GUI);
    // close notification
    furi_record_close(RECORD_NOTIFICATION);
    // delete the viewport
    view_port_free(view_port);
    // delete the message queue
    furi_message_queue_free(event_queue);
    // delete mutex
    delete_mutex(&state_mutex);
    // free plugin state
    free(plugin_state);

    return 0;
}