"""Compare the icons Windows extracts and displays with the project icon."""

import ctypes
from ctypes import wintypes
from pathlib import Path

from windows_ui import BitmapInfo, api, create_dc, create_dib, delete_dc, delete_object, select_object, user32


SOURCE_ICON = Path(__file__).resolve().parents[1] / "src" / "PlainPDF.ico"
SIZES = (16, 32, 256)
extract = api(user32, "PrivateExtractIconsW", wintypes.UINT, wintypes.LPCWSTR, ctypes.c_int,
              ctypes.c_int, ctypes.c_int, ctypes.POINTER(wintypes.HICON),
              ctypes.POINTER(wintypes.UINT), wintypes.UINT, wintypes.UINT)
destroy_icon = api(user32, "DestroyIcon", wintypes.BOOL, wintypes.HICON)
draw_icon = api(user32, "DrawIconEx", wintypes.BOOL, wintypes.HDC, ctypes.c_int, ctypes.c_int,
               wintypes.HICON, ctypes.c_int, ctypes.c_int, wintypes.UINT, wintypes.HBRUSH, wintypes.UINT)
class_api = "GetClassLongPtrW" if ctypes.sizeof(ctypes.c_void_p) == 8 else "GetClassLongW"
class_value = api(user32, class_api, ctypes.c_size_t, wintypes.HWND, ctypes.c_int)
metric = api(user32, "GetSystemMetrics", ctypes.c_int, ctypes.c_int)


def rendered_icon(icon, size):
    info = BitmapInfo(ctypes.sizeof(BitmapInfo), size, -size, 1, 32)
    bits = ctypes.c_void_p()
    dc = create_dc(None)
    assert dc, "Could not create an icon test DC"
    try:
        bitmap = create_dib(dc, ctypes.byref(info), 0, ctypes.byref(bits), None, 0)
        assert bitmap, "Could not allocate an icon test bitmap"
        previous = select_object(dc, bitmap)
        try:
            count = size * size * 4
            ctypes.memset(bits, 255, count)
            assert draw_icon(dc, 0, 0, icon, size, size, 0, None, 3)
            return ctypes.string_at(bits, count)
        finally:
            select_object(dc, previous)
            delete_object(bitmap)
    finally:
        delete_dc(dc)


def file_icon(path, size):
    icon = wintypes.HICON()
    assert extract(str(path), 0, size, size, ctypes.byref(icon), None, 1, 0) == 1, path
    try:
        return rendered_icon(icon, size)
    finally:
        destroy_icon(icon)


def check_file_icon(path):
    for size in SIZES:
        assert file_icon(path, size) == file_icon(SOURCE_ICON, size), f"Wrong {size}px icon in {path}"


def check_window_icon(hwnd):
    for field, metric_index in ((-14, 11), (-34, 49)):  # GCLP_HICON/SM and SM_CXICON/SMICON.
        icon = class_value(hwnd, field)
        size = metric(metric_index)
        assert icon and rendered_icon(icon, size) == file_icon(SOURCE_ICON, size), (
            f"Wrong {size}px icon for the window class")
