"""Inspect all three packages, then exercise the two locally runnable installers."""

import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import tempfile
import winreg
from pathlib import Path
from zipfile import ZipFile

from acceptance import RED, preserve_recent, wait_marker
from icon_checks import check_file_icon, check_window_icon
from windows_ui import Reader, wait_for

ARCHITECTURES = {"x86": 0x014C, "x64": 0x8664, "arm64": 0xAA64}
UNINSTALL = r"Software\Microsoft\Windows\CurrentVersion\Uninstall"
OPEN_WITH = r"Software\Classes\.pdf\OpenWithProgids"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def machine(path):
    data = path.read_bytes()
    assert data.startswith(b"MZ")
    offset = struct.unpack_from("<I", data, 60)[0]
    assert data[offset:offset + 4] == b"PE\0\0"
    return struct.unpack_from("<H", data, offset + 4)[0]


def imports(path, dumpbin):
    result = subprocess.run([dumpbin, "/dependents", path], check=True, capture_output=True,
                            text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    dlls = [line.strip() for line in result.stdout.splitlines()
            if re.fullmatch(r"\s+[A-Za-z0-9_-]+\.dll", line, re.I)]
    assert dlls, f"No imports read from {path}"
    assert not any(re.match(r"(vcruntime|msvcp|ucrtbase[d]?|vcomp)", dll, re.I) for dll in dlls)
    return dlls


def registry_value(path, name):
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as key:
            return winreg.QueryValueEx(key, name)[0]
    except FileNotFoundError:
        return None


def run_installer(exe):
    subprocess.run([exe, "/S"], check=True, timeout=25, creationflags=subprocess.CREATE_NO_WINDOW)


def static_checks(directory, version, architecture, work, sevenzip, dumpbin, checked_hashes):
    base = f"PlainPDF-{version}-{architecture}"
    archive = directory / (base + ".zip")
    setup = directory / (base + "-setup.exe")
    for file in (archive, setup):
        assert sha(file) == checked_hashes[file.name], f"Checksum mismatch for {file.name}"
    zipper = work / "zip"
    installer = work / "installer"
    zipper.mkdir(parents=True)
    installer.mkdir()
    with ZipFile(archive) as zipped:
        zipped.extractall(zipper)
    subprocess.run([sevenzip, "x", "-y", "-bd", "-bso0", "-o" + str(installer), setup],
                   check=True, creationflags=subprocess.CREATE_NO_WINDOW)
    architecture_code = ARCHITECTURES[architecture]
    payload = {}
    for binary in ("PlainPDF.exe", "pdfium.dll"):
        executable = zipper / binary
        assert machine(executable) == architecture_code
        assert machine(installer / binary) == architecture_code
        assert sha(executable) == sha(installer / binary)
        payload[binary] = {"machine": hex(architecture_code), "sha256": sha(executable),
                           "dependencies": imports(executable, dumpbin)}
    expected = set(item.relative_to(zipper) for item in zipper.rglob("*") if item.is_file())
    assert Path("LICENSE.txt") in expected and Path("README.md") in expected
    assert Path("licenses/pdfium/LICENSE") in expected
    assert Path("licenses/pdfium/manifest.json") in expected
    assert Path("licenses/pdfium/licenses/libopenjpeg.txt") in expected
    for item in expected:
        assert sha(zipper / item) == sha(installer / item), f"Package mismatch for {item}"
    assert machine(setup) == 0x014C
    check_file_icon(zipper / "PlainPDF.exe")
    check_file_icon(setup)
    return {
        "portable": str(archive), "installer": str(setup), "payload_files": len(expected),
        "binaries": payload, "installer_stub_machine": "0x14c",
        "embedded_icons": "App and installer matched at 16, 32 and 256 px.",
        "sha256": {str(path.name): sha(path) for path in (archive, setup)},
    }


def check_reader(executable, fixture):
    with Reader(executable, fixture) as reader:
        reader.wait_title(fixture.name)
        wait_marker(reader.canvas, RED)
        check_window_icon(reader.main)


def live_checks(architecture, info, extracted, fixture):
    key = UNINSTALL + "\\PlainPDF-" + architecture
    install = Path(os.environ["LOCALAPPDATA"]) / "Programs" / ("PlainPDF-" + architecture)
    menu = Path(os.environ["APPDATA"]) / "Microsoft" / "Windows" / "Start Menu" / "Programs"
    shortcut = menu / f"PlainPDF ({architecture}).lnk"
    handler = "PlainPDF.PDF." + architecture
    assert registry_value(key, "InstallLocation") is None and not shortcut.exists()
    assert not install.exists() or (install.is_dir() and not any(install.iterdir()))
    before_association = registry_value(OPEN_WITH, handler)
    before_default = registry_value(r"Software\Classes\.pdf", "")
    run_installer(info["installer"])
    uninstaller = install / "Uninstall.exe"
    userdata = install / "plainpdf-test-user-file.txt"
    assert uninstaller.is_file(), "Per-user uninstaller missing"
    try:
        assert registry_value(key, "InstallLocation") == str(install)
        assert shortcut.is_file()
        environment = dict(os.environ, PLAINPDF_TEST_SHORTCUT=str(shortcut))
        link = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
             "(New-Object -ComObject WScript.Shell).CreateShortcut($env:PLAINPDF_TEST_SHORTCUT).IconLocation"],
            env=environment, check=True, capture_output=True, text=True,
            creationflags=subprocess.CREATE_NO_WINDOW, timeout=15)
        assert link.stdout.strip().casefold() == f"{install / 'PlainPDF.exe'},0".casefold()
        assert registry_value(OPEN_WITH, handler) == before_association
        assert registry_value(r"Software\Classes\.pdf", "") == before_default
        assert sha(install / "PlainPDF.exe") == info["binaries"]["PlainPDF.exe"]["sha256"]
        assert sha(install / "pdfium.dll") == info["binaries"]["pdfium.dll"]["sha256"]

        check_reader(install / "PlainPDF.exe", fixture)
        check_file_icon(uninstaller)
        userdata.write_text("Uninstall must preserve this unrelated file.\n", encoding="utf-8")
    finally:
        run_installer(uninstaller)
        wait_for(lambda: not (install / "PlainPDF.exe").exists()
                 and not (install / "pdfium.dll").exists()
                 and not (install / "licenses").exists()
                 and not uninstaller.exists()
                 and not shortcut.exists()
                 and registry_value(key, "InstallLocation") is None, "uninstall completion")
    assert not shortcut.exists()
    assert not (install / "pdfium.dll").exists() and not (install / "licenses").exists()
    assert registry_value(OPEN_WITH, handler) == before_association
    assert registry_value(r"Software\Classes\.pdf", "") == before_default
    assert userdata.read_text(encoding="utf-8").startswith("Uninstall must preserve")
    shutil.move(userdata, extracted.parent / "preserved-user-file.txt")
    return "Per-user install, installed raster display, shortcut and uninstaller icons, and uninstall preserving unrelated data and associations."


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--version", default="1.0.0-rc.2")
    parser.add_argument("--sevenzip", type=Path, required=True)
    parser.add_argument("--dumpbin", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, default=Path("out/fixtures/single.pdf"))
    parser.add_argument("--static-only", action="store_true")
    parser.add_argument("--skip-install", action="append", default=[], choices=("x86", "x64"),
                        help="Keep an existing installation and exercise only the portable app.")
    args = parser.parse_args()
    directory = args.directory.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    expected = {}
    for entry in (directory / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
        digest, name = entry.split("  ", 1)
        expected[name] = digest
    report = {}
    with tempfile.TemporaryDirectory(prefix="plainpdf-packages-", dir=args.report.parent) as temporary:
        with preserve_recent():
            for architecture in ARCHITECTURES:
                work = Path(temporary) / architecture
                info = static_checks(directory, args.version, architecture, work,
                                     args.sevenzip, args.dumpbin, expected)
                info["runtime"] = "Not Tested"
                if architecture in ("x86", "x64") and not args.static_only:
                    extracted = work / "zip"
                    fixture = args.fixture.resolve()
                    check_reader(extracted / "PlainPDF.exe", fixture)
                    if architecture in args.skip_install:
                        info["runtime"] = "Portable raster display and window icons. Installer execution skipped."
                    else:
                        installed = live_checks(architecture, info, extracted, fixture)
                        info["runtime"] = f"Portable raster display and window icons. {installed}"
                report[architecture] = info
                print(f"{architecture}: {info['payload_files']} matching payload files. {info['runtime']}",
                      flush=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
