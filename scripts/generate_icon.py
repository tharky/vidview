import math
import struct
from pathlib import Path

SIZES = [16, 32, 48, 64, 128, 256]

BG = (22, 24, 29, 255)
TEAL = (45, 212, 191, 255)
AMBER = (245, 158, 11, 255)


def segment_distance(px, py, ax, ay, bx, by):
    vx = bx - ax
    vy = by - ay

    wx = px - ax
    wy = py - ay

    length_squared = vx * vx + vy * vy

    if length_squared == 0:
        return math.hypot(px - ax, py - ay)

    t = (wx * vx + wy * vy) / length_squared
    t = max(0.0, min(1.0, t))

    cx = ax + t * vx
    cy = ay + t * vy

    return math.hypot(px - cx, py - cy)


def inside_rounded_square(x, y):
    margin = 0.04
    radius = 0.19

    left = margin
    right = 1.0 - margin
    top = margin
    bottom = 1.0 - margin

    if left + radius <= x <= right - radius:
        return top <= y <= bottom

    if top + radius <= y <= bottom - radius:
        return left <= x <= right

    corners = [
        (left + radius, top + radius),
        (right - radius, top + radius),
        (left + radius, bottom - radius),
        (right - radius, bottom - radius),
    ]

    return any(
        (x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2
        for cx, cy in corners
    )


def pixel(x, y):
    if not inside_rounded_square(x, y):
        return (0, 0, 0, 0)

    color = BG

    # VidView "V"
    width = 0.055

    d1 = segment_distance(
        x, y,
        0.24, 0.28,
        0.50, 0.72
    )

    d2 = segment_distance(
        x, y,
        0.50, 0.72,
        0.76, 0.28
    )

    if min(d1, d2) <= width:
        color = TEAL

    # Amber marker at the upper-right end.
    if math.hypot(
        x - 0.76,
        y - 0.28
    ) <= 0.07:
        color = AMBER

    return color


def create_icon_image(size):
    pixels = bytearray()

    # Windows DIB pixels are bottom-up and BGRA.
    for py in range(size - 1, -1, -1):
        for px in range(size):
            x = (px + 0.5) / size
            y = (py + 0.5) / size

            r, g, b, a = pixel(x, y)

            pixels.extend(
                (b, g, r, a)
            )

    mask_row_bytes = ((size + 31) // 32) * 4
    mask = bytes(mask_row_bytes * size)

    bitmap_info = struct.pack(
        "<IIIHHIIIIII",
        40,             # header size
        size,
        size * 2,       # XOR + AND height
        1,              # planes
        32,             # bits per pixel
        0,              # BI_RGB
        len(pixels),
        0,
        0,
        0,
        0,
    )

    return bitmap_info + pixels + mask


def main():
    output = (
        Path(__file__).resolve().parent.parent
        / "resources"
        / "VidView.ico"
    )

    output.parent.mkdir(
        parents=True,
        exist_ok=True
    )

    images = [
        create_icon_image(size)
        for size in SIZES
    ]

    count = len(images)

    header = struct.pack(
        "<HHH",
        0,
        1,
        count
    )

    directory = bytearray()

    offset = (
        6 +
        count * 16
    )

    for size, data in zip(
        SIZES,
        images
    ):
        encoded_size = (
            0
            if size == 256
            else size
        )

        directory.extend(
            struct.pack(
                "<BBBBHHII",
                encoded_size,
                encoded_size,
                0,
                0,
                1,
                32,
                len(data),
                offset,
            )
        )

        offset += len(data)

    with output.open("wb") as f:
        f.write(header)
        f.write(directory)

        for data in images:
            f.write(data)

    print(
        f"Created {output}"
    )


if __name__ == "__main__":
    main()