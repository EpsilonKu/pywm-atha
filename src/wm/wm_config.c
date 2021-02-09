#include <stdlib.h>
#include "wm/wm_config.h"

void wm_config_init_default(struct wm_config* config){
    config->output_scale = 1.;
    config->xcursor_theme = NULL;
    config->xcursor_name = "left_ptr";
    config->xcursor_size = 12;

    config->focus_follows_mouse = 1;
    config->constrain_popups_to_toplevel = 0;
}
