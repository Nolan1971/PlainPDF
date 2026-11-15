"""Exercise replacement, close, shutdown, and Unicode long paths in the real UI."""

import argparse
import json
import os
import shutil
import subprocess
import time
from pathlib import Path

from acceptance import RED, open_file_dialog, preserve_recent, wait_marker
from measure import counters, idle
from windows_ui import Reader, wait_for

SCAN = (118, 154, 163)


def schedule_expensive_work(reader):
    reader.command(1011)
    reader.jump(8)
    for _ in range(4):
        reader.command(1013)
    reader.query("query that deliberately does not exist in the heavy fixture")


def observe_work(reader):
    for attempt in range(10):
        schedule_expensive_work(reader)
        if attempt % 2 == 0:
            reader.jump(10)
        try:
            return wait_for(lambda: state if (state := counters(reader))["Busy"] > 0
                            or state["Pending"] > 0 else None, "queued PDF work", timeout=0.25)
        except AssertionError:
            continue
    raise AssertionError("No pending or active PDF worker state was observable")


def replace(exe, fixtures):
    heavy = fixtures / "heavy.pdf"
    scan = fixtures / "scan.pdf"
    with Reader(exe, scan) as reader:
        reader.wait_title(scan.name)
        open_file_dialog(reader, heavy)
        reader.wait_title(heavy.name)
        wait_marker(reader.canvas, RED, timeout=20)
        before_replace = observe_work(reader)
        reader.command(1101)
        reader.wait_title(scan.name)
        wait_marker(reader.canvas, SCAN)
        wait_for(lambda: not reader.dialogs(), "stable replacement document")
        assert reader.page() == "1"
        reader.command(1101)
        reader.wait_title(heavy.name)
        before_close = observe_work(reader)
        reader.command(1002)
        assert reader.title() == "PlainPDF"
        idle(reader, cache_empty=True)
        reader.command(1100)
        reader.wait_title(heavy.name)
        before_shutdown = observe_work(reader)
    return {
        "before_replace": before_replace, "before_close": before_close,
        "before_shutdown": before_shutdown,
        "outcome": "Replacement painted the new scan; close emptied the cache; shutdown exited normally.",
    }


def rename(exe, fixtures, evidence):
    original = evidence / "renamed-source.pdf"
    changed = evidence / "renamed-target.pdf"
    shutil.copyfile(fixtures / "single.pdf", original)
    with Reader(exe, original) as reader:
        reader.wait_title(original.name)
        wait_marker(reader.canvas, RED)
        original.replace(changed)
        wait_for(lambda: reader.title() == "PlainPDF", "source rename detection")
        assert "moved, removed, or changed" in reader.status()
    return "Renaming an open PDF invalidated the live document and its cached display."


def long_path(exe, fixtures, evidence):
    path = evidence / "long paths"
    while len(str(path)) < 290:
        path = path / "Unicode file path test segment"
    normal = path / "résumé_日本語.pdf"
    native = Path("\\\\?\\" + str(normal))
    native.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(fixtures / "single.pdf", native)
    with Reader(exe, normal) as reader:
        reader.wait_title(normal.name)
        wait_marker(reader.canvas, RED)
        assert reader.page() == "1"
    return f"Command-line opening rendered a {len(str(normal))}-character Unicode path."


def audit_local_runtime(exe, fixtures, evidence):
    fixture = fixtures / "scripted.pdf"
    with Reader(exe, fixture) as reader:
        reader.wait_title(fixture.name)
        wait_marker(reader.canvas, RED)
        reader.query("needle")
        wait_for(lambda: "Match" in reader.status(), "local search")
        for _ in range(3):
            status = subprocess.check_output(["netstat.exe", "-ano"], text=True,
                                             creationflags=subprocess.CREATE_NO_WINDOW,
                                             timeout=8)
            pid = str(reader.process.pid)
            assert not any(line.split()[-1:] == [pid] for line in status.splitlines())
            assert not reader.dialogs()

    incomplete = evidence / "missing-pdfium"
    incomplete.mkdir(exist_ok=True)
    standalone = incomplete / "PlainPDF.exe"
    shutil.copyfile(exe, standalone)
    with Reader(standalone, working_directory=exe.resolve().parent) as reader:
        message = wait_for(lambda: note if "PDFium" in (note := reader.status()) else None,
                           "clear error for missing application-local PDFium")
        assert reader.title() == "PlainPDF"
    return {"local_read_search": "No TCP or UDP endpoints observed for the reader PID.",
            "missing_app_dll": message}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--fixtures", type=Path, default=Path("out/fixtures"))
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    fixtures = args.fixtures.resolve()
    os.environ["PLAINPDF_MEASURE"] = "1"
    start = time.perf_counter()
    with preserve_recent():
        report = {"lifetime": replace(args.exe, fixtures),
                  "source_rename": rename(args.exe, fixtures, evidence),
                  "long_path": long_path(args.exe, fixtures, evidence),
                  "local_runtime": audit_local_runtime(args.exe, fixtures, evidence)}
    report["elapsed_seconds"] = round(time.perf_counter() - start, 3)
    (evidence / "lifetime.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
