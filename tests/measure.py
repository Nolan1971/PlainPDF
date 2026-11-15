"""Measure Release startup, repaint, responsiveness, and retained resources."""

import argparse
import ctypes
import json
import os
import platform
import statistics
import time
from ctypes import wintypes
from pathlib import Path

from acceptance import RED, preserve_recent, wait_marker
from windows_ui import (
    WM_KEYDOWN, WM_VSCROLL, Reader, api, capture, close_handle, dpi_for_window,
    get_gui_resources, kernel32, open_process, post, psapi, rect, send, user32, wait_for,
)

PROPERTIES = ("ViewRenders", "Thumbnails", "PeakPending", "Pending", "Busy",
              "LargestRaster", "CacheBytes", "PeakCacheBytes", "PaintCacheHits")
get_property = api(user32, "GetPropW", wintypes.HANDLE, wintypes.HWND, wintypes.LPCWSTR)
redraw = api(user32, "RedrawWindow", wintypes.BOOL, wintypes.HWND,
             ctypes.c_void_p, wintypes.HRGN, wintypes.UINT)
window_position = api(user32, "SetWindowPos", wintypes.BOOL, wintypes.HWND, wintypes.HWND,
                      ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT)
handle_count = api(kernel32, "GetProcessHandleCount", wintypes.BOOL, wintypes.HANDLE,
                   ctypes.POINTER(wintypes.DWORD))


class ProcessMemory(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD), ("faults", wintypes.DWORD),
        ("peak_working_set", ctypes.c_size_t), ("working_set", ctypes.c_size_t),
        ("peak_paged_pool", ctypes.c_size_t), ("paged_pool", ctypes.c_size_t),
        ("peak_nonpaged_pool", ctypes.c_size_t), ("nonpaged_pool", ctypes.c_size_t),
        ("pagefile", ctypes.c_size_t), ("peak_pagefile", ctypes.c_size_t),
        ("private_bytes", ctypes.c_size_t),
    ]


get_memory = api(psapi, "GetProcessMemoryInfo", wintypes.BOOL, wintypes.HANDLE,
                 ctypes.POINTER(ProcessMemory), wintypes.DWORD)


def counters(reader):
    return {name: (get_property(reader.main, "PlainPDF." + name) or 0) - 1
            for name in PROPERTIES}


def idle(reader, cache_empty=False):
    def check():
        values = counters(reader)
        if values["Pending"] == 0 and values["Busy"] == 0:
            if not cache_empty or values["CacheBytes"] == 0:
                return values
        return None

    return wait_for(check, "idle PDF worker", timeout=18)


def resources(reader):
    handle = open_process(0x0410, False, reader.process.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        memory = ProcessMemory()
        memory.cb = ctypes.sizeof(memory)
        count = wintypes.DWORD()
        if not get_memory(handle, ctypes.byref(memory), memory.cb):
            raise ctypes.WinError(ctypes.get_last_error())
        if not handle_count(handle, ctypes.byref(count)):
            raise ctypes.WinError(ctypes.get_last_error())
        return {
            "private_bytes": memory.private_bytes, "working_set": memory.working_set,
            "gdi": get_gui_resources(handle, 0), "user": get_gui_resources(handle, 1),
            "handles": count.value, "render_cache_bytes": counters(reader)["CacheBytes"],
        }
    finally:
        close_handle(handle)


def launch_times(exe):
    # Warm loader and standby-cache state once before the measured repetitions.
    with Reader(exe):
        pass
    values = []
    for _ in range(10):
        with Reader(exe) as reader:
            values.append(round(1000 * reader.start_seconds, 3))
    return {"milliseconds": values, "median_ms": statistics.median(values)}


def repaint(exe, fixture):
    with Reader(exe, fixture) as reader:
        reader.wait_title(fixture.name)
        wait_marker(reader.canvas, RED)
        before = idle(reader)
        elapsed = []
        for _ in range(20):
            start = time.perf_counter()
            assert redraw(reader.canvas, None, None, 0x0101)
            elapsed.append(round(1000 * (time.perf_counter() - start), 3))
        after = wait_for(lambda: state if
                         (state := counters(reader))["PaintCacheHits"] > before["PaintCacheHits"]
                         else None, "cached repaint instrumentation")
        assert after["ViewRenders"] == before["ViewRenders"]
        return {
            "median_repaint_ms": statistics.median(elapsed), "max_repaint_ms": max(elapsed),
            "cache_hits_added": after["PaintCacheHits"] - before["PaintCacheHits"],
            "pdfium_raster_calls_added": after["ViewRenders"] - before["ViewRenders"],
        }


def repeated_open(exe, fixture, cycles=20):
    samples = []
    with Reader(exe, fixture) as reader:
        reader.wait_title(fixture.name)
        wait_marker(reader.canvas, RED)
        reader.command(1002)
        idle(reader, cache_empty=True)
        samples.append(resources(reader))
        for _ in range(cycles):
            reader.command(1100)
            reader.wait_title(fixture.name)
            wait_marker(reader.canvas, RED)
            reader.command(1002)
            wait_for(lambda: reader.title() == "PlainPDF", "closed document")
            idle(reader, cache_empty=True)
            time.sleep(0.12)
            samples.append(resources(reader))
    print("retained resources", json.dumps(samples), flush=True)
    assert all(sample["render_cache_bytes"] == 0 for sample in samples)
    for key in ("handles", "gdi"):
        assert max(sample[key] for sample in samples[-10:]) - min(
            sample[key] for sample in samples[-10:]) <= 2, f"Unstable retained {key} count"
    # Native USER controls can gain and release transient windows between
    # samples. Compare the lower bounds of two late windows for retention.
    first_user = min(sample["user"] for sample in samples[-10:-5])
    last_user = min(sample["user"] for sample in samples[-5:])
    assert last_user <= first_user + 1, "Retained USER objects grew between late sample windows"
    return {"cycles": cycles, "samples": samples, "late_user_minima": [first_user, last_user]}


def stress(exe, fixture, seconds, evidence):
    with Reader(exe, fixture) as reader:
        reader.wait_title(fixture.name)
        wait_marker(reader.canvas, RED, timeout=20)
        dpi = dpi_for_window(reader.main)
        left, top, right, bottom = rect(reader.main)
        width, height = right - left, bottom - top
        latency = []
        busy_latency = []
        sequence = (1011, 1013, 1013, 1014, 1010, 1012)
        start = time.perf_counter()
        stop = start + seconds
        iteration = 0
        snapshots = []

        while time.perf_counter() < stop:
            if iteration % 8 == 0:
                inset = (iteration % 24) * 5
                assert window_position(reader.main, None, 0, 0, width - inset, height - inset, 0x16)
                snapshots.append(counters(reader))

            busy = counters(reader)["Busy"] == 1
            tick = time.perf_counter()
            reader.command(sequence[iteration % len(sequence)])
            send(reader.canvas, WM_VSCROLL, 3 if (iteration % 14) < 7 else 2)
            sample = 1000 * (time.perf_counter() - tick)
            latency.append(sample)
            if busy:
                busy_latency.append(sample)
            assert 1 <= int(reader.page()) <= 14
            iteration += 1
            time.sleep(0.025)

        recorded = time.perf_counter() - start
        reader.command(1012)
        post(reader.canvas, WM_KEYDOWN, 0x24)
        wait_for(lambda: reader.page() == "1", "page one after zoom/scroll stress")
        wait_marker(reader.canvas, RED, timeout=20)
        capture(reader.canvas, evidence / "after-stress.png")
        last = idle(reader)
        assert last["PeakPending"] <= 96
        assert last["PeakCacheBytes"] <= 256 * 1024 * 1024
        assert last["LargestRaster"] <= 64 * 1024 * 1024
        assert last["ViewRenders"] > 1
        return {
            "seconds": recorded, "dpi": dpi, "iterations": iteration,
            "common_command_pairs": len(latency), "while_worker_busy": len(busy_latency),
            "median_latency_ms": statistics.median(latency),
            "p95_latency_ms": statistics.quantiles(latency, n=20)[18], "max_latency_ms": max(latency),
            "worker_busy_max_latency_ms": max(busy_latency) if busy_latency else None,
            "over_100_ms": sum(value > 100 for value in latency),
            "metrics": last, "snapshots": snapshots, "resources": resources(reader),
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--fixtures", type=Path, default=Path("out/fixtures"))
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--seconds", type=float, default=60)
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    args.fixtures = args.fixtures.resolve()
    os.environ["PLAINPDF_MEASURE"] = "1"
    result = {"windows": platform.platform(), "processor": platform.processor()}
    with preserve_recent():
        for name, measure in (
            ("startup", lambda: launch_times(args.exe)),
            ("cached_repaint", lambda: repaint(args.exe, args.fixtures / "single.pdf")),
            ("repeated_open", lambda: repeated_open(args.exe, args.fixtures / "single.pdf")),
            ("zoom_scroll", lambda: stress(args.exe, args.fixtures / "heavy.pdf",
                                           args.seconds, args.evidence)),
        ):
            result[name] = measure()
            print(name, json.dumps({key: value for key, value in result[name].items()
                                    if key not in ("snapshots", "samples")}), flush=True)
            (args.evidence / "performance.json").write_text(json.dumps(result, indent=2) + "\n",
                                                             encoding="utf-8")


if __name__ == "__main__":
    main()
