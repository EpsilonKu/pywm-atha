#ifndef WM_WIDGET_H
#define WM_WIDGET_H

#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/render/wlr_texture.h>

#include "wm_content.h"

struct wm_server;

struct wm_widget {
    struct wm_content super;

    /*
     * Widgets are fixed to one output - only for comparison; don't dereference
     */
    struct wm_output* wm_output;

    /* Either texture needs to be set, or primitive */
    struct wlr_texture* wlr_texture;
    struct {
        char* name;
        int n_params_int;
        int* params_int;
        int n_params_float;
        float* params_float;
    } primitive;
};

void wm_widget_init(struct wm_widget* widget, struct wm_server* server);

void wm_widget_set_pixels(struct wm_widget* widget, uint32_t format, uint32_t stride, uint32_t width, uint32_t height, const void* data);

void wm_widget_set_primitive(struct wm_widget* widget, char* name, int n_params_int, int* params_int, int n_params_float, float* params_float);

#endif
