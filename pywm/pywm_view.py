from ._pywm import (
    view_get_box,
    view_get_dimensions,
    view_get_info,
    view_set_box,
    view_set_dimensions,
    view_focus
)


class PyWMView:
    def __init__(self, wm, handle):
        self._handle = handle

        """
        Consider these readonly
        """
        self.wm = wm
        self.box = view_get_box(self._handle)
        self.focused = False

    def focus(self):
        view_focus(self._handle)

    def set_box(self, x, y, w, h):
        view_set_box(self._handle, x, y, w, h)
        self.box = (x, y, w, h)

    def get_info(self):
        return view_get_info(self._handle)

    def get_dimensions(self):
        return view_get_dimensions(self._handle)

    def set_dimensions(self, width, height):
        view_set_dimensions(self._handle, round(width), round(height))

    """
    Virtual methods
    """

    def main(self):
        pass

    def destroy(self):
        pass

    def on_focus_change(self):
        pass
