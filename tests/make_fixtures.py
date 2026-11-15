"""Generate deterministic, redistributable test inputs under out/fixtures."""

import argparse
import io
import random
import shutil
from pathlib import Path

from PIL import Image, ImageDraw
from pypdf import PdfReader, PdfWriter
from pypdf.generic import DictionaryObject, NameObject, TextStringObject
from reportlab import rl_config
from reportlab.lib.colors import HexColor
from reportlab.lib.pdfencrypt import StandardEncryption
from reportlab.lib.utils import ImageReader
from reportlab.pdfgen import canvas

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "out" / "fixtures"
COLORS = ("#B82927", "#25704A", "#224F9D")
rl_config.useA85 = 0


def new_pdf(path, size, **options):
    return canvas.Canvas(str(path), pagesize=size, invariant=1, pageCompression=1, **options)


def heading(pdf, size, title, marker):
    _, height = size
    pdf.setFillColor(HexColor(marker))
    pdf.rect(30, height - 47, 24, 24, fill=1, stroke=0)
    pdf.setFillColor(HexColor("#1B1E24"))
    pdf.setFont("Helvetica-Bold", 21)
    pdf.drawString(60, height - 43, title)


def text_page(pdf, size, title, marker, lines):
    heading(pdf, size, title, marker)
    pdf.setFont("Helvetica", 17)
    y = size[1] - 86
    for line in lines:
        pdf.drawString(60, y, line)
        y -= 26


def mixed(path):
    letter = (612, 792)
    landscape = (792, 612)
    annex = (500, 650)
    pdf = new_pdf(path, letter)
    pdf.bookmarkPage("first")
    pdf.addOutlineEntry("Overview", "first", level=0)
    text_page(pdf, letter, "PlainPDF reading fixture", COLORS[0], (
        "needle page one - café and euro €",
        "Selected text can start on this page.",
        "This is the last line before the link tests.",
    ))
    pdf.setFont("Helvetica", 15)
    pdf.drawString(60, 600, "Internal link to the annex")
    pdf.linkRect("", "annex", (60, 596, 247, 616), thickness=1)
    pdf.drawString(60, 565, "External HTTP link")
    pdf.linkURL("http://127.0.0.1:49153/plainpdf-test", (60, 561, 220, 581), thickness=1)
    pdf.drawString(60, 530, "Disallowed file link")
    pdf.linkURL("file:///C:/nonexistent-plainpdf-test.txt", (60, 526, 220, 546), thickness=1)
    pdf.showPage()

    pdf.setPageSize(landscape)
    pdf.bookmarkPage("wide")
    pdf.addOutlineEntry("Wide page", "wide", level=0)
    text_page(pdf, landscape, "Landscape page", COLORS[1], (
        "needle page two - résumé",
        "The second page has a different width and height.",
    ))
    pdf.setFont("Helvetica", 13)
    for i in range(5):
        pdf.drawString(60, 400 - 21 * i, f"Left column line {i + 1}")
        pdf.drawString(410, 400 - 21 * i, f"Right column line {i + 1}")
    pdf.showPage()

    pdf.setPageSize(annex)
    pdf.bookmarkPage("annex", fit="Fit")
    pdf.addOutlineEntry("Annex and form", "annex", level=1)
    text_page(pdf, annex, "Annex", COLORS[2], ("needle page three", "Read-only appearance below."))
    pdf.acroForm.textfield(
        name="sample-field", tooltip="Read-only fixture value", x=60, y=470,
        width=250, height=32, value="STATIC FORM VALUE", forceBorder=True,
        fillColor=HexColor("#F3EBBB"), textColor=HexColor("#51301D"),
        borderColor=HexColor("#923C0A"), fontName="Helvetica", fontSize=16,
    )
    pdf.acroForm.checkbox(
        name="sample-check", tooltip="Read-only fixture check mark", x=60, y=425,
        size=21, checked=True, buttonStyle="check", forceBorder=True,
        fillColor=HexColor("#E0F0E0"), borderColor=HexColor("#245A3E"),
    )
    pdf.showPage()
    pdf.save()


def plain(path, password=None, size=(612, 792), marker=COLORS[0]):
    encryption = None
    if password is not None:
        encryption = StandardEncryption(password, ownerPassword="fixture-owner",
                                        canPrint=1, canCopy=1, strength=128)
    pdf = new_pdf(path, size, encrypt=encryption)
    text_page(pdf, size, "One page", marker, ("opening sample - café", "Only one page."))
    pdf.showPage()
    pdf.save()


def scanned(path):
    image = Image.new("RGB", (960, 1280), "#FDFBF1")
    draw = ImageDraw.Draw(image)
    draw.rectangle((95, 135, 865, 1150), outline="#427E8B", width=5)
    draw.text((135, 195), "IMAGE ONLY - NO TEXT LAYER", fill="#214457")
    draw.ellipse((325, 390, 650, 712), fill="#769AA3")
    pdf = new_pdf(path, (612, 792))
    pdf.drawImage(ImageReader(image), 66, 86, width=480, height=640)
    pdf.showPage()
    pdf.save()


def long_report(path):
    pdf = new_pdf(path, (612, 792))
    for i in range(150):
        text_page(pdf, (612, 792), f"Reference report page {i + 1}", COLORS[i % 3],
                  (f"unique report page {i + 1:03d}", "searchable baseline text"))
        pdf.setFont("Helvetica", 12)
        for line in range(24):
            pdf.drawString(60, 624 - line * 21, f"Reference paragraph line {line + 1:02d}.")
        pdf.showPage()
    pdf.save()


def image_heavy(path):
    generator = random.Random(81052)
    pdf = new_pdf(path, (612, 792))
    for i in range(14):
        width, height = 2050, 1490
        image = Image.frombytes("RGB", (width, height), generator.randbytes(width * height * 3))
        pdf.drawImage(ImageReader(image), 30, 140, 552, 540)
        text_page(pdf, (612, 792), f"Image stress page {i + 1}", COLORS[i % 3], ("image stress",))
        pdf.showPage()
    pdf.save()
    assert path.stat().st_size >= 100 * 1024 * 1024


def hostile(source, path):
    writer = PdfWriter(clone_from=source)
    writer.root_object[NameObject("/OpenAction")] = DictionaryObject({
        NameObject("/S"): NameObject("/JavaScript"),
        NameObject("/JS"): TextStringObject("app.alert('DO NOT RUN PDF SCRIPT');"),
    })
    writer.write(path)
    assert "/OpenAction" in PdfReader(path).root_object


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--heavy", action="store_true", help="Generate the 100+ MiB image stress PDF.")
    options = parser.parse_args()
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    mixed(output / "mixed.pdf")
    plain(output / "single.pdf")
    plain(output / "locked.pdf", password="plainpass", marker=COLORS[1])
    plain(output / "extreme.pdf", size=(90000, 72000), marker=COLORS[2])
    scanned(output / "scan.pdf")
    long_report(output / "report.pdf")
    hostile(output / "mixed.pdf", output / "scripted.pdf")
    shutil.copyfile(output / "single.pdf", output / "café – 日本語.pdf")
    (output / "empty.pdf").write_bytes(b"")
    (output / "invalid.pdf").write_bytes(b"This is ordinary text.\r\n")
    (output / "truncated.pdf").write_bytes((output / "single.pdf").read_bytes()[:190])

    if options.heavy:
        image_heavy(output / "heavy.pdf")

    print(f"Fixtures written to {output}")


if __name__ == "__main__":
    main()
