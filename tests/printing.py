"""Verify real Windows printing through the Microsoft Print to PDF queue only."""

import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path

from PIL import Image, ImageChops
from pypdf import PdfReader
from pypdf.errors import PdfReadError

from acceptance import BLUE, FORM, GREEN, RED, dialog_text, preserve_recent
from windows_ui import (
    BM_CLICK, WM_CLOSE, WM_COMMAND, Reader, class_name, descendants, get_id,
    is_enabled, is_visible, post, send, set_foreground, set_text, text, wait_for,
)

QUEUE_COMMENT = "Temporary PlainPDF acceptance-test queue"
LVM_GETITEMCOUNT = 0x1004
SELECT_PRINTER = Path(__file__).resolve().parents[1] / "out/test/tools/SelectPrinter.exe"


def dialog_by_title(reader, title):
    return next((window for window in reader.dialogs() if text(window) == title), None)


def show_print_dialog(reader):
    reader.command(1003, asynchronous=True)
    return wait_for(lambda: dialog_by_title(reader, "Print"), "the Windows Print dialog")


def print_control(dialog, ident, cls):
    matches = [window for window in descendants(dialog)
               if get_id(window) == ident and class_name(window) == cls
               and is_visible(window) and is_enabled(window)]
    if len(matches) != 1:
        raise AssertionError(f"No unique Print dialog {cls} control {ident}")
    return matches[0]


def select_virtual(dialog, mode):
    # The test-created Microsoft PDF queue has a unique comment. Read back the
    # Windows dialog's selected printer comment before enabling submission.
    radio = print_control(dialog, {"All": 1056, "Current": 1058, "Range": 1059}[mode], "Button")
    set_foreground(dialog)
    post(radio, BM_CLICK)
    wait_for(lambda: send(radio, 0x00F0) == 1, "requested print range")
    if mode == "Range":
        set_text(print_control(dialog, 1152, "Edit"), "2-3")

    listing = print_control(dialog, 1, "SysListView32")
    comment = print_control(dialog, 1009, "Edit")
    wait_for(lambda: send(listing, LVM_GETITEMCOUNT) > 0, "printer list populated")

    def exact_selection(action):
        result = subprocess.run([str(SELECT_PRINTER), str(listing), action],
                                capture_output=True, text=True, timeout=8,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        return result.returncode == 0

    wait_for(lambda: exact_selection("select"), "selection of Microsoft Print to PDF")
    wait_for(lambda: text(comment) == QUEUE_COMMENT, "verified test queue comment")
    assert send(radio, 0x00F0) == 1, "Page selection changed. Print was not invoked."
    submit = print_control(dialog, 1, "Button")
    assert text(submit).replace("&", "") == "Print"
    assert exact_selection("verify"), "Virtual printer lost selection. Print was not invoked."
    post(dialog, WM_COMMAND, 1, submit)


def print_to(reader, mode, output, reject_source=False):
    if output.exists() and not reject_source:
        raise AssertionError(f"Refusing to overwrite an existing test file {output}")
    dialog = show_print_dialog(reader)
    select_virtual(dialog, mode)
    try:
        save = wait_for(lambda: dialog_by_title(reader, "Save Print Output"),
                        "the virtual printer output filename dialog")
    except AssertionError as error:
        remaining = [text(window) for window in reader.dialogs()]
        raise AssertionError(f"{error}. Reader status {reader.status()!r}, dialogs {remaining}.") from error
    controls = descendants(save)
    name = next((hwnd for hwnd in controls if get_id(hwnd) == 1148 and is_visible(hwnd)), None)
    candidates = descendants(name) if name else controls
    edit = next((hwnd for hwnd in candidates if class_name(hwnd) == "Edit"
                 and is_enabled(hwnd) and is_visible(hwnd)), None)
    assert edit, "Missing print output file-name field"
    set_text(edit, str(output))
    reader.respond(save, 1)
    if reject_source:
        def overwrite_decision():
            if reader.status() == "Choose a print output file other than the original PDF.":
                return True
            return next((window for window in reader.dialogs()
                         if text(window) == "Confirm Save As" or
                         "already exists" in dialog_text(window)), None)

        overwrite = wait_for(overwrite_decision,
                             "Windows replacement question or reader source-file rejection")
        if overwrite is not True:
            choices = [node for node in descendants(overwrite)
                       if class_name(node) == "Button" and is_visible(node)
                       and is_enabled(node) and text(node).replace("&", "") == "Yes"]
            assert len(choices) == 1, "Cannot find an unambiguous confirmation for the test copy."
            yes = choices[0]
            set_foreground(overwrite)
            post(yes, BM_CLICK)
        try:
            wait_for(lambda: reader.status() == "Choose a print output file other than the original PDF.",
                     "reader rejected the source as print output")
        except AssertionError as error:
            choices = [(text(dialog), [(get_id(node), class_name(node), text(node))
                                       for node in descendants(dialog) if is_visible(node)])
                       for dialog in reader.dialogs()]
            raise AssertionError(f"{error}. Status {reader.status()!r}, dialogs {choices}.") from error
    else:
        wait_for(lambda: "Printing" in reader.status() or "Print job submitted." == reader.status(),
                 "printing or submission in progress")


def wait_pdf(path, pages):
    def complete():
        if not path.exists() or path.stat().st_size < 1000:
            return None
        try:
            with path.open("rb") as stream:
                document = PdfReader(stream)
                return len(document.pages) == pages
        except (OSError, PdfReadError, ValueError):
            return None

    wait_for(complete, f"complete {pages}-page print output", timeout=25)


def color_box(image, color):
    delta = ImageChops.difference(Image.new("RGB", image.size, color), image)
    red, green, blue = delta.split()
    difference = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    return difference.point(lambda sample: 255 if sample <= 20 else 0).getbbox()


def verify_output(path, colors, includes_form, poppler):
    wait_pdf(path, len(colors))
    prefix = path.with_suffix("")
    subprocess.run([poppler, "-png", "-r", "72", path, prefix], check=True,
                   creationflags=subprocess.CREATE_NO_WINDOW, timeout=25)
    for i, color in enumerate(colors, 1):
        picture = Path(str(prefix) + f"-{i}.png")
        with Image.open(picture) as raster:
            page = raster.convert("RGB")
            box = color_box(page, color)
            assert box is not None, f"The expected page color is absent from print page {i}"
            _, _, right, bottom = box
            width = right - box[0]
            height = bottom - box[1]
            assert 0.80 <= width / height <= 1.25, f"Square marker distorted on print page {i}"
            if includes_form and i == len(colors):
                assert color_box(page, FORM), "Static form appearance absent from print output"
    return {"page_count": len(colors), "markers": colors, "includes_static_form": includes_form}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--fixtures", type=Path, default=Path("out/fixtures"))
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--poppler", type=Path, required=True)
    parser.add_argument("--inspect", action="store_true")
    options = parser.parse_args()
    os.environ["PLAINPDF_TEST_VIRTUAL_PRINT"] = "1"
    evidence = options.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    file = options.fixtures.resolve() / "mixed.pdf"
    digest = hashlib.sha256(file.read_bytes()).hexdigest()
    report = {}
    with preserve_recent(), Reader(options.exe, file) as reader:
        reader.wait_title(file.name)
        if options.inspect:
            dialog = show_print_dialog(reader)
            print([(class_name(window), get_id(window), text(window))
                   for window in descendants(dialog) if is_visible(window)])
            post(dialog, WM_CLOSE)
            return

        dialog = show_print_dialog(reader)
        post(dialog, WM_CLOSE)
        wait_for(lambda: not reader.dialogs(), "print dialog cancel")
        assert reader.title() == "mixed.pdf — PlainPDF"
        report["dialog_cancel"] = "Closed without a print job."

        for mode, colors, form in (
            ("All", [RED, GREEN, BLUE], True),
            ("Current", [GREEN], False),
            ("Range", [GREEN, BLUE], True),
        ):
            if mode == "Current":
                reader.jump(2)
                wait_for(lambda: reader.page() == "2", "current page 2")
            output = evidence / (mode.lower() + ".pdf")
            print_to(reader, mode, output)
            wait_for(lambda: reader.status() in ("Print job submitted.", "Printing failed."),
                     "print completion", timeout=25)
            assert reader.status() == "Print job submitted."
            report[mode] = verify_output(output, colors, form, options.poppler)
            print(mode, report[mode], flush=True)
            (evidence / "printing.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    report_file = options.fixtures.resolve() / "report.pdf"
    with preserve_recent(), Reader(options.exe, report_file) as reader:
        reader.wait_title(report_file.name)
        print_to(reader, "All", evidence / "cancelled-report.pdf")
        assert reader.status().startswith("Printing…"), "Job finished before cancellation"
        reader.command(1002)
        wait_for(lambda: reader.title() == "PlainPDF", "document closed during printing")
        wait_for(lambda: reader.status() == "Printing cancelled.", "in-progress print cancelled",
                 timeout=20)
        report["active_job_cancel"] = "Closed the 150-page document while the job was active."
        print(report["active_job_cancel"], flush=True)

    # Exercise the overwrite guard against a private test copy.
    protected = evidence / "protected-source.pdf"
    protected.write_bytes(file.read_bytes())
    with preserve_recent(), Reader(options.exe, protected) as reader:
        reader.wait_title(protected.name)
        print_to(reader, "Current", protected, reject_source=True)
    assert hashlib.sha256(protected.read_bytes()).hexdigest() == digest
    assert hashlib.sha256(file.read_bytes()).hexdigest() == digest
    report["original_unchanged"] = True
    report["original_overwrite_guard"] = "Selected the open synthetic PDF as output; the reader refused before spooling."
    (evidence / "printing.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
