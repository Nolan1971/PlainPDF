"""Small Win32 UI driver for exercising the released reader on a Windows desktop."""

import ctypes
import subprocess
import time
from ctypes import wintypes
from pathlib import Path

from PIL import Image, ImageGrab

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)

LPARAM = ctypes.c_ssize_t
WPARAM = ctypes.c_size_t
LRESULT = ctypes.c_ssize_t
WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, LPARAM)

WM_NULL = 0
WM_CLOSE = 0x0010
WM_GETTEXT = 0x000D
WM_SETTEXT = 0x000C
WM_COMMAND = 0x0111
WM_KEYDOWN = 0x0100
WM_VSCROLL = 0x0115
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
WM_MOUSEMOVE = 0x0200
WM_DROPFILES = 0x0233
BM_CLICK = 0x00F5


def api(library, name, result, *arguments):
    entry = getattr(library, name)
    entry.argtypes = arguments
    entry.restype = result
    return entry


send_message = api(user32, "SendMessageTimeoutW", LRESULT, wintypes.HWND, wintypes.UINT,
                   WPARAM, LPARAM, wintypes.UINT, wintypes.UINT, ctypes.POINTER(WPARAM))
post_message = api(user32, "PostMessageW", wintypes.BOOL, wintypes.HWND, wintypes.UINT,
                   WPARAM, LPARAM)
enum_windows = api(user32, "EnumWindows", wintypes.BOOL, WNDENUMPROC, LPARAM)
enum_children = api(user32, "EnumChildWindows", wintypes.BOOL, wintypes.HWND, WNDENUMPROC, LPARAM)
get_window_pid = api(user32, "GetWindowThreadProcessId", wintypes.DWORD,
                     wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
get_class = api(user32, "GetClassNameW", ctypes.c_int, wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
is_visible = api(user32, "IsWindowVisible", wintypes.BOOL, wintypes.HWND)
is_enabled = api(user32, "IsWindowEnabled", wintypes.BOOL, wintypes.HWND)
get_control = api(user32, "GetDlgItem", wintypes.HWND, wintypes.HWND, ctypes.c_int)
get_id = api(user32, "GetDlgCtrlID", ctypes.c_int, wintypes.HWND)
get_rect = api(user32, "GetWindowRect", wintypes.BOOL, wintypes.HWND, ctypes.POINTER(wintypes.RECT))
get_client = api(user32, "GetClientRect", wintypes.BOOL, wintypes.HWND, ctypes.POINTER(wintypes.RECT))
to_screen = api(user32, "ClientToScreen", wintypes.BOOL, wintypes.HWND, ctypes.POINTER(wintypes.POINT))
set_foreground = api(user32, "SetForegroundWindow", wintypes.BOOL, wintypes.HWND)
get_foreground = api(user32, "GetForegroundWindow", wintypes.HWND)
dpi_for_window = api(user32, "GetDpiForWindow", wintypes.UINT, wintypes.HWND)
get_gui_resources = api(user32, "GetGuiResources", wintypes.DWORD, wintypes.HANDLE, wintypes.DWORD)
open_process = api(kernel32, "OpenProcess", wintypes.HANDLE, wintypes.DWORD, wintypes.BOOL,
                   wintypes.DWORD)
close_handle = api(kernel32, "CloseHandle", wintypes.BOOL, wintypes.HANDLE)
set_thread_dpi = api(user32, "SetThreadDpiAwarenessContext", ctypes.c_void_p, ctypes.c_void_p)
set_thread_dpi(ctypes.c_void_p(-4))
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
create_dc = api(gdi32, "CreateCompatibleDC", wintypes.HDC, wintypes.HDC)
delete_dc = api(gdi32, "DeleteDC", wintypes.BOOL, wintypes.HDC)
create_dib = api(gdi32, "CreateDIBSection", wintypes.HBITMAP, wintypes.HDC, ctypes.c_void_p,
                 wintypes.UINT, ctypes.POINTER(ctypes.c_void_p), wintypes.HANDLE, wintypes.DWORD)
select_object = api(gdi32, "SelectObject", wintypes.HANDLE, wintypes.HDC, wintypes.HANDLE)
delete_object = api(gdi32, "DeleteObject", wintypes.BOOL, wintypes.HANDLE)
print_window = api(user32, "PrintWindow", wintypes.BOOL, wintypes.HWND, wintypes.HDC, wintypes.UINT)


def wait_for(function, description, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = function()
        if result:
            return result
        time.sleep(0.025)
    raise AssertionError(f"Timed out waiting for {description}")


def send(hwnd, message, wparam=0, lparam=0, timeout=2000):
    result = WPARAM()
    if not send_message(hwnd, message, wparam, lparam, 0x0002, timeout, ctypes.byref(result)):
        raise TimeoutError(f"Win32 message 0x{message:X} did not complete")
    return LRESULT(result.value).value


def post(hwnd, message, wparam=0, lparam=0):
    if not post_message(hwnd, message, wparam, lparam):
        raise ctypes.WinError(ctypes.get_last_error())


def text(hwnd):
    contents = ctypes.create_unicode_buffer(2048)
    send(hwnd, WM_GETTEXT, len(contents), ctypes.addressof(contents))
    return contents.value


def set_text(hwnd, value):
    contents = ctypes.create_unicode_buffer(value)
    send(hwnd, WM_SETTEXT, 0, ctypes.addressof(contents))


def class_name(hwnd):
    value = ctypes.create_unicode_buffer(200)
    get_class(hwnd, value, len(value))
    return value.value


def window_pid(hwnd):
    value = wintypes.DWORD()
    get_window_pid(hwnd, ctypes.byref(value))
    return value.value


def descendants(parent=None):
    found = []

    @WNDENUMPROC
    def add(hwnd, _):
        found.append(hwnd)
        return True

    if parent:
        enum_children(parent, add, 0)
    else:
        enum_windows(add, 0)
    return found


def rect(hwnd):
    value = wintypes.RECT()
    if not get_rect(hwnd, ctypes.byref(value)):
        raise ctypes.WinError(ctypes.get_last_error())
    return value.left, value.top, value.right, value.bottom


def client_size(hwnd):
    value = wintypes.RECT()
    if not get_client(hwnd, ctypes.byref(value)):
        raise ctypes.WinError(ctypes.get_last_error())
    return value.right, value.bottom


class BitmapInfo(ctypes.Structure):
    _fields_ = [
        ("size", wintypes.DWORD), ("width", wintypes.LONG), ("height", wintypes.LONG),
        ("planes", wintypes.WORD), ("bits", wintypes.WORD), ("compression", wintypes.DWORD),
        ("image_size", wintypes.DWORD), ("x_pels", wintypes.LONG), ("y_pels", wintypes.LONG),
        ("colors", wintypes.DWORD), ("important", wintypes.DWORD),
    ]


def capture(hwnd, destination=None):
    width, height = client_size(hwnd)
    info = BitmapInfo(ctypes.sizeof(BitmapInfo), width, -height, 1, 32)
    bits = ctypes.c_void_p()
    dc = create_dc(None)
    bitmap = create_dib(dc, ctypes.byref(info), 0, ctypes.byref(bits), None, 0)
    if not dc or not bitmap or not bits.value:
        if bitmap:
            delete_object(bitmap)
        if dc:
            delete_dc(dc)
        raise ctypes.WinError(ctypes.get_last_error())
    previous = select_object(dc, bitmap)
    try:
        if not print_window(hwnd, dc, 1):
            raise ctypes.WinError(ctypes.get_last_error())
        pixels = ctypes.string_at(bits, width * height * 4)
        picture = Image.frombytes("RGB", (width, height), pixels, "raw", "BGRX")
        if destination:
            picture.save(destination)
        return picture
    finally:
        select_object(dc, previous)
        delete_object(bitmap)
        delete_dc(dc)


def capture_control(reader, hwnd, destination=None):
    set_foreground(reader.main)
    wait_for(lambda: get_foreground() == reader.main, "PlainPDF foreground window")
    picture = ImageGrab.grab(bbox=rect(hwnd), all_screens=True)
    if destination:
        picture.save(destination)
    return picture


def packed(x, y):
    return (x & 0xFFFF) | ((y & 0xFFFF) << 16)


def click(hwnd, x, y):
    send(hwnd, WM_LBUTTONDOWN, 1, packed(x, y))
    send(hwnd, WM_LBUTTONUP, 0, packed(x, y))


class Reader:
    def __init__(self, exe, argument=None):
        self.executable = Path(exe).resolve()
        self.argument = argument
        self.process = None
        self.main = None
        self.canvas = None
        self.start_seconds = None

    def __enter__(self):
        arguments = [str(self.executable)]
        if self.argument is not None:
            arguments.append(str(self.argument))
        start = time.perf_counter()
        self.process = subprocess.Popen(arguments, cwd=self.executable.parent)
        try:
            self.main = wait_for(self.find_main, "PlainPDF main window")
            send(self.main, WM_NULL)
            self.start_seconds = time.perf_counter() - start
            self.canvas = wait_for(lambda: self.child("PlainPDFDocument"), "document canvas")
            set_foreground(self.main)
            return self
        except BaseException:
            self.process.terminate()
            self.process.wait(timeout=10)
            raise

    def __exit__(self, *_):
        if self.process.poll() is None:
            for dialog in self.dialogs():
                post(dialog, WM_CLOSE)
            post(self.main, WM_CLOSE)
            try:
                self.process.wait(timeout=12)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                self.process.wait(timeout=10)
                raise
        if self.process.returncode != 0:
            raise AssertionError(f"PlainPDF exit code {self.process.returncode}")

    def find_main(self):
        for hwnd in descendants():
            if (window_pid(hwnd) == self.process.pid and is_visible(hwnd)
                    and class_name(hwnd) == "PlainPDFMainWindow"):
                return hwnd
        return None

    def child(self, name):
        return next((hwnd for hwnd in descendants(self.main) if class_name(hwnd) == name), None)

    def dialogs(self):
        return [hwnd for hwnd in descendants() if window_pid(hwnd) == self.process.pid
                and is_visible(hwnd) and class_name(hwnd) == "#32770"]

    def control(self, control_id):
        return get_control(self.main, control_id)

    def status(self):
        labels = [node for node in descendants(self.main)
                  if class_name(node) == "Static" and is_visible(node)]
        if labels:
            return text(max(labels, key=lambda node: rect(node)[1]))
        return ""

    def command(self, command_id, asynchronous=False):
        deliver = post if asynchronous else send
        return deliver(self.main, WM_COMMAND, command_id)

    def title(self):
        return text(self.main)

    def wait_title(self, filename):
        return wait_for(lambda: self.title().startswith(str(filename) + " — PlainPDF"),
                        f"document title for {filename}")

    def jump(self, page):
        control = self.control(1018)
        set_text(control, str(page))
        post(control, WM_KEYDOWN, 13)
        send(self.main, WM_NULL)

    def page(self):
        return text(self.control(1018))

    def query(self, value):
        self.command(1006)
        control = self.control(1019)
        set_text(control, value)
        post(control, WM_KEYDOWN, 13)

    def respond(self, dialog, control_id):
        button = get_control(dialog, control_id)
        set_foreground(dialog)
        if text(dialog) in ("Open", "Save Print Output") and button and is_enabled(button):
            post(button, BM_CLICK)
        else:
            post(dialog, WM_COMMAND, control_id, button or 0)
