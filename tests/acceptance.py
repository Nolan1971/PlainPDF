"""Run functional checks using PlainPDF's ordinary Win32 controls and messages."""

import argparse
import ctypes
import json
import subprocess
import time
import traceback
import winreg
from contextlib import contextmanager
from ctypes import wintypes
from pathlib import Path

from PIL import Image, ImageChops

from windows_ui import (
    WM_KEYDOWN, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE, WM_VSCROLL,
    Reader, api, capture, capture_control, class_name,
    click, descendants, get_control, get_id, is_enabled, is_visible, kernel32,
    packed, post, rect, send, set_foreground, set_text, text, user32, wait_for,
)

RED = (184, 41, 39)
GREEN = (37, 112, 74)
BLUE = (34, 79, 157)
FORM = (243, 235, 187)

global_lock = api(kernel32, "GlobalLock", ctypes.c_void_p, wintypes.HGLOBAL)
global_unlock = api(kernel32, "GlobalUnlock", wintypes.BOOL, wintypes.HGLOBAL)
open_clipboard = api(user32, "OpenClipboard", wintypes.BOOL, wintypes.HWND)
close_clipboard = api(user32, "CloseClipboard", wintypes.BOOL)
clipboard_data = api(user32, "GetClipboardData", wintypes.HANDLE, wintypes.UINT)
ole32 = ctypes.WinDLL("ole32")
ole_init = api(ole32, "OleInitialize", wintypes.LONG, ctypes.c_void_p)
ole_uninit = api(ole32, "OleUninitialize", None)
ole_get_clipboard = api(ole32, "OleGetClipboard", wintypes.LONG, ctypes.POINTER(ctypes.c_void_p))
ole_set_clipboard = api(ole32, "OleSetClipboard", wintypes.LONG, ctypes.c_void_p)


@contextmanager
def preserve_recent():
    key = r"Software\PlainPDF"
    name = "RecentFiles"
    initial = None
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as registry:
            initial = winreg.QueryValueEx(registry, name)
    except FileNotFoundError:
        pass
    try:
        yield
    finally:
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, key) as registry:
            if initial:
                value, kind = initial
                winreg.SetValueEx(registry, name, 0, kind, value)
            else:
                try:
                    winreg.DeleteValue(registry, name)
                except FileNotFoundError:
                    pass


@contextmanager
def preserve_clipboard():
    original = ctypes.c_void_p()
    if ole_init(None) < 0:
        raise OSError("OLE clipboard initialization failed")
    ole_get_clipboard(ctypes.byref(original))
    try:
        yield
    finally:
        ole_set_clipboard(original)
        if original.value:
            table = ctypes.cast(original, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
            release = ctypes.WINFUNCTYPE(wintypes.ULONG, ctypes.c_void_p)(table[2])
            release(original)
        ole_uninit()


def clipboard_text():
    if not open_clipboard(None):
        return ""
    try:
        handle = clipboard_data(13)
        if not handle:
            return ""
        address = global_lock(handle)
        if not address:
            return ""
        try:
            return ctypes.wstring_at(address)
        finally:
            global_unlock(handle)
    finally:
        close_clipboard()


def marker(image, color):
    model = Image.new("RGB", image.size, color)
    delta = ImageChops.difference(model, image)
    channels = delta.split()
    total = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
    box = total.point(lambda level: 255 if level == 0 else 0).getbbox()
    if box and box[2] - box[0] >= 4 and box[3] - box[1] >= 4:
        return box
    return None


def wait_marker(hwnd, color, timeout=8):
    def check():
        image = capture(hwnd)
        return marker(image, color)

    return wait_for(check, f"painted fixture marker {color}", timeout=timeout)


def point_at(reader, color, pdf_x, from_top):
    left, top, right, bottom = wait_marker(reader.canvas, color)
    scale = ((right - left) + (bottom - top)) / 48
    return round(left + (pdf_x - 30) * scale), round(top + (from_top - 23) * scale)


def open_file_dialog(reader, path):
    reader.command(1001, asynchronous=True)
    dialog = wait_for(lambda: next((w for w in reader.dialogs() if text(w) == "Open"), None),
                      "Windows Open dialog")
    controls = descendants(dialog)
    filename = next((w for w in controls if get_id(w) == 1148 and is_visible(w)), None)
    edits = descendants(filename) if filename else controls
    field = next((w for w in edits if class_name(w) == "Edit"
                  and is_visible(w) and is_enabled(w)), None)
    if not field:
        raise AssertionError(f"File name edit missing, classes: "
                             f"{[(class_name(w), get_id(w)) for w in controls]}")
    set_text(field, str(path))
    set_foreground(dialog)
    reader.respond(dialog, 1)


def drop_file(reader, path):
    driver = Path(__file__).resolve().parents[1] / "out/test/tools/DropPdf.exe"
    if not driver.is_file():
        raise AssertionError("Run tests/Build-DropTest.ps1 before the file drop case")
    set_foreground(reader.main)
    left, top, right, bottom = rect(reader.canvas)
    subprocess.run([str(driver), str(path), str((left + right) // 2), str((top + bottom) // 2)],
                   check=True, timeout=6, creationflags=subprocess.CREATE_NO_WINDOW)


def dialog_text(dialog):
    return "\n".join(text(control) for control in descendants(dialog)
                     if class_name(control) == "Static" and is_visible(control))


class Tests:
    def __init__(self, exe, fixtures, evidence):
        self.exe = exe
        self.fixtures = fixtures
        self.evidence = evidence
        self.cases = []

    def run(self, ids, case):
        start = time.perf_counter()
        try:
            evidence = case()
            self.cases.append({"ids": ids, "result": "Pass", "evidence": evidence})
            print(f"PASS {ids}: {evidence}", flush=True)
        except Exception as error:
            self.cases.append({"ids": ids, "result": "Fail", "evidence": str(error),
                               "traceback": traceback.format_exc()})
            print(f"FAIL {ids}: {error}", flush=True)
        self.cases[-1]["seconds"] = round(time.perf_counter() - start, 3)

    def blank_and_open(self):
        with Reader(self.exe) as reader:
            assert reader.title() == "PlainPDF" and not is_enabled(reader.control(1018))
            image = capture(reader.canvas)
            assert len(image.getcolors(image.width * image.height)) == 1
            path = self.fixtures / "café – 日本語.pdf"
            open_file_dialog(reader, path)
            reader.wait_title(path.name)
            assert reader.page() == "1"
            wait_marker(reader.canvas, RED)
            reader.command(1002)
            assert reader.title() == "PlainPDF"
            image = capture(reader.canvas)
            assert len(image.getcolors(image.width * image.height)) == 1
        return "Blank launch and close; native Open dialog rendered a Unicode filename and page 1."

    def entry_points(self):
        with Reader(self.exe, self.fixtures / "single.pdf") as reader:
            reader.wait_title("single.pdf")
            drop_file(reader, self.fixtures / "scan.pdf")
            reader.wait_title("scan.pdf")
            wait_marker(reader.canvas, (118, 154, 163))
            reader.command(1002)
            reader.command(1101)
            reader.wait_title("single.pdf")
        return "Command line, Win32 file drop, and Recent Files selected the expected documents."

    def navigate(self):
        with Reader(self.exe, self.fixtures / "mixed.pdf") as reader:
            reader.wait_title("mixed.pdf")
            wait_marker(reader.canvas, RED)
            reader.jump(99)
            wait_for(lambda: "within this document" in reader.status(), "invalid page feedback")
            assert reader.page() == "1"
            reader.jump(2)
            wait_for(lambda: reader.page() == "2", "page field navigation")
            wait_marker(reader.canvas, GREEN)
            reader.command(1010)
            wait_for(lambda: send(reader.control(1021), 0x147) == 1, "Fit Page selection")
            reader.command(1011)
            assert send(reader.control(1021), 0x147) == 5
            assert reader.page() == "2"
            reader.command(1012)
            assert send(reader.control(1021), 0x147) == 0
            reader.command(1013)
            assert send(reader.control(1021), 0x147) > 1
            reader.command(1014)
            assert send(reader.control(1021), 0x147) > 1
            post(reader.canvas, WM_KEYDOWN, 0x24)
            wait_for(lambda: reader.page() == "1", "Home key")
            post(reader.canvas, WM_KEYDOWN, 0x23)
            wait_for(lambda: reader.page() == "3", "End key")
            reader.command(1010)
            reader.jump(3)
            wait_marker(reader.canvas, BLUE)
            wait_marker(reader.canvas, FORM)
            capture(reader.canvas, self.evidence / "form-appearance.png")
        return "Mixed page markers, native page field, invalid input, zoom commands, Home/End, and visible form appearance."

    def find(self):
        with Reader(self.exe, self.fixtures / "mixed.pdf") as reader:
            reader.wait_title("mixed.pdf")
            reader.query("NEEDLE")
            for page in (1, 2, 3, 1):
                wait_for(lambda: reader.status() == f"Match on page {page}.", f"find page {page}")
                reader.command(1007)
            wait_for(lambda: reader.status() == "Match on page 2.", "forward match after wrap")
            reader.command(1008)
            wait_for(lambda: reader.status() == "Match on page 1.", "previous result")
            capture(reader.canvas, self.evidence / "find-match.png")
            post(reader.control(1019), WM_KEYDOWN, 0x1B)
            wait_for(lambda: not is_visible(reader.control(1019)), "Escape closes Find")
        return "Case-insensitive next/previous across pages, forward wrap and Escape."

    def search_scan(self):
        with Reader(self.exe, self.fixtures / "scan.pdf") as reader:
            reader.wait_title("scan.pdf")
            wait_marker(reader.canvas, (118, 154, 163))
            reader.query("IMAGE")
            wait_for(lambda: reader.status() == "No matches.", "no scan text")
        return "Image-only fixture rendered; searching text baked into the image returned no matches."

    def select(self):
        with preserve_clipboard(), Reader(self.exe, self.fixtures / "mixed.pdf") as reader:
            reader.wait_title("mixed.pdf")
            reader.command(1010)
            x, y = point_at(reader, RED, 65, 80)
            send(reader.canvas, WM_LBUTTONDOWN, 1, packed(x, y))
            time.sleep(0.08)
            send(reader.canvas, WM_MOUSEMOVE, 1, packed(x + 30, y))
            send(reader.canvas, WM_VSCROLL, 3)
            wait_for(lambda: reader.page() == "2", "scroll to page two while selecting")
            x, y = point_at(reader, GREEN, 229, 80)
            send(reader.canvas, WM_MOUSEMOVE, 1, packed(x, y))
            time.sleep(0.12)
            send(reader.canvas, WM_LBUTTONUP, 0, packed(x, y))
            time.sleep(0.15)
            reader.command(1005)

            def copied():
                value = clipboard_text()
                return value if "needle page one" in value and "needle page two" in value else None

            value = wait_for(copied, "cross-page Unicode clipboard text")
            assert "café" in value and "€" in value
            assert value.index("needle page one") < value.index("needle page two")
            capture(reader.canvas, self.evidence / "selection.png")
        return "Drag selection crossed pages; CF_UNICODETEXT contained both pages, café, and euro sign in order."

    def password(self):
        with Reader(self.exe, self.fixtures / "locked.pdf") as reader:
            first = wait_for(lambda: next(iter(reader.dialogs()), None), "initial password dialog")
            set_text(get_control(first, 1200), "invalid")
            reader.respond(first, 1)

            def retry_dialog():
                return next((d for d in reader.dialogs() if "Incorrect password" in dialog_text(d)), None)

            retry = wait_for(retry_dialog, "wrong-password retry prompt")
            set_text(get_control(retry, 1200), "plainpass")
            reader.respond(retry, 1)
            reader.wait_title("locked.pdf")
            wait_marker(reader.canvas, GREEN)
            reader.command(1100)
            cancel = wait_for(lambda: next(iter(reader.dialogs()), None), "password needed again")
            reader.respond(cancel, 2)
            wait_for(lambda: not reader.dialogs(), "password prompt dismissed")
            assert reader.title() == "locked.pdf — PlainPDF"
        return "Wrong password was rejected, correct password rendered the PDF, reopening prompted again, cancel preserved the live document."

    def invalid(self):
        with Reader(self.exe, self.fixtures / "single.pdf") as reader:
            reader.wait_title("single.pdf")
            for name in ("empty.pdf", "invalid.pdf", "truncated.pdf"):
                open_file_dialog(reader, self.fixtures / name)
                dialog = wait_for(lambda: next((d for d in reader.dialogs()
                                                if "damaged" in dialog_text(d)
                                                or "readable" in dialog_text(d)), None),
                                  f"error for {name}")
                reader.respond(dialog, 1)
                wait_for(lambda: not reader.dialogs(), "error dismissal")
                assert reader.title() == "single.pdf — PlainPDF"
                wait_marker(reader.canvas, RED)
        return "Empty, non-PDF, and truncated files showed controlled errors and preserved the first PDF."

    def sidebar(self):
        with Reader(self.exe, self.fixtures / "mixed.pdf") as reader:
            reader.wait_title("mixed.pdf")
            original = rect(reader.canvas)
            reader.command(1009)
            tree = reader.control(1025)
            wait_for(lambda: send(tree, 0x1105) == 3, "bookmark hierarchy")
            root = send(tree, 0x110A, 0)
            next_root = send(tree, 0x110A, 1, root)
            child = send(tree, 0x110A, 4, next_root)
            assert root and next_root and child
            assert send(tree, 0x1102, 2, next_root)
            assert send(tree, 0x110B, 9, child)
            wait_for(lambda: reader.page() == "3", "bookmark jump")
            assert rect(reader.canvas)[0] > original[0]
            reader.command(1024)
            listing = reader.control(1026)
            assert is_visible(listing) and not is_visible(tree)
            assert send(listing, 0x18B) == 3
            send(listing, 0x197, 0)
            height = send(listing, 0x1A1, 0)
            click(listing, 50, height + 20)
            wait_for(lambda: reader.page() == "2", "thumbnail navigation")
            def rendered_thumbnails():
                image = capture_control(reader, listing)
                counts = image.getcolors(image.width * image.height)
                return image if any(pixel in (RED, GREEN, BLUE) and count >= 5
                                    for count, pixel in counts) else None

            previews = wait_for(rendered_thumbnails, "visible rendered thumbnails")
            previews.save(self.evidence / "thumbnails.png")
            reader.command(1009)
            assert rect(reader.canvas) == original
        return "Bookmark hierarchy expanded and navigated; thumbnail list navigated; hiding reclaimed document width."

    def internal_link(self):
        with Reader(self.exe, self.fixtures / "mixed.pdf") as reader:
            reader.wait_title("mixed.pdf")
            x, y = point_at(reader, RED, 150, 188)
            click(reader.canvas, x, y)
            wait_for(lambda: reader.page() == "3", "internal link page jump")
            wait_marker(reader.canvas, BLUE)
            assert send(reader.control(1021), 0x147) == 1
        return "Clicked a GoTo link, reached page 3, and applied its Fit destination."

    def link_safety(self):
        with Reader(self.exe, self.fixtures / "scripted.pdf") as reader:
            reader.wait_title("scripted.pdf")
            assert not reader.dialogs()
            x, y = point_at(reader, RED, 130, 258)
            click(reader.canvas, x, y)
            time.sleep(0.2)
            assert not reader.dialogs()
            assert reader.page() == "1"
            x, y = point_at(reader, RED, 130, 221)
            click(reader.canvas, x, y)
            dialog = wait_for(lambda: next((d for d in reader.dialogs()
                                            if text(d) == "Open external link"), None),
                              "external link confirmation")
            assert "http://127.0.0.1:49153/plainpdf-test" in dialog_text(dialog)
            reader.respond(dialog, 7)
            wait_for(lambda: not reader.dialogs(), "confirmation decline")
        return "OpenAction script and file URI remained inert; HTTP URI showed the exact target and supported decline."

    def recent(self):
        with Reader(self.exe, self.fixtures / "single.pdf") as reader:
            reader.wait_title("single.pdf")
            reader.command(1020)
            reader.command(1002)
            assert reader.title() == "PlainPDF"
            reader.command(1100)
            assert reader.title() == "PlainPDF"
            temporary = self.evidence / "recent-missing.pdf"
            temporary.write_bytes((self.fixtures / "single.pdf").read_bytes())
            open_file_dialog(reader, temporary)
            reader.wait_title(temporary.name)
            reader.command(1002)
            temporary.unlink()
            reader.command(1100, asynchronous=True)
            dialog = wait_for(lambda: next((d for d in reader.dialogs()
                                            if "Remove it" in dialog_text(d)), None),
                              "missing recent file prompt")
            reader.respond(dialog, 6)
            wait_for(lambda: not reader.dialogs(), "recent removal")
            assert reader.title() == "PlainPDF"
            reader.command(1100)
            assert reader.title() == "PlainPDF"
        return "Clear Recent Files removed rows; missing recent file prompted for removal; record no longer opened."

    def large_and_extreme(self):
        with Reader(self.exe, self.fixtures / "report.pdf") as reader:
            reader.wait_title("report.pdf")
            reader.jump(149)
            wait_for(lambda: reader.page() == "149", "late page jump")
            reader.query("unique report page 150")
            wait_for(lambda: reader.status() == "Match on page 150.", "search at end of 150-page report")
        with Reader(self.exe, self.fixtures / "extreme.pdf") as reader:
            reader.wait_title("extreme.pdf")
            reader.command(1011)
            for _ in range(4):
                reader.command(1013)
            assert reader.page() == "1"
            send(reader.canvas, WM_VSCROLL, 1)
            assert reader.page() == "1"
            capture(reader.canvas, self.evidence / "extreme-tiles.png")
        return "Late-page navigation and search in a 150-page report; high-zoom 90,000-point page remained responsive."


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--fixtures", type=Path, default=Path("out/fixtures"))
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--case", action="append", help="Run only the named test method.")
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    tests = Tests(args.exe, args.fixtures.resolve(), evidence)

    cases = (
        ("AT-02, AT-03", tests.blank_and_open),
        ("AT-04", tests.entry_points),
        ("AT-05, AT-06, AT-08, AT-30 (view)", tests.navigate),
        ("AT-10", tests.find),
        ("AT-11", tests.search_scan),
        ("AT-09", tests.select),
        ("AT-13", tests.password),
        ("AT-14", tests.invalid),
        ("AT-25, AT-26, AT-27", tests.sidebar),
        ("AT-28", tests.internal_link),
        ("AT-29 (confirmation), AT-31 (script)", tests.link_safety),
        ("AT-32", tests.recent),
        ("AT-15, AT-16", tests.large_and_extreme),
    )
    with preserve_recent():
        for ids, case in cases:
            if not args.case or case.__name__ in args.case:
                tests.run(ids, case)
    (evidence / "functional.json").write_text(json.dumps(tests.cases, indent=2) + "\n",
                                                encoding="utf-8")
    return int(any(case["result"] == "Fail" for case in tests.cases))


if __name__ == "__main__":
    raise SystemExit(main())
