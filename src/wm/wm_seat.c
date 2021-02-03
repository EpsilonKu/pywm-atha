#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>
#include "wm/wm.h"
#include "wm/wm_seat.h"
#include "wm/wm_view.h"
#include "wm/wm_server.h"
#include "wm/wm_keyboard.h"
#include "wm/wm_pointer.h"
#include "wm/wm_cursor.h"

/*
 * Callbacks
 */
static void handle_destroy(struct wl_listener* listener, void* data){
	wlr_log(WLR_DEBUG, "Destroying seat...");
    struct wm_seat* seat = wl_container_of(listener, seat, destroy);
    wm_seat_destroy(seat);
}

/*
 * Class implementation
 */
void wm_seat_init(struct wm_seat* seat, struct wm_server* server, struct wm_layout* layout){
    seat->wm_server = server;
    wl_list_init(&seat->wm_keyboards);
    wl_list_init(&seat->wm_pointers);

    seat->wlr_seat = wlr_seat_create(server->wl_display, "default");
    assert(seat->wlr_seat);

    seat->wm_cursor = calloc(1, sizeof(struct wm_cursor));
    wm_cursor_init(seat->wm_cursor, seat, layout);

    seat->destroy.notify = handle_destroy;
    wl_signal_add(&seat->wlr_seat->events.destroy, &seat->destroy);
}

void wm_seat_destroy(struct wm_seat* seat) {
    wl_list_remove(&seat->destroy.link);

    /* TODO */
}

void wm_seat_add_input_device(struct wm_seat* seat, struct wlr_input_device* input_device){
    switch(input_device->type){
    case WLR_INPUT_DEVICE_KEYBOARD:
        wlr_log(WLR_DEBUG, "New keyboard");

        struct wm_keyboard* keyboard = calloc(1, sizeof(struct wm_keyboard));
        wm_keyboard_init(keyboard, seat, input_device);
        wl_list_insert(&seat->wm_keyboards, &keyboard->link);

        wlr_seat_set_keyboard(seat->wlr_seat, keyboard->wlr_input_device);
        break;

    case WLR_INPUT_DEVICE_POINTER:
        wlr_log(WLR_DEBUG, "New pointer");

        struct wm_pointer* pointer = calloc(1, sizeof(struct wm_pointer));
        wm_pointer_init(pointer, seat, input_device);
        wl_list_insert(&seat->wm_pointers, &pointer->link);

        wm_cursor_add_pointer(seat->wm_cursor, pointer);
        break;
    case WLR_INPUT_DEVICE_TOUCH:
    case WLR_INPUT_DEVICE_SWITCH:
    case WLR_INPUT_DEVICE_TABLET_TOOL:
    case WLR_INPUT_DEVICE_TABLET_PAD:
        wlr_log(WLR_DEBUG, "Unsupported input device");
    }

    uint32_t capabilities = 0;
    if(!wl_list_empty(&seat->wm_keyboards)){
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if(!wl_list_empty(&seat->wm_pointers)){
        capabilities |= WL_SEAT_CAPABILITY_POINTER;
    }

    wlr_seat_set_capabilities(seat->wlr_seat, capabilities);
        wlr_log(WLR_DEBUG, "...done");
}

static void activate_surface(struct wlr_surface* surface, bool activated){
    if(!surface) return;

    if(wlr_surface_is_xdg_surface(surface)){
        struct wlr_xdg_surface* xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);
        if(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL){
            wlr_xdg_toplevel_set_activated(xdg_surface, activated);
        }
        return;
    }

    if(wlr_surface_is_xwayland_surface(surface)){
        wlr_xwayland_surface_activate(wlr_xwayland_surface_from_wlr_surface(surface), activated);
        return;
    }

    wlr_log(WLR_ERROR, "Unknown surface implementation");
}


void wm_seat_focus_surface(struct wm_seat* seat, struct wlr_surface* surface){
    struct wlr_surface* prev = seat->wlr_seat->keyboard_state.focused_surface;
    if(prev == surface){
        return;
    }

    if(prev && surface){
        struct wm_view* prev_view = wm_server_view_for_surface(seat->wm_server, prev);
        struct wm_view* view = wm_server_view_for_surface(seat->wm_server, surface);

        if(prev_view && prev_view == view) return;
    }

    activate_surface(surface, true);

    struct wm_view* prev_view = wm_server_view_for_surface(seat->wm_server, prev);
    struct wm_view* view = wm_server_view_for_surface(seat->wm_server, surface);


    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat->wlr_seat);
    wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface,
            keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);

    if(view && view != prev_view){
        wm_callback_view_focused(view);
    }
}

void wm_seat_dispatch_key(struct wm_seat* seat, struct wlr_input_device* input_device, struct wlr_event_keyboard_key* event){
    wlr_seat_set_keyboard(seat->wlr_seat, input_device);
    wlr_seat_keyboard_notify_key(seat->wlr_seat, event->time_msec, event->keycode, event->state);
}

void wm_seat_dispatch_modifiers(struct wm_seat* seat, struct wlr_input_device* input_device){
    wlr_seat_set_keyboard(seat->wlr_seat, input_device);
    wlr_seat_keyboard_notify_modifiers(seat->wlr_seat, &input_device->keyboard->modifiers);
}

bool wm_seat_dispatch_motion(struct wm_seat* seat, double x, double y, uint32_t time_msec){
    struct wlr_surface* surface;
    double sx;
    double sy;

    wm_server_surface_at(seat->wm_server, x, y, &surface, &sx, &sy);
    if(!surface){
        wlr_seat_pointer_clear_focus(seat->wlr_seat);
        return false;
    }

    /* 
     * TODO! Configurable!
     * Automatically focus surface on mouse enter 
     */
    wm_seat_focus_surface(seat, surface);

    bool focus_change = (surface != seat->wlr_seat->pointer_state.focused_surface);
    wlr_seat_pointer_notify_enter(seat->wlr_seat, surface, sx, sy);
    if(!focus_change){
        wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);
    }

    return true;
}

void wm_seat_dispatch_button(struct wm_seat* seat, struct wlr_event_pointer_button* event){
    wlr_seat_pointer_notify_button(seat->wlr_seat, event->time_msec, event->button, event->state);
}
void wm_seat_dispatch_axis(struct wm_seat* seat, struct wlr_event_pointer_axis* event){
    wlr_seat_pointer_notify_axis(seat->wlr_seat,
            event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source);
}
