#!/usr/bin/env python3
"""把 docs/*.md 排版为提交用 Word 文档（A4、规范标题层级、表格、代码块、页码、目录域）。

用法：python3 scripts/md2docx.py docs/01-需求文档.md docs/word/01-需求文档.docx
"""
import re
import sys
from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_ALIGN_VERTICAL
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.shared import Cm, Pt, RGBColor
from docx.oxml import OxmlElement
from docx.oxml.ns import qn

BODY_FONT_CN = "宋体"
BODY_FONT_EN = "Times New Roman"
HEAD_FONT_CN = "黑体"
CODE_FONT = "Consolas"

projects = {}


def set_font(run, cn_font, en_font, size, bold=False, color=None):
    run.font.name = en_font
    run.font.size = Pt(size)
    run.font.bold = bold
    if color is not None:
        run.font.color.rgb = color
    rpr = run._element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    rfonts.set(qn("w:eastAsia"), cn_font)


def shade(paragraph_or_cell, hex_color):
    element = paragraph_or_cell._element
    if element.tag.endswith("}tc"):
        pr = element.get_or_add_tcPr()
    else:
        pr = element.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), hex_color)
    pr.append(shd)


def configure_styles(doc):
    normal = doc.styles["Normal"]
    normal.font.name = BODY_FONT_EN
    normal.font.size = Pt(12)
    set_style_east_asia(normal, BODY_FONT_CN)
    pf = normal.paragraph_format
    pf.line_spacing = 1.5
    pf.space_before = Pt(0)
    pf.space_after = Pt(0)
    pf.first_line_indent = Pt(24)
    pf.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY

    specs = {
        "Title": (18, 18, 0),
        "Heading 1": (16, 14, 6),
        "Heading 2": (14, 11, 5),
        "Heading 3": (12, 9, 4),
    }
    for name, (size, before, after) in specs.items():
        style = doc.styles[name]
        style.font.name = BODY_FONT_EN
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = RGBColor(0, 0, 0)
        set_style_east_asia(style, HEAD_FONT_CN)
        style.paragraph_format.space_before = Pt(before)
        style.paragraph_format.space_after = Pt(after)
        style.paragraph_format.line_spacing = 1.5
        style.paragraph_format.first_line_indent = Pt(0)
        style.paragraph_format.keep_with_next = True
        style.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.LEFT

    subtitle = doc.styles["Subtitle"]
    subtitle.font.size = Pt(12)
    subtitle.font.color.rgb = RGBColor(0, 0, 0)
    set_style_east_asia(subtitle, "楷体")
    subtitle.paragraph_format.space_after = Pt(12)
    subtitle.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER

    code = doc.styles.add_style("Code Block", 1)  # paragraph style
    code.font.name = CODE_FONT
    code.font.size = Pt(8.5)
    set_style_east_asia(code, BODY_FONT_CN)
    cpf = code.paragraph_format
    cpf.line_spacing = 1.0
    cpf.space_before = Pt(0)
    cpf.space_after = Pt(0)
    cpf.first_line_indent = Pt(0)
    cpf.left_indent = Cm(0.3)
    cpf.alignment = WD_ALIGN_PARAGRAPH.LEFT


def set_style_east_asia(style, font_name):
    rpr = style.element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    rfonts.set(qn("w:eastAsia"), font_name)


def add_page_field(paragraph):
    run = paragraph.add_run()
    set_font(run, BODY_FONT_CN, BODY_FONT_EN, 10.5)
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = " PAGE "
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    text = OxmlElement("w:t")
    text.text = "1"
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    for node in (begin, instr, separate, text, end):
        run._r.append(node)


def restart_numbering(section):
    sect_pr = section._sectPr
    pg = OxmlElement("w:pgNumType")
    pg.set(qn("w:start"), "1")
    sect_pr.append(pg)


def add_toc(doc):
    paragraph = doc.add_paragraph()
    paragraph.paragraph_format.first_line_indent = Pt(0)
    run = paragraph.add_run()
    set_font(run, BODY_FONT_CN, BODY_FONT_EN, 12)
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = ' TOC \\o "1-2" \\h \\z \\u '
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    placeholder = OxmlElement("w:t")
    placeholder.text = "目录将在打开文档时更新（如未更新请按 Ctrl+A 后 F9）"
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    for node in (begin, instr, separate, placeholder, end):
        run._r.append(node)


TOKEN = re.compile(r"(\*\*[^*]+\*\*|`[^`]+`)")


def add_inline(paragraph, text, size=12, base_bold=False):
    for part in TOKEN.split(text):
        if not part:
            continue
        if part.startswith("**") and part.endswith("**") and len(part) > 4:
            run = paragraph.add_run(part[2:-2])
            set_font(run, BODY_FONT_CN, BODY_FONT_EN, size, bold=True)
        elif part.startswith("`") and part.endswith("`") and len(part) > 2:
            run = paragraph.add_run(part[1:-1])
            set_font(run, CODE_FONT, CODE_FONT, size - 1)
        else:
            run = paragraph.add_run(part)
            set_font(run, BODY_FONT_CN, BODY_FONT_EN, size, bold=base_bold)


def add_body_paragraph(doc, text):
    paragraph = doc.add_paragraph()
    add_inline(paragraph, text)
    return paragraph


def add_code_block(doc, lines):
    for index, line in enumerate(lines):
        paragraph = doc.add_paragraph(style="Code Block")
        paragraph.paragraph_format.space_before = Pt(4 if index == 0 else 0)
        paragraph.paragraph_format.space_after = Pt(4 if index == len(lines) - 1 else 0)
        paragraph.paragraph_format.keep_with_next = index < len(lines) - 1
        shade(paragraph, "F2F2F2")
        run = paragraph.add_run(line if line else " ")
        set_font(run, BODY_FONT_CN, CODE_FONT, 8.5)


def add_table(doc, rows):
    columns = max(len(row) for row in rows)
    table = doc.add_table(rows=0, cols=columns)
    table.style = "Table Grid"
    table.alignment = 1  # center
    table.autofit = True

    for row_index, row in enumerate(rows):
        cells = table.add_row().cells
        for column in range(columns):
            text = row[column] if column < len(row) else ""
            cell = cells[column]
            cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
            paragraph = cell.paragraphs[0]
            paragraph.paragraph_format.first_line_indent = Pt(0)
            paragraph.paragraph_format.left_indent = Pt(0)
            paragraph.paragraph_format.line_spacing = 1.0
            paragraph.paragraph_format.space_before = Pt(1)
            paragraph.paragraph_format.space_after = Pt(1)
            paragraph.alignment = (
                WD_ALIGN_PARAGRAPH.CENTER if column == 0 or row_index == 0
                else WD_ALIGN_PARAGRAPH.LEFT
            )
            add_inline(paragraph, text, size=10.5, base_bold=(row_index == 0))
            if row_index == 0:
                shade(cell, "D9D9D9")
        tr = cells[0]._element.getparent()
        tr_pr = tr.get_or_add_trPr()
        cant = OxmlElement("w:cantSplit")
        tr_pr.append(cant)
        if row_index == 0:
            header = OxmlElement("w:tblHeader")
            tr_pr.append(header)
    # 表后留一点间距
    spacer = doc.add_paragraph()
    spacer.paragraph_format.space_after = Pt(0)
    spacer.paragraph_format.space_before = Pt(0)
    spacer.paragraph_format.first_line_indent = Pt(0)
    for run in spacer.runs:
        run.font.size = Pt(6)


def parse_markdown(text):
    blocks = []
    lines = text.splitlines()
    index = 0
    while index < len(lines):
        line = lines[index]
        stripped = line.strip()
        if not stripped:
            index += 1
            continue
        if stripped.startswith("```"):
            language = stripped[3:].strip()
            index += 1
            buffer = []
            while index < len(lines) and not lines[index].strip().startswith("```"):
                buffer.append(lines[index])
                index += 1
            index += 1
            blocks.append(("code", language, buffer))
            continue
        if stripped.startswith("|") and stripped.endswith("|"):
            buffer = []
            while index < len(lines) and lines[index].strip().startswith("|"):
                row = lines[index].strip()
                if not re.match(r"^\|[\s:|-]+\|$", row):
                    cells = [cell.strip() for cell in row.strip("|").split("|")]
                    buffer.append(cells)
                index += 1
            blocks.append(("table", buffer))
            continue
        if stripped.startswith("#"):
            level = len(stripped) - len(stripped.lstrip("#"))
            blocks.append(("heading", level, stripped[level:].strip()))
            index += 1
            continue
        if stripped in ("---", "***"):
            index += 1
            continue
        if stripped.startswith("> "):
            buffer = []
            while index < len(lines) and lines[index].strip().startswith(">"):
                buffer.append(lines[index].strip().lstrip(">").strip())
                index += 1
            blocks.append(("quote", buffer))
            continue
        if re.match(r"^[-*] ", stripped):
            buffer = []
            while index < len(lines) and re.match(r"^[-*] ", lines[index].strip()):
                buffer.append(re.sub(r"^[-*] ", "", lines[index].strip()))
                index += 1
            blocks.append(("bullet", buffer))
            continue
        if re.match(r"^\d+\. ", stripped):
            buffer = []
            while index < len(lines) and re.match(r"^\d+\. ", lines[index].strip()):
                buffer.append(re.sub(r"^\d+\. ", "", lines[index].strip()))
                index += 1
            blocks.append(("number", buffer))
            continue
        buffer = [stripped]
        index += 1
        while index < len(lines):
            nxt = lines[index].strip()
            if (not nxt or nxt.startswith(("#", "|", "```", "> ", "---"))
                    or re.match(r"^[-*] ", nxt) or re.match(r"^\d+\. ", nxt)):
                break
            buffer.append(nxt)
            index += 1
        blocks.append(("paragraph", "".join(buffer)))
    return blocks


def build(md_path: Path, docx_path: Path, with_cover=True, with_toc=True):
    text = md_path.read_text(encoding="utf-8")
    blocks = parse_markdown(text)

    title = "ros_sim 文档"
    subtitle = "C++ 模拟 ROS 节点系统"
    cover_table = None
    start = 0
    if blocks and blocks[0][0] == "heading":
        title = blocks[0][2]
        start = 1
    for offset in range(start, min(start + 3, len(blocks))):
        if blocks[offset][0] == "table" and cover_table is None:
            cover_table = blocks[offset][1]
            start = offset + 1
        elif blocks[offset][0] == "heading":
            break

    doc = Document()
    configure_styles(doc)

    section = doc.sections[0]
    section.page_width = Cm(21)
    section.page_height = Cm(29.7)
    for attr in ("top_margin", "bottom_margin", "left_margin", "right_margin"):
        setattr(section, attr, Cm(2.5))

    # ---------------------------------------------------------------- 封面
    for _ in range(4):
        blank = doc.add_paragraph()
        blank.paragraph_format.space_after = Pt(0)
    title_paragraph = doc.add_paragraph(style="Title")
    title_paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = title_paragraph.add_run(title)
    set_font(run, HEAD_FONT_CN, BODY_FONT_EN, 18, bold=True)

    sub_paragraph = doc.add_paragraph(style="Subtitle")
    sub_paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    sub_run = sub_paragraph.add_run(subtitle)
    set_font(sub_run, "楷体", BODY_FONT_EN, 12)

    if cover_table:
        spacer = doc.add_paragraph()
        spacer.paragraph_format.space_after = Pt(6)
        table = doc.add_table(rows=0, cols=2)
        table.style = "Table Grid"
        table.alignment = 1
        for row in cover_table:
            cells = table.add_row().cells
            for column in range(2):
                cell = cells[column]
                cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
                paragraph = cell.paragraphs[0]
                paragraph.paragraph_format.first_line_indent = Pt(0)
                paragraph.paragraph_format.line_spacing = 1.0
                paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
                add_inline(paragraph, row[column] if column < len(row) else "", size=10.5)

    # ---------------------------------------------------------------- 目录节
    toc_section = doc.add_section(WD_SECTION.NEW_PAGE)
    toc_section.footer.is_linked_to_previous = False
    if with_toc:
        toc_title = doc.add_paragraph()
        toc_title.alignment = WD_ALIGN_PARAGRAPH.CENTER
        toc_title.paragraph_format.space_after = Pt(12)
        toc_run = toc_title.add_run("目　录")
        set_font(toc_run, HEAD_FONT_CN, BODY_FONT_EN, 16, bold=True)
        add_toc(doc)

    # ---------------------------------------------------------------- 正文节
    body_section = doc.add_section(WD_SECTION.NEW_PAGE)
    body_section.footer.is_linked_to_previous = False
    restart_numbering(body_section)
    footer_paragraph = body_section.footer.paragraphs[0]
    footer_paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    add_page_field(footer_paragraph)

    for block in blocks[start:]:
        kind = block[0]
        if kind == "heading":
            level = min(block[1], 3)
            heading = doc.add_paragraph(style=f"Heading {level}")
            heading.paragraph_format.first_line_indent = Pt(0)
            run = heading.add_run(block[2])
            set_font(run, HEAD_FONT_CN, BODY_FONT_EN,
                     18 - 2 * (level - 1) if level < 3 else 12, bold=True)
            heading.paragraph_format.space_before = Pt({1: 14, 2: 11, 3: 9}[level])
            heading.paragraph_format.space_after = Pt({1: 6, 2: 5, 3: 4}[level])
        elif kind == "paragraph":
            add_body_paragraph(doc, block[1])
        elif kind == "code":
            add_code_block(doc, block[2])
        elif kind == "table":
            add_table(doc, block[1])
        elif kind == "quote":
            paragraph = doc.add_paragraph()
            paragraph.paragraph_format.left_indent = Cm(0.5)
            paragraph.paragraph_format.first_line_indent = Pt(0)
            for line in block[1]:
                add_inline(paragraph, line)
                paragraph.add_run("\n")
        elif kind in ("bullet", "number"):
            style_name = "List Bullet" if kind == "bullet" else "List Number"
            for item in block[1]:
                paragraph = doc.add_paragraph(style=style_name)
                paragraph.paragraph_format.first_line_indent = Pt(0)
                paragraph.paragraph_format.line_spacing = 1.5
                add_inline(paragraph, item)

    docx_path.parent.mkdir(parents=True, exist_ok=True)
    doc.save(docx_path)
    return docx_path


def main():
    if len(sys.argv) < 3:
        print("用法: python3 scripts/md2docx.py <输入.md> <输出.docx>")
        return 1
    source = Path(sys.argv[1])
    target = Path(sys.argv[2])
    build(source, target)
    print(f"已生成 {target}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
