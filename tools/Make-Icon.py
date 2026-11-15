"""Draw the PlainPDF icon. Requires Pillow, which is also used by the UI tests."""

from pathlib import Path

from PIL import Image, ImageDraw


SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
INK = "#24344A"
PAPER = "#F7F9FC"
FOLD = "#BBC9D8"
RULE = "#7389A2"
RED = "#B6253D"
DARK_RED = "#841A2D"
WHITE = "#FFFFFF"


def small_icon():
    # At 16 px each letter has its own three-by-five pixel grid.
    image = Image.new("RGBA", (16, 16))
    draw = ImageDraw.Draw(image)
    draw.polygon([(3, 1), (9, 1), (12, 4), (12, 14), (3, 14)], fill=INK)
    draw.polygon([(4, 2), (8, 2), (8, 5), (11, 5), (11, 13), (4, 13)], fill=PAPER)
    draw.polygon([(9, 2), (9, 4), (11, 4)], fill=FOLD)
    draw.line((5, 7, 10, 7), fill=RULE)
    draw.rounded_rectangle((1, 9, 14, 15), radius=1, fill=RED)
    rows = ("## .## .###", "# #.# #.#  ", "## .# #.## ", "#  .# #.#  ", "#  .## .#  ")
    for y, row in enumerate(rows, 10):
        for x, pixel in enumerate(row, 2):
            if pixel == "#":
                draw.point((x, y), fill=WHITE)
    return image


def document_icon():
    image = Image.new("RGBA", (256, 256))
    draw = ImageDraw.Draw(image)

    def polygon(points, color):
        draw.polygon([(x * 4, y * 4) for x, y in points], fill=color)

    def block(box, color, radius=0):
        draw.rounded_rectangle(tuple(value * 4 for value in box), radius * 4, fill=color)

    polygon([(12, 3), (38, 3), (52, 17), (52, 62), (12, 62)], INK)
    polygon([(15, 6), (36, 6), (36, 20), (49, 20), (49, 59), (15, 59)], PAPER)
    polygon([(39, 8), (48, 17), (39, 17)], FOLD)
    block((20, 25, 43, 28), RULE, 1)
    block((20, 31, 36, 34), RULE, 1)
    block((4, 38, 60, 58), DARK_RED, 3)
    block((4, 38, 60, 56), RED, 3)

    # Draw the badge lettering as shapes so regeneration needs no font.
    polygon([(14, 43), (21, 43), (24, 45), (24, 48), (21, 50), (17, 50),
             (17, 53), (14, 53)], WHITE)
    block((17, 45, 20, 47), RED, 1)
    polygon([(27, 43), (33, 43), (37, 46), (37, 50), (33, 53), (27, 53)], WHITE)
    polygon([(30, 45), (32, 45), (34, 47), (34, 49), (32, 51), (30, 51)], RED)
    polygon([(40, 43), (50, 43), (50, 45), (43, 45), (43, 47), (48, 47),
             (48, 49), (43, 49), (43, 53), (40, 53)], WHITE)
    return image


if __name__ == "__main__":
    path = Path(__file__).resolve().parents[1] / "src" / "PlainPDF.ico"
    icon = document_icon()
    icon.save(path, sizes=[(size, size) for size in SIZES], append_images=[small_icon()])
