#ifndef WM_LAYOUT_H
#define WM_LAYOUT_H

#include <stdio.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>

struct wm_server;
struct wm_view;
struct wm_content;
struct wm_output;

struct wm_layout {
    struct wm_server* wm_server;

    struct wlr_output_layout* wlr_output_layout;
    struct wl_list wm_outputs; // wm_output::link

    struct wl_listener change;

    int refresh_master_output;
    int refresh_scheduled;
};

void wm_layout_init(struct wm_layout* layout, struct wm_server* server);
void wm_layout_destroy(struct wm_layout* layout);

void wm_layout_add_output(struct wm_layout* layout, struct wlr_output* output);
void wm_layout_remove_output(struct wm_layout* layout, struct wm_output* output);

void wm_layout_reconfigure(struct wm_layout* layout);

/* Damage whole output layout */
void wm_layout_damage_whole(struct wm_layout* layout);

/* Calls wm_content_damage_output, expects calls to wm_layout_damage_output */
void wm_layout_damage_from(struct wm_layout* layout, struct wm_content* content, struct wlr_surface* origin);
void wm_layout_damage_output(struct wm_layout* layout, struct wm_output* output, pixman_region32_t* damage, struct wm_content* from);

void wm_layout_start_update(struct wm_layout* layout);
int wm_layout_get_refresh_output(struct wm_layout* layout);

void wm_layout_update_content_outputs(struct wm_layout* layout, struct wm_content* content);

void wm_layout_printf(FILE* file, struct wm_layout* layout);


#endif
