#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>

#include "wm/wm_server.h"
#include "wm/wm_util.h"
#include "wm/wm.h"
#include "wm/wm_seat.h"
#include "wm/wm_view_xdg.h"
#include "wm/wm_view_xwayland.h"
#include "wm/wm_layout.h"
#include "wm/wm_widget.h"
#include "wm/wm_config.h"
#include "wm/wm_output.h"


/*
 * Callbacks
 */
static void handle_new_input(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Server: New input");

    struct wm_server* server = wl_container_of(listener, server, new_input);
    struct wlr_input_device* input_device = data;

    wm_seat_add_input_device(server->wm_seat, input_device);
}

static void handle_new_output(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Server: New output");

    struct wm_server* server = wl_container_of(listener, server, new_output);
    struct wlr_output* output = data;

    wm_layout_add_output(server->wm_layout, output);
}

static void handle_new_xdg_surface(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Server: New xdg surface");

    struct wm_server* server = wl_container_of(listener, server, new_xdg_surface);
    struct wlr_xdg_surface* surface = data;

    /* Let clients know which (only one is supported) output they're on */
    if(server->wm_layout->default_output) {
        wlr_surface_send_enter(surface->surface,
                server->wm_layout->default_output->wlr_output);
    }

    if(surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL){
        /* Popups should be handled by the parent */
        return;
    }

    wlr_xdg_surface_ping(surface);

    struct wm_view_xdg* view = calloc(1, sizeof(struct wm_view_xdg));
    wm_view_xdg_init(view, server, surface);
}

static void handle_new_xwayland_surface(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Server: New xwayland surface");
    
    struct wm_server* server = wl_container_of(listener, server, new_xwayland_surface);
    struct wlr_xwayland_surface* surface = data;

    wlr_xwayland_surface_ping(surface);

    struct wm_view_xwayland* view = calloc(1, sizeof(struct wm_view_xwayland));
    wm_view_xwayland_init(view, server, surface);
}

static void handle_new_server_decoration(struct wl_listener* listener, void* data){
    /* struct wm_server* server = wl_container_of(listener, server, new_xdg_decoration); */
    /* struct wlr_server_decoration* wlr_deco = data; */

    wlr_log(WLR_DEBUG, "Server: New server decoration");
}

static void handle_new_xdg_decoration(struct wl_listener* listener, void* data){
    /* struct wm_server* server = wl_container_of(listener, server, new_xdg_decoration); */
    /* struct wlr_xdg_toplevel_decoration_v1* wlr_deco = data; */

    wlr_log(WLR_DEBUG, "Server: New XDG toplevel decoration");
}

static void handle_ready(struct wl_listener* listener, void* data){
    wlr_log(WLR_DEBUG, "Server: Ready");

    /* Both parameters ignored */
    wm_callback_ready();
}

/*
 * Class implementation
 */
void wm_server_init(struct wm_server* server, struct wm_config* config){
    wl_list_init(&server->wm_contents);
    server->wm_config = config;

    /* Wayland and wlroots resources */
    server->wl_display = wl_display_create();
    assert(server->wl_display);

    server->wl_event_loop = wl_display_get_event_loop(server->wl_display);
    assert(server->wl_event_loop);

    server->wlr_backend = wlr_backend_autocreate(server->wl_display, NULL);
    assert(server->wlr_backend);

    server->wlr_renderer = wlr_backend_get_renderer(server->wlr_backend);
    assert(server->wlr_renderer);

    wlr_renderer_init_wl_display(server->wlr_renderer, server->wl_display);

    server->wlr_compositor = wlr_compositor_create(server->wl_display, server->wlr_renderer);
    assert(server->wlr_compositor);

    server->wlr_data_device_manager = wlr_data_device_manager_create(server->wl_display);
    assert(server->wlr_data_device_manager);

    server->wlr_xdg_shell = wlr_xdg_shell_create(server->wl_display);
    assert(server->wlr_xdg_shell);

    server->wlr_server_decoration_manager = wlr_server_decoration_manager_create(server->wl_display);
	wlr_server_decoration_manager_set_default_mode(server->wlr_server_decoration_manager, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

    server->wlr_xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(server->wl_display);
    assert(server->wlr_xdg_decoration_manager);

    server->wlr_xwayland = wlr_xwayland_create(server->wl_display, server->wlr_compositor, false);
    assert(server->wlr_xwayland);

    server->wlr_xcursor_manager = wlr_xcursor_manager_create(server->wm_config->xcursor_theme, server->wm_config->xcursor_size);
    assert(server->wlr_xcursor_manager);

    if(wlr_xcursor_manager_load(server->wlr_xcursor_manager, server->wm_config->output_scale)){
        wlr_log(WLR_ERROR, "Cannot load XCursor");
    }

    struct wlr_xcursor* xcursor = wlr_xcursor_manager_get_xcursor(server->wlr_xcursor_manager, server->wm_config->xcursor_name, 1);
    if(xcursor){
        struct wlr_xcursor_image* image = xcursor->images[0];
        wlr_xwayland_set_cursor(server->wlr_xwayland,
                image->buffer, image->width * 4, image->width, image->height, image->hotspot_x, image->hotspot_y);
    }

    /* Children */
    server->wm_layout = calloc(1, sizeof(struct wm_layout));
    wm_layout_init(server->wm_layout, server);

    server->wm_seat = calloc(1, sizeof(struct wm_seat));
    wm_seat_init(server->wm_seat, server, server->wm_layout);

    wlr_xwayland_set_seat(server->wlr_xwayland, server->wm_seat->wlr_seat);

    /* Handlers */
    server->new_input.notify = handle_new_input;
    wl_signal_add(&server->wlr_backend->events.new_input, &server->new_input);

    server->new_output.notify = handle_new_output;
    wl_signal_add(&server->wlr_backend->events.new_output, &server->new_output);

    server->new_xdg_surface.notify = handle_new_xdg_surface;
    wl_signal_add(&server->wlr_xdg_shell->events.new_surface, &server->new_xdg_surface);

	server->new_server_decoration.notify = handle_new_server_decoration;
	wl_signal_add(&server->wlr_server_decoration_manager->events.new_decoration, &server->new_server_decoration);

    server->new_xdg_decoration.notify = handle_new_xdg_decoration;
    wl_signal_add(&server->wlr_xdg_decoration_manager->events.new_toplevel_decoration, &server->new_xdg_decoration);

    server->new_xwayland_surface.notify = handle_new_xwayland_surface;
    wl_signal_add(&server->wlr_xwayland->events.new_surface, &server->new_xwayland_surface);

    /*
     * Due to the unfortunate handling of XWayland forks via SIGUSR1, we need to be sure not
     * to create any threads before the XWayland server is ready
     */
    server->xwayland_ready.notify = handle_ready;
    wl_signal_add(&server->wlr_xwayland->events.ready, &server->xwayland_ready);
}

void wm_server_destroy(struct wm_server* server){
    wlr_xwayland_destroy(server->wlr_xwayland);
    wl_display_destroy_clients(server->wl_display);
    wl_display_destroy(server->wl_display);
}

void wm_server_surface_at(struct wm_server* server, double at_x, double at_y, 
        struct wlr_surface** result, double* result_sx, double* result_sy){
    struct wm_content* content;
    wl_list_for_each(content, &server->wm_contents, link){
        if(!wm_content_is_view(content)) continue;
        struct wm_view* view = wm_cast(wm_view, content);

        if(!view->mapped) continue;
        if(!view->accepts_input) continue;

        int width;
        int height;
        wm_view_get_size(view, &width, &height);

        if(width <= 0 || height <=0) continue;

        double display_x, display_y, display_width, display_height;
        wm_content_get_box(content, &display_x, &display_y, &display_width, &display_height);

        double scale_x = display_width/width;
        double scale_y = display_height/height;

        int view_at_x = round((at_x - display_x) / scale_x);
        int view_at_y = round((at_y - display_y) / scale_y);

        double sx;
        double sy;
        struct wlr_surface* surface = wm_view_surface_at(view, view_at_x, view_at_y, &sx, &sy);

        if(surface){
            *result = surface;
            *result_sx = sx;
            *result_sy = sy;
            return;
        }
    }

    *result = NULL;
}

struct _view_for_surface_data {
    struct wlr_surface* surface;
    bool result;
};

static void _view_for_surface(struct wlr_surface* surface, int sx, int sy, void* _data){
    struct _view_for_surface_data* data = _data;
    if(surface == data->surface){
        data->result = true;
        return;
    }
}

struct wm_view* wm_server_view_for_surface(struct wm_server* server, struct wlr_surface* surface){
    struct wm_content* content;
    wl_list_for_each(content, &server->wm_contents, link){
        if(!wm_content_is_view(content)) continue;
        struct wm_view* view = wm_cast(wm_view, content);

        struct _view_for_surface_data data = { 0 };
        data.surface = surface;
        wm_view_for_each_surface(view, _view_for_surface, &data);
        if(data.result){
            return view;
        }
    }

    return NULL;
}

struct wm_widget* wm_server_create_widget(struct wm_server* server){
    struct wm_widget* widget = calloc(1, sizeof(struct wm_widget));
    wm_widget_init(widget, server);
    return widget;
}


void wm_server_update_contents(struct wm_server* server){
    /* Empty or only one element */
    if(server->wm_contents.next == server->wm_contents.prev) return;

    int swapped = 1;
    do {
        swapped = 0;

        struct wl_list* cur1 = server->wm_contents.next;
        struct wl_list* cur2 = cur1->next;
        for(;cur1 != server->wm_contents.prev; 
                cur1 = cur1->next, cur2 = cur2->next){

            struct wm_content* content1 = wl_container_of(cur1, content1, link);
            struct wm_content* content2 = wl_container_of(cur2, content2, link);

            if(content1->z_index<content2->z_index){
                cur1->prev->next = cur2;
                cur2->next->prev = cur1;

                cur1->next = cur2->next;
                cur2->prev = cur1->prev;

                cur2->next = cur1;
                cur1->prev = cur2;

                cur1 = cur1->prev;
                cur2 = cur1;
                swapped = 1;
            }
        }

    } while(swapped);
}
