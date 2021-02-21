from .touchpad import Touchpad, find_touchpad
from .sanitize_bogus_ids import SanitizeBogusIds
from .gestures import (  # noqa F401
    Gestures,
    SingleFingerMoveGesture,
    TwoFingerSwipePinchGesture,
    HigherSwipeGesture,
    GestureListener,
    LowpassGesture
)


def create_touchpad(device_name, gesture_listener):
    event = find_touchpad(device_name)
    if event is not None:
        touchpad = Touchpad(event)
        gestures = Gestures(
            # SanitizeBogusIds(
                touchpad
            # )
        )
        gestures.listener(gesture_listener)
        return touchpad, gestures
    else:
        return None
