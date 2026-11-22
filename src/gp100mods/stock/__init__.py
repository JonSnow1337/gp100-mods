"""Patches for the original Valeton GP-100 V2.1 firmware. Each module's `apply()` returns new firmware bytes and raises
ValueError - never anything else - when the input is not exactly the V2.1 layout it was written for."""
import functools
import struct


def guarded(fn):
    """Turn "read past the end / unexpected layout" crashes on a foreign file into a clean ValueError."""
    @functools.wraps(fn)
    def wrapper(*args, **kwargs):
        try:
            return fn(*args, **kwargs)
        except (struct.error, IndexError):
            raise ValueError("this is not the V2.1 firmware (unexpected layout)") from None
    return wrapper
