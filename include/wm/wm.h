#ifndef WM_H
#define WM_H

#include <time.h>
#include <stdbool.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>

struct wm_view;
struct wm_server;
struct wm_layout;
struct wm_widget;

struct wm {
    struct wm_server* server;

    void (*callback_layout_change)(struct wm_layout*);
    bool (*callback_key)(struct wlr_event_keyboard_key*, const char* keysyms);
    bool (*callback_modifiers)(struct wlr_keyboard_modifiers*);
    bool (*callback_motion)(double, double, uint32_t);
    bool (*callback_motion_absolute)(double, double, uint32_t);
    bool (*callback_button)(struct wlr_event_pointer_button*);
    bool (*callback_axis)(struct wlr_event_pointer_axis*);
    void (*callback_init_view)(struct wm_view*);
    void (*callback_destroy_view)(struct wm_view*);
    void (*callback_view_event)(struct wm_view*, const char* event);

    /* Once the server is ready, and we can create new threads */
    void (*callback_ready)(void);

    /* Synchronous update once per frame */
    void (*callback_update)(void);
};

void wm_init();
void wm_destroy();
int wm_run();
void wm_terminate();

void wm_focus_view(struct wm_view* view);
void wm_update_cursor(int cursor_visible);

void wm_set_locked(double locked);

struct wm_widget* wm_create_widget();
void wm_destroy_widget(struct wm_widget* widget);

/*
 * Instead of writing setters for every single callback,
 * just put them in this object
 */
struct wm* get_wm();

void wm_callback_layout_change(struct wm_layout* layout);

/*
 * Return false if event should be dispatched to clients
 */
bool wm_callback_key(struct wlr_event_keyboard_key* event, const char* keysyms);
bool wm_callback_modifiers(struct wlr_keyboard_modifiers* modifiers);
bool wm_callback_motion(double delta_x, double delta_y, uint32_t time_msec);
bool wm_callback_motion_absolute(double x, double y, uint32_t time_msec);
bool wm_callback_button(struct wlr_event_pointer_button* event);
bool wm_callback_axis(struct wlr_event_pointer_axis* event);

/*
 * Should set display_x, display_y, display_height, display_width
 * Can also call set_size
 */
void wm_callback_init_view(struct wm_view* view);
void wm_callback_destroy_view(struct wm_view* view);
void wm_callback_view_event(struct wm_view* view, const char* event);

void wm_callback_update();
void wm_callback_ready();

#endif
