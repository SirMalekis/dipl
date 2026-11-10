"""Генерация DOCX «Топологии ER и BA»."""

from __future__ import annotations

from pathlib import Path

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "VKR_Topologii_ER_BA.docx"
FONT = "Times New Roman"


def style_run(run, size=14, bold=False, italic=False):
    run.font.name = FONT
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.italic = italic
    rpr = run._element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    for attr in ("w:ascii", "w:hAnsi", "w:cs"):
        rfonts.set(qn(attr), FONT)


def body_style(document):
    style = document.styles["Normal"]
    style.font.name = FONT
    style.font.size = Pt(14)
    fmt = style.paragraph_format
    fmt.line_spacing = 1.5
    fmt.first_line_indent = Cm(1.25)
    fmt.space_after = Pt(0)
    fmt.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY


def heading(document, text, level=1):
    p = document.add_paragraph()
    p.paragraph_format.first_line_indent = Cm(0)
    p.paragraph_format.space_before = Pt(12 if level == 1 else 8)
    p.paragraph_format.space_after = Pt(6)
    p.paragraph_format.keep_with_next = True
    style_run(p.add_run(text), 16 if level == 1 else 14, True)


def para(document, text, indent=True):
    p = document.add_paragraph()
    p.paragraph_format.first_line_indent = Cm(1.25 if indent else 0)
    style_run(p.add_run(text), 14)


def formula(document, text):
    p = document.add_paragraph()
    p.paragraph_format.first_line_indent = Cm(0)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    style_run(p.add_run(text), 13)


def caption(document, text):
    p = document.add_paragraph()
    p.paragraph_format.first_line_indent = Cm(0)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    style_run(p.add_run(text), 12, italic=True)


def shade(cell, color):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), color)
    tc_pr.append(shd)


def table(document, headers, rows, widths):
    table_obj = document.add_table(rows=1, cols=len(headers))
    table_obj.style = "Table Grid"
    table_obj.alignment = WD_TABLE_ALIGNMENT.CENTER
    table_obj.autofit = False

    header = table_obj.rows[0].cells
    for i, text in enumerate(headers):
        header[i].text = ""
        p = header[i].paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.first_line_indent = Cm(0)
        style_run(p.add_run(text), 10, True)
        shade(header[i], "E7E6E6")

    for row in rows:
        cells = table_obj.add_row().cells
        for i, text in enumerate(row):
            cells[i].text = ""
            p = cells[i].paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            p.paragraph_format.first_line_indent = Cm(0)
            style_run(p.add_run(text), 9)

    tbl_pr = table_obj._tbl.tblPr
    layout = OxmlElement("w:tblLayout")
    layout.set(qn("w:type"), "fixed")
    tbl_pr.append(layout)
    for i, width in enumerate(widths):
        table_obj.columns[i].width = Cm(width)
        for cell in table_obj.columns[i].cells:
            cell.width = Cm(width)


def bullet(document, text):
    p = document.add_paragraph(style="List Bullet")
    p.paragraph_format.first_line_indent = Cm(0)
    p.paragraph_format.left_indent = Cm(1.0)
    style_run(p.add_run(text), 14)


def main():
    document = Document()
    body_style(document)
    section = document.sections[0]
    section.page_width = Cm(21.0)
    section.page_height = Cm(29.7)
    section.left_margin = Cm(3.0)
    section.right_margin = Cm(1.5)
    section.top_margin = Cm(2.0)
    section.bottom_margin = Cm(2.0)

    # Титул
    for _ in range(4):
        document.add_paragraph()
    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    style_run(p.add_run("МОДЕЛИ СЛУЧАЙНОГО И БЕЗМАСШТАБНОГО ГРАФА\n"
                        "В ОЦЕНКЕ ЖИВУЧЕСТИ ИНФОРМАЦИОННЫХ СЕТЕЙ"), 16, True)
    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    style_run(p.add_run("Эрдёш–Реньи и Барабаши–Альберт:\n"
                        "назначение, генерация, структурный анализ"), 14)
    for _ in range(10):
        document.add_paragraph()
    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    style_run(p.add_run("2026"), 13)
    document.add_page_break()

    heading(document, "ВВЕДЕНИЕ", 1)
    para(document,
         "Топология сети определяет, как отказ одного узла влияет на "
         "связность всей инфраструктуры. Детерминированные топологии "
         "(звезда, кольцо, полносвязная сеть) задают крайние случаи "
         "централизации и избыточности. Для исследования реальных сетей "
         "используются вероятностные модели: случайный граф Эрдёша–Реньи "
         "(ER) и безмасштабная сеть Барабаши–Альберта (BA).")
    para(document,
         "ER — однородная случайная сеть без выраженных хабов, удобная как "
         "нулевая гипотеза. BA воспроизводит предпочтительное присоединение "
         "и степенное распределение степеней, характерное для многих "
         "реальных инфраструктур. В работе обе модели поддерживаются во всех "
         "сценариях, а их структурные характеристики рассчитываются "
         "автоматически.")

    heading(document, "1 МОДЕЛЬ ЭРДЁША–РЕНЬИ G(N, p)", 1)
    para(document,
         "Каждая из N(N−1)/2 возможных пар узлов соединяется ребром "
         "независимо с вероятностью p. Математическое ожидание числа рёбер "
         "и средняя степень:")
    formula(document, "E[E] = p · N(N−1)/2,   ⟨k⟩ = p · (N−1)")
    para(document,
         "Степень вершины имеет биномиальное распределение Bin(N−1, p); при "
         "больших N и фиксированном ⟨k⟩ оно приближается распределением "
         "Пуассона. Коэффициент кластеризации близок к p. Порог появления "
         "гигантской компоненты p_c ≈ 1/(N−1); генератор дополнительно "
         "обеспечивает связность, добавляя рёбра остовного дерева.")
    para(document,
         "Назначение: базовая модель однородной случайной сети и нулевая "
         "гипотеза для сравнения. Ограничения: нет хабов, узкое "
         "распределение степеней, целевая атака почти не отличается от "
         "случайной, не воспроизводятся сообщества и иерархия.")

    heading(document, "2 МОДЕЛЬ БАРАБАШИ–АЛЬБЕРТА", 1)
    para(document,
         "Сеть строится инкрементально. Начальное ядро из m0 = max(m+1, 3) "
         "узлов соединяется полносвязно. Каждый новый узел присоединяется к "
         "m различным существующим узлам с вероятностью, пропорциональной "
         "их степени:")
    formula(document, "Π(i) = k_i / Σ_j k_j")
    para(document,
         "Это механизм предпочтительного присоединения: «богатые становятся "
         "ещё богаче», появляются хабы. Для больших N распределение "
         "степеней имеет степенной вид P(k) ~ k^(−γ) с γ ≈ 3, а средняя "
         "степень ⟨k⟩ ≈ 2m.")
    para(document,
         "Следствия: хабы с k_max, значительно превышающим ⟨k⟩; короткие "
         "пути (диаметр O(log N / log log N)); дисассортативность (хабы "
         "соединяются с низкостепенными узлами); повышенная уязвимость к "
         "целевым атакам при устойчивости к случайным отказам.")
    para(document,
         "Назначение: модель реальных сетей с центрами — магистральные "
         "маршрутизаторы, контроллеры домена, серверы баз данных. "
         "Ограничения: фиксированный γ ≈ 3, нет старения и fitness, не "
         "воспроизводятся сообщества; при m = 2 сеть особенно хрупка.")

    heading(document, "3 СТРУКТУРНЫЕ МЕТРИКИ", 1)
    para(document,
         "Для сравнения топологий вычисляются: число узлов и рёбер, средняя, "
         "минимальная и максимальная степень, коэффициент вариации, "
         "кластеризация, средняя длина пути, диаметр, ассортативность, число "
         "компонент, доля LCC, ожидаемая средняя степень, наклон CCDF и "
         "оценка показателя γ методом Хилла.")
    formula(document, "C = Σ_u (число замкнутых пар соседей u) / "
                      "Σ_u k_u(k_u−1)/2")
    formula(document, "r = [Σ_e j_e k_e/E − ((Σ_e(j_e+k_e))/(2E))²] / "
                      "[Σ_e(j_e²+k_e²)/(2E) − ((Σ_e(j_e+k_e))/(2E))²]")
    para(document,
         "Оценка Хилла γ = 1 + n_tail / Σ ln(k_i/1,5) для k_i ≥ 2 устойчивее "
         "линейной регрессии CCDF на малых выборках. Для BA при N = 50, "
         "m = 2 она даёт γ ≈ 3,2.")

    heading(document, "4 РЕЗУЛЬТАТЫ ДЛЯ N = 50", 1)
    caption(document, "Таблица 1 — Структурные метрики топологий")
    table(document,
          ["Топология", "E", "⟨k⟩", "k_max", "CV", "C", "L", "D", "r", "γ"],
          [
              ["star", "49", "1,96", "49", "3,46", "0,000", "1,96", "2", "−1,000", "—"],
              ["ring", "49", "1,96", "2", "0,10", "0,000", "17,00", "49", "−0,021", "—"],
              ["full_mesh", "1225", "49,00", "49", "0,00", "1,000", "1,00", "1", "0,000", "—"],
              ["erdos_renyi", "101", "4,04", "8", "0,49", "0,097", "2,82", "5", "−0,290", "1,99"],
              ["barabasi_albert", "97", "3,88", "37", "1,94", "0,086", "1,92", "2", "−0,928", "3,19"],
          ],
          widths=[2.7, 1.4, 1.4, 1.5, 1.3, 1.4, 1.3, 1.1, 1.6, 1.3])
    para(document,
         "При близкой средней степени ER и BA различаются принципиально: "
         "k_max 8 против 37, CV 0,49 против 1,94, L 2,82 против 1,92, "
         "r −0,29 против −0,93, γ 1,99 против 3,19. BA воспроизводит "
         "безмасштабную структуру с хабами, ER — однородный случайный граф.")

    heading(document, "5 ВЛИЯНИЕ НА ЖИВУЧЕСТЬ", 1)
    caption(document, "Таблица 2 — Базовый сценарий (N = 50, λ = 1,0, R = 200)")
    table(document,
          ["Топология", "E_QoS", "Q", "LCC", "φ_w"],
          [
              ["erdos_renyi", "0,406", "0,477", "0,625", "0,425"],
              ["barabasi_albert", "0,114", "0,122", "0,187", "0,213"],
          ],
          widths=[4.0, 2.4, 2.4, 2.4, 2.4])
    para(document,
         "ER устойчивее BA при целевых атаках: удаление нескольких хабов "
         "фрагментирует BA, тогда как в ER все узлы статистически "
         "равноправны. При этом BA компактнее и может дольше сохранять "
         "связность при случайных отказах; результат зависит от m, числа "
         "бригад и ресурсов. Поэтому исследуются оба класса сетей.")

    heading(document, "6 ИСПОЛЬЗОВАНИЕ В ПРОГРАММЕ", 1)
    para(document,
         "В GUI топологии выбираются флажками; параметры p и m задаются в "
         "секции генератора. Вкладка «Структура» показывает таблицу метрик и "
         "CCDF распределения степеней. В CLI доступен отдельный сценарий "
         "structure, который не запускает симуляцию, а только анализирует "
         "графы. Результаты сохраняются в topology_structure.csv и "
         "topology_degrees_<topology>.csv.")
    formula(document, "dipl.exe run --scenario structure --topologies "
                      "erdos_renyi,barabasi_albert --num-nodes 100 "
                      "--er-p 0.05 --ba-m 2 --out results\\structure")

    heading(document, "7 ВЫВОДЫ", 1)
    for item in [
        "ER и BA — две разные гипотезы о структуре сети: однородная "
        "случайность и предпочтительное присоединение.",
        "Структурный анализ подтверждает различие: BA имеет хабы (k_max 37 "
        "против 8), высокую неоднородность (CV 1,94 против 0,49), короткие "
        "пути и степенной хвост (γ ≈ 3,2).",
        "Различие влияет на живучесть: ER устойчивее к целевым атакам, BA "
        "компактнее, но хрупок при потере хабов.",
        "Обе модели поддержаны во всех сценариях, GUI и CLI; структурный "
        "анализ экспортируется автоматически и доступен на отдельной "
        "вкладке.",
    ]:
        bullet(document, item)

    heading(document, "ЛИТЕРАТУРА", 1)
    for i, ref in enumerate([
        "Эрдёш П., Реньи А. On the evolution of random graphs. — 1960.",
        "Барабаши А.-Л., Альберт Р. Emergence of scaling in random networks // Science. 1999.",
        "Ньюман М. Сети: введение. — М.: Вильямс, 2019.",
        "Олифер В.Г., Олифер Н.А. Компьютерные сети. — СПб.: Питер, 2020.",
    ], 1):
        p = document.add_paragraph()
        p.paragraph_format.first_line_indent = Cm(0)
        style_run(p.add_run(f"{i}. {ref}"), 12)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    document.save(OUT)
    print("SAVED", OUT)


if __name__ == "__main__":
    main()
