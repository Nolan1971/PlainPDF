"""Exercise opening and rendering through the real PlainPDF window."""

import argparse
from pathlib import Path

from windows_ui import Reader, capture, wait_for

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("exe", type=Path)
parser.add_argument("pdf", type=Path)
parser.add_argument("--capture", type=Path)
parser.add_argument("--marker", default="B82927")
options = parser.parse_args()
with Reader(options.exe, options.pdf.resolve()) as reader:
    reader.wait_title(options.pdf.name)
    wait_for(lambda: reader.page() == "1", "one-based page field")
    if options.capture:
        marker = bytes.fromhex(options.marker)
        def rendered():
            image = capture(reader.canvas)
            colors = image.getcolors(image.width * image.height)
            return image if any(pixel == tuple(marker) and count >= 100 for count, pixel in colors) else None

        picture = wait_for(rendered, "visible rendered pixels")
        options.capture.parent.mkdir(parents=True, exist_ok=True)
        picture.save(options.capture)
    print(f"{reader.title()}, page {reader.page()}, startup {reader.start_seconds:.3f}s")
