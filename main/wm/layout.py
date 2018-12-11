import math
import os
from itertools import product

from pywm import (
    PyWM,
    PYWM_MOD_CTRL,
    PYWM_PRESSED,
    PYWM_MOD_ALT,
    PYWM_MOD_LOGO
)


from .background import Background
from .bar import TopBar, BottomBar
from .view import View, ViewState
from .state import State
from .animate import Animate, Transition
from .pinch_overlay import PinchOverlay
from .swipe_overlay import SwipeOverlay
from .overview_overlay import OverviewOverlay


class LayoutState(State):
    def __init__(self, i, j, size, min_i, min_j, max_i, max_j, padding,
                 background_factor, top_bar_dy, bottom_bar_dy):
        super().__init__(['i', 'j', 'size',
                          'min_i', 'min_j', 'max_i', 'max_j',
                          'padding', 'background_factor',
                          'top_bar_dy', 'bottom_bar_dy'])

        self.i = i
        self.j = j
        self.size = size
        self.min_i = min_i
        self.min_j = min_j
        self.max_i = max_i
        self.max_j = max_j
        self.padding = padding
        self.background_factor = background_factor
        self.top_bar_dy = top_bar_dy
        self.bottom_bar_dy = bottom_bar_dy

    def lies_within_extent(self, i, j):
        if i < self.min_i:
            return False
        if j < self.min_j:
            return False
        if i + self.size - 1 > self.max_i:
            return False
        if j + self.size - 1 > self.max_j:
            return False

        return True


def _box_intersects(box1, box2):
    box1_tiles = []
    for i, j in product(range(math.floor(box1[0]),
                              math.ceil(box1[0] + box1[2])),
                        range(math.floor(box1[1]),
                              math.ceil(box1[1] + box1[3]))):
        box1_tiles += [(i, j)]

    box2_tiles = []
    for i, j in product(range(math.floor(box2[0]),
                              math.ceil(box2[0] + box2[2])),
                        range(math.floor(box2[1]),
                              math.ceil(box2[1] + box2[3]))):
        box2_tiles += [(i, j)]

    for t in box1_tiles:
        if t in box2_tiles:
            return True
    return False


class ResizeViewTransition(Transition):
    def __init__(self, layout, view, duration, delta_i, delta_j):
        super().__init__(view, duration)
        self.layout = layout
        self.view = view
        self.delta_i = delta_i
        self.delta_j = delta_j

    def setup(self):
        new_view_box = [self.view.state.i,
                        self.view.state.j,
                        self.view.state.w,
                        self.view.state.h]

        delta_i = self.delta_i
        delta_j = self.delta_j
        if new_view_box[2] == 1 and delta_i < 0:
            new_view_box[0] += delta_i
            new_view_box[2] -= delta_i
            delta_i = 0

        if new_view_box[3] == 1 and delta_j < 0:
            new_view_box[1] += delta_j
            new_view_box[3] -= delta_j
            delta_j = 0

        new_view_box[2] += delta_i
        new_view_box[3] += delta_j

        for v in self.layout.windows():
            if v == self.view:
                continue
            if _box_intersects(new_view_box, [v.state.i, v.state.j,
                                              v.state.w, v.state.h]):
                return

        new_state = self.view.state.copy()
        new_state.i = new_view_box[0]
        new_state.j = new_view_box[1]
        new_state.w = new_view_box[2]
        new_state.h = new_view_box[3]
        super().setup(new_state)

    def finish(self):
        super().finish()
        self.layout.rescale()
        self.layout.reset_extent(focus_view=self.view)


class MoveViewTransition(Transition):
    def __init__(self, layout, view, duration, delta_i, delta_j):
        super().__init__(view, duration)
        self.layout = layout
        self.view = view
        self.delta_i = delta_i
        self.delta_j = delta_j

    def setup(self):
        new_view_box = [self.view.state.i + self.delta_i,
                        self.view.state.j + self.delta_j,
                        self.view.state.w,
                        self.view.state.h]

        for v in self.layout.windows():
            if v == self.view:
                continue
            if _box_intersects(new_view_box, [v.state.i, v.state.j,
                                              v.state.w, v.state.h]):
                return

        new_state = self.view.state.copy()
        new_state.i = new_view_box[0]
        new_state.j = new_view_box[1]
        new_state.w = new_view_box[2]
        new_state.h = new_view_box[3]
        super().setup(new_state)

    def finish(self):
        super().finish()
        self.layout.reset_extent(focus_view=self.view)


class Layout(PyWM, Animate):
    def __init__(self, mod, **kwargs):
        PyWM.__init__(self, View, **kwargs)
        Animate.__init__(self)

        self.mod = mod
        self.mod_sym = None
        if mod == PYWM_MOD_ALT:
            self.mod_sym = "Alt"
        elif mod == PYWM_MOD_LOGO:
            self.mod_sym = "Super"
        else:
            raise Exception("Unknown mod")

        self.default_padding = 0.01
        self.state = LayoutState(0, 0, 2, 0, 0, 1, 1,
                                 self.default_padding, 3, 0, 0)

        self.overlay = None

        self.background = None
        self.top_bar = None
        self.bottom_bar = None

        """
        scale == size: pixel-to-pixel
        scale == 2 * size: client-side width height are twice as
            high as rendered width, height => Appears half as big
        ...
        """
        self.is_half_scale = False
        self.scale = 2

        self.fullscreen_backup = 0, 0, 1

    def windows(self):
        return [v for v in self.views if not v.floating]

    def dialogs(self):
        return [v for v in self.views if v.floating]

    def update(self):
        for v in self.views:
            v.update()

        if self.background is not None:
            self.background.update()

        if self.top_bar is not None:
            self.top_bar.update()

        if self.bottom_bar is not None:
            self.bottom_bar.update()

    def find_at_tile(self, i, j):
        for view in self.windows():
            if (view.state.i <= i < view.state.i + view.state.w) and \
                    (view.state.j <= j < view.state.j + view.state.h):
                return view

        return None

    def find_focused_box(self):
        for view in self.views:
            if view.focused:
                return view.state.i, view.state.j, view.state.w, view.state.h

        return 0, 0, 1, 1

    def get_extent(self):
        if len(self.views) == 0:
            return self.state.i, self.state.j, \
                self.state.i + self.state.size - 1, \
                self.state.j + self.state.size - 1

        min_i = min([view.state.i for view in self.views])
        min_j = min([view.state.j for view in self.views])
        max_i = max([view.state.i + view.state.w - 1 for view in self.views])
        max_j = max([view.state.j + view.state.h - 1 for view in self.views])

        """
        Borders around, such that views can be at the edges
        """
        min_i -= max(self.state.size - 1, 1)
        min_j -= max(self.state.size - 1, 1)
        max_i += max(self.state.size - 1, 1)
        max_j += max(self.state.size - 1, 1)

        return min_i, min_j, max_i, max_j

    def place_initial(self, view, w, h):
        place_i = 0
        place_j = 0
        for j, i in product(range(math.floor(self.state.j),
                                  math.ceil(self.state.j + self.state.size)),
                            range(math.floor(self.state.i),
                                  math.ceil(self.state.i + self.state.size))):
            for jp, ip in product(range(j, j + h), range(i, i + w)):
                if self.find_at_tile(ip, jp) is not None:
                    break
            else:
                place_i, place_j = i, j
                break
        else:
            place_i, place_j = self.state.i, self.state.j
            while self.find_at_tile(place_i, place_j) is not None:
                place_i += 1

        view.state = ViewState(place_i, place_j, w, h)
        view.update_size()

        new_state = self.state.copy()
        new_state.min_i, new_state.min_j, new_state.max_i, new_state.max_j = \
            self.get_extent()

        self.focus_view(view, new_state)

    def reset_extent(self, focus_view=None):
        new_state = self.state.copy()
        new_state.min_i, new_state.min_j, new_state.max_i, new_state.max_j = \
            self.get_extent()

        if focus_view is None:
            self.animation(Transition(self, .2,
                                      **new_state.kwargs()), pend=True)
        else:
            self.focus_view(focus_view, new_state)

    def on_key(self, time_msec, keycode, state, keysyms):
        if self.overlay is not None and self.overlay.ready():
            return self.overlay.on_key(time_msec, keycode, state, keysyms)

        """
        All events with  our modifier are consumed.
        No events without our modifier are consumed.
        """
        if not self.modifiers & self.mod:
            if self.overlay is not None:
                if not self.overlay.keep_alive():
                    self.exit_overlay()
                    return True

            """
            Also do not dispatch release of mod
            """
            if self.mod_sym in keysyms:
                return True

            return False

        if state != PYWM_PRESSED:
            if keysyms == "h":
                if self.modifiers & PYWM_MOD_CTRL:
                    self.resize_view(-1, 0)
                else:
                    self.move(-1, 0)
            elif keysyms == "H":
                self.move_view(-1, 0)
            elif keysyms == "l":
                if self.modifiers & PYWM_MOD_CTRL:
                    self.resize_view(1, 0)
                else:
                    self.move(1, 0)
            elif keysyms == "L":
                self.move_view(1, 0)
            elif keysyms == "k":
                if self.modifiers & PYWM_MOD_CTRL:
                    self.resize_view(0, -1)
                else:
                    self.move(0, -1)
            elif keysyms == "K":
                self.move_view(0, -1)
            elif keysyms == "j":
                if self.modifiers & PYWM_MOD_CTRL:
                    self.resize_view(0, 1)
                else:
                    self.move(0, 1)
            elif keysyms == "J":
                self.move_view(0, 1)
            elif keysyms == "Return":
                os.system("termite &")
            elif keysyms == "C":
                self.terminate()
            elif keysyms == "a":
                self.enter_overlay(OverviewOverlay(self))
            elif keysyms == "s":
                self.toggle_half_scale()
            elif keysyms == "f":
                self.toggle_padding()

        return True

    def on_modifiers(self, modifiers):
        if not self.modifiers & self.mod:
            if self.overlay is not None:
                if not self.overlay.keep_alive():
                    self.exit_overlay()
                    return True
        return False

    def move(self, delta_i, delta_j):
        i, j, w, h = self.find_focused_box()
        ci, cj = i + w/2., j + h/2.

        def score(view):
            cvi, cvj = view.state.i + view.state.w/2., \
                view.state.j + view.state.h/2.
            sp = (cvi - ci) * delta_i + (cvj - cj) * delta_j
            sp *= ((cvi - ci) ** 2) + ((cvj - cj) ** 2)
            return sp

        best_view = None
        best_view_score = 1000

        for view in self.views:
            s = score(view)
            if s > 0. and s < best_view_score:
                best_view_score = s
                best_view = view

        if best_view is not None:
            self.focus_view(best_view)

    def move_view(self, delta_i, delta_j):
        view = [v for v in self.views if v.focused]
        if len(view) == 0:
            return

        view = view[0]
        while view.floating and view.parent is not None:
            view = view.parent

        if view.floating:
            return

        view.animation(MoveViewTransition(self, view, .2, delta_i, delta_j),
                       pend=True)

        for v in self.dialogs():
            if v.parent == view:
                v.animation(MoveViewTransition(self, v, .2, delta_i, delta_j),
                            pend=True)

    def resize_view(self, delta_i, delta_j):
        view = [v for v in self.views if v.focused]
        if len(view) == 0:
            return
        view = view[0]
        while view.floating and view.parent is not None:
            view = view.parent

        if view.floating:
            return

        view.animation(ResizeViewTransition(self, view, .2, delta_i, delta_j),
                       pend=True)

    def focus_view(self, view, new_state=None):
        view.focus()
        i, j, w, h = view.state.i, view.state.j, \
            view.state.w, view.state.h

        target_i, target_j, target_size = self.state.i, \
            self.state.j, self.state.size

        target_size = max(target_size, w, h)
        target_i = min(target_i, i)
        target_j = min(target_j, j)
        target_i = max(target_i, i + w - target_size)
        target_j = max(target_j, j + h - target_size)

        if new_state is None:
            new_state = self.state.copy()
        new_state.i = target_i
        new_state.j = target_j
        new_state.size = target_size

        self.animation(Transition(self, .2,
                                  finished_func=self.rescale,
                                  **new_state.kwargs()), pend=True)

    def toggle_half_scale(self):
        self.is_half_scale = not self.is_half_scale
        self.rescale()

    def rescale(self):
        self.scale = self.state.size * (.5 if self.is_half_scale else 1.)
        for v in self.views:
            v.update_size()

    def toggle_padding(self):
        padding = self.default_padding \
            if self.state.padding == 0 else 0

        if padding == 0:
            focused = self.find_focused_box()
            self.fullscreen_backup = self.state.i, self.state.j, \
                self.state.size
            self.animation(Transition(self, .2,
                                      finished_func=self.rescale,
                                      padding=padding,
                                      i=focused[0],
                                      j=focused[1],
                                      size=max(focused[2:])))
        else:
            reset = self.fullscreen_backup
            min_i, min_j, max_i, max_j = self.get_extent()
            print(min_i, min_j, max_i, max_j, reset)
            if reset[0] >= min_i and reset[1] >= min_j \
                    and reset[0] + reset[2] - 1 <= max_i \
                    and reset[1] + reset[2] - 1 <= max_j:
                self.animation(Transition(self, .2,
                                          finished_func=self.rescale,
                                          padding=padding,
                                          i=reset[0],
                                          j=reset[1],
                                          size=reset[2]))
            else:
                self.animation(Transition(self, .2, padding=padding))

    def enter_overlay(self, overlay):
        if self.overlay is not None:
            return

        self.overlay = overlay
        self.overlay.init()

    def exit_overlay(self):
        if self.overlay is None:
            return

        self.overlay.destroy()

    def on_overlay_destroyed(self):
        self.overlay = None

    def on_motion(self, time_msec, delta_x, delta_y):
        if self.overlay is not None and self.overlay.ready():
            return self.overlay.on_motion(time_msec, delta_x, delta_y)

        if self.modifiers & self.mod:
            ovr = PinchOverlay(self)
            ovr.on_motion(time_msec, delta_x, delta_y)
            self.enter_overlay(ovr)
            return True

        return False

    def on_axis(self, time_msec, source, orientation, delta, delta_discrete):
        if self.overlay is not None and self.overlay.ready():
            return self.overlay.on_axis(time_msec, source, orientation,
                                        delta, delta_discrete)

        return False

    def on_multitouch_begin(self, touches):
        if self.overlay is not None and self.overlay.ready():
            return self.overlay.on_multitouch_begin(touches)

        if self.modifiers & self.mod:
            ovr = PinchOverlay(self)
            ovr.on_multitouch_begin(touches)
            self.enter_overlay(ovr)
            return True

        if touches.number == 3:
            ovr = SwipeOverlay(self)
            ovr.on_multitouch_begin(touches)
            self.enter_overlay(ovr)
            return True

        return False

    def on_multitouch_end(self):
        if self.overlay is not None and self.overlay.ready():
            self.overlay.on_multitouch_end()

        if isinstance(self.overlay, PinchOverlay):
            if not self.modifiers & self.mod:
                self.exit_overlay()

    def on_multitouch_update(self, touches):
        if self.overlay is not None and self.overlay.ready():
            self.overlay.on_multitouch_update(touches)

    def main(self):
        self.background = self.create_widget(Background,
                                             '/home/jonas/wallpaper.jpg')
        self.top_bar = self.create_widget(TopBar)
        self.bottom_bar = self.create_widget(BottomBar)

    def terminate(self):
        super().terminate()
        self.top_bar.stop()
        self.bottom_bar.stop()
