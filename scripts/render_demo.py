#!/usr/bin/env python3
"""把程序的真实控制台输出渲染成终端窗口截图（用于文档展示运行效果）。"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
FONT_INDEX = 7  # Noto Sans Mono CJK SC
SIZE = 21
LINE_H = 30
PAD_X = 26
PAD_TOP = 52
PAD_BOTTOM = 26

BG = (30, 30, 30)
TITLE_BG = (45, 45, 45)
DEFAULT = (212, 212, 212)
DIM = (128, 128, 128)
BLUE = (97, 175, 239)
GREEN = (152, 195, 121)
YELLOW = (229, 192, 123)
RED = (240, 113, 120)


def line_color(line):
    if "][ERROR]" in line:
        return RED
    if "][WARN]" in line:
        return YELLOW
    if "[已拦截]" in line:
        return GREEN
    if line.startswith("---") or line.startswith("==="):
        return BLUE
    return DEFAULT


def render(lines, out_path, title="./build/ros_demo"):
    font = ImageFont.truetype(FONT_PATH, SIZE, index=FONT_INDEX)
    try:
        bold = ImageFont.truetype(FONT_PATH, SIZE, index=FONT_INDEX)
    except OSError:
        bold = font

    max_width = max(font.getlength(line) for line in lines) + 4
    width = int(max_width) + PAD_X * 2
    height = PAD_TOP + LINE_H * len(lines) + PAD_BOTTOM

    image = Image.new("RGB", (width, height), BG)
    draw = ImageDraw.Draw(image)

    # 标题栏
    draw.rectangle([0, 0, width, 38], fill=TITLE_BG)
    for i, color in enumerate([(255, 95, 86), (255, 189, 46), (39, 201, 63)]):
        cx = 20 + i * 22
        draw.ellipse([cx - 7, 12, cx + 7, 26], fill=color)
    draw.text((100, 9), title, font=font, fill=(200, 200, 200))

    for index, line in enumerate(lines):
        y = PAD_TOP + index * LINE_H
        color = line_color(line)
        if line.startswith("[") and "]" in line:
            # 时间戳压暗、级别与内容正常色
            stamp_end = line.find("]") + 1
            draw.text((PAD_X, y), line[:stamp_end], font=font, fill=DIM)
            draw.text((PAD_X + font.getlength(line[:stamp_end]), y), line[stamp_end:],
                      font=font, fill=color)
        else:
            draw.text((PAD_X, y), line if line else " ", font=font, fill=color)

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    image.save(out_path)
    print(f"{out_path}  {width}x{height}")


def find_section(output, keyword):
    for index, line in enumerate(output):
        if line.strip().startswith(keyword):
            return index
    return len(output)


def main():
    output = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    section3 = find_section(output, "--- 3.")
    section5 = find_section(output, "--- 5.")
    section6 = find_section(output, "--- 6.")
    blocks = {
        "a": output[:40],
        "b": output[section3:section5]
        + ["", "……（此处省略第 5 幕「运行时动态增删节点」的日志）……", ""]
        + output[section6:],
    }
    for name, lines in blocks.items():
        render(lines, f"docs/img/运行效果-{name}.png")


if __name__ == "__main__":
    main()
