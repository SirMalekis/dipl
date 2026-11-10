"""Генерация DOCX «Этап 1. Теоретическое обоснование» для ВКР."""

from __future__ import annotations

from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_TAB_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "VKR_Etap1_Teoreticheskoe_obosnovanie.docx"

FONT = "Times New Roman"


def set_run_font(run, size: int = 14, bold: bool = False,
                 italic: bool = False, color: RGBColor | None = None) -> None:
    run.font.name = FONT
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.italic = italic
    if color is not None:
        run.font.color.rgb = color
    rpr = run._element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    for attr in ("w:ascii", "w:hAnsi", "w:cs"):
        rfonts.set(qn(attr), FONT)


def body_style(document: Document) -> None:
    style = document.styles["Normal"]
    style.font.name = FONT
    style.font.size = Pt(14)
    rpr = style.element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    for attr in ("w:ascii", "w:hAnsi", "w:cs"):
        rfonts.set(qn(attr), FONT)
    fmt = style.paragraph_format
    fmt.line_spacing = 1.5
    fmt.first_line_indent = Cm(1.25)
    fmt.space_after = Pt(0)
    fmt.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY


def heading(document: Document, text: str, level: int = 1) -> None:
    paragraph = document.add_paragraph()
    paragraph.paragraph_format.first_line_indent = Cm(0)
    paragraph.paragraph_format.space_before = Pt(12 if level == 1 else 8)
    paragraph.paragraph_format.space_after = Pt(6)
    paragraph.paragraph_format.keep_with_next = True
    paragraph.alignment = WD_ALIGN_PARAGRAPH.LEFT
    run = paragraph.add_run(text)
    set_run_font(run, 16 if level == 1 else 14, bold=True, color=RGBColor(0, 0, 0))


def para(document: Document, text: str, first_indent: bool = True) -> None:
    paragraph = document.add_paragraph()
    paragraph.paragraph_format.first_line_indent = Cm(1.25 if first_indent else 0)
    paragraph.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    run = paragraph.add_run(text)
    set_run_font(run, 14)


def formula(document: Document, text: str, number: str | None = None) -> None:
    paragraph = document.add_paragraph()
    paragraph.paragraph_format.first_line_indent = Cm(0)
    paragraph.paragraph_format.space_before = Pt(4)
    paragraph.paragraph_format.space_after = Pt(4)
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.paragraph_format.tab_stops.add_tab_stop(
        Cm(15.5), WD_TAB_ALIGNMENT.RIGHT)
    run = paragraph.add_run(text)
    set_run_font(run, 13, italic=False)
    if number:
        paragraph.add_run("\t")
        number_run = paragraph.add_run(f"({number})")
        set_run_font(number_run, 13)


def caption(document: Document, text: str) -> None:
    paragraph = document.add_paragraph()
    paragraph.paragraph_format.first_line_indent = Cm(0)
    paragraph.paragraph_format.space_before = Pt(2)
    paragraph.paragraph_format.space_after = Pt(6)
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = paragraph.add_run(text)
    set_run_font(run, 12, italic=True)


def shade(cell, color: str) -> None:
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), color)
    tc_pr.append(shd)


def table(document: Document, headers: list[str], rows: list[list[str]],
          widths: list[float] | None = None) -> None:
    table_obj = document.add_table(rows=1, cols=len(headers))
    table_obj.style = "Table Grid"
    table_obj.alignment = WD_TABLE_ALIGNMENT.CENTER
    table_obj.autofit = False

    header = table_obj.rows[0].cells
    for index, text in enumerate(headers):
        header[index].text = ""
        paragraph = header[index].paragraphs[0]
        paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
        paragraph.paragraph_format.first_line_indent = Cm(0)
        run = paragraph.add_run(text)
        set_run_font(run, 11, bold=True)
        shade(header[index], "E7E6E6")

    for row in rows:
        cells = table_obj.add_row().cells
        for index, text in enumerate(row):
            cells[index].text = ""
            paragraph = cells[index].paragraphs[0]
            paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
            paragraph.paragraph_format.first_line_indent = Cm(0)
            run = paragraph.add_run(text)
            set_run_font(run, 10)

    if widths:
        table_obj.autofit = False
        tbl_pr = table_obj._tbl.tblPr
        layout = OxmlElement("w:tblLayout")
        layout.set(qn("w:type"), "fixed")
        tbl_pr.append(layout)
        tbl_w = OxmlElement("w:tblW")
        tbl_w.set(qn("w:type"), "dxa")
        tbl_w.set(qn("w:w"), str(int(sum(widths) * 567)))
        tbl_pr.append(tbl_w)
        for index, width in enumerate(widths):
            table_obj.columns[index].width = Cm(width)
            for cell in table_obj.columns[index].cells:
                cell.width = Cm(width)
    document.add_paragraph().paragraph_format.space_after = Pt(0)


def bullet(document: Document, text: str) -> None:
    paragraph = document.add_paragraph(style="List Bullet")
    paragraph.paragraph_format.first_line_indent = Cm(0)
    paragraph.paragraph_format.left_indent = Cm(1.0)
    run = paragraph.add_run(text)
    set_run_font(run, 14)


def add_page_number(document: Document) -> None:
    section = document.sections[0]
    footer = section.footer
    paragraph = footer.paragraphs[0]
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = paragraph.add_run()
    fld = OxmlElement("w:fldSimple")
    fld.set(qn("w:instr"), "PAGE")
    run._r.addnext(fld)
    set_run_font(run, 11)


def main() -> None:
    document = Document()
    body_style(document)

    section = document.sections[0]
    section.page_width = Cm(21.0)
    section.page_height = Cm(29.7)
    section.left_margin = Cm(3.0)
    section.right_margin = Cm(1.5)
    section.top_margin = Cm(2.0)
    section.bottom_margin = Cm(2.0)
    section.different_first_page_header_footer = True

    # ------------------------------------------------------------------
    # Титульный лист
    # ------------------------------------------------------------------
    for _ in range(3):
        document.add_paragraph()
    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    set_run_font(p.add_run(
        "МИНИСТЕРСТВО НАУКИ И ВЫСШЕГО ОБРАЗОВАНИЯ\n"
        "РОССИЙСКОЙ ФЕДЕРАЦИИ"), 13, bold=True)

    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    set_run_font(p.add_run(
        "Томский государственный университет систем управления\n"
        "и радиоэлектроники (ТУСУР)"), 13)

    for _ in range(3):
        document.add_paragraph()

    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    set_run_font(p.add_run("ВЫПУСКНАЯ КВАЛИФИКАЦИОННАЯ РАБОТА"), 16, bold=True)

    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    set_run_font(p.add_run(
        "Теоретическое обоснование и математическое моделирование\n"
        "оценки живучести гетерогенной информационной инфраструктуры\n"
        "с учётом требований качества обслуживания IP-сетей"), 15, bold=True)

    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    set_run_font(p.add_run("Этап 1"), 14, italic=True)

    for _ in range(8):
        document.add_paragraph()

    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.first_line_indent = Cm(0)
    set_run_font(p.add_run("2026"), 13)

    document.add_page_break()

    # ------------------------------------------------------------------
    # Введение
    # ------------------------------------------------------------------
    heading(document, "ВВЕДЕНИЕ", 1)
    para(document,
         "Настоящий этап выпускной квалификационной работы развивает "
         "математическую и программную модель, созданную в ходе "
         "научно-исследовательской работы (НИР). В НИР были формализованы "
         "взвешенная живучесть φ_w, топологически-ориентированное "
         "профилирование узлов, дискретно-событийная модель атак и "
         "алгоритмы восстановления FIFO/Triage. Однако оценка качества "
         "обслуживания в НИР носила преимущественно топологический "
         "характер: задержка и потери не проверялись по нормируемым "
         "порогам, а пропускная способность каналов фактически не "
         "ограничивала потоки.")
    para(document,
         "Для перехода от абстрактных графовых метрик к физически "
         "интерпретируемым показателям необходимо решить три задачи: "
         "проанализировать требования МСЭ-Т Y.1541 к задержке (IPTD) и "
         "потерям (IPLR); построить модель канала, связывающую пропускную "
         "способность, загрузку и качество; адаптировать нормированную "
         "глобальную эффективность так, чтобы она учитывала только пути, "
         "выполняющие соглашение об уровне обслуживания (SLA).")
    para(document,
         "Объект исследования — гетерогенная информационная "
         "инфраструктура в условиях деструктивных воздействий и "
         "ограниченных ресурсов восстановления. Предмет — закономерности "
         "изменения показателей живучести и качества обслуживания в "
         "зависимости от топологии, вектора атаки, нагрузки и стратегии "
         "восстановления.")

    # ------------------------------------------------------------------
    # 1. Y.1541
    # ------------------------------------------------------------------
    heading(document, "1 АНАЛИЗ ТРЕБОВАНИЙ МСЭ-Т Y.1541", 1)
    heading(document, "1.1 Классы сетевого QoS", 2)
    para(document,
         "Рекомендация МСЭ-Т Y.1541 (02/2006) устанавливает классы "
         "качества обслуживания для сетей с протоколом IP. Требования "
         "задаются для интерфейса UNI–UNI — между точками измерения на "
         "границах сети. Классы 0–5 определены таблицей 1, классы 6–7 — "
         "таблицей 3 и являются условными: их значения могут "
         "корректироваться по фактическому опыту эксплуатации.")
    caption(document, "Таблица 1 — Классы QoS и требования Y.1541")
    table(document,
          ["Класс", "IPTD", "IPDV", "IPLR", "IPER", "Назначение"],
          [
              ["0", "100 мс", "50 мс", "1·10⁻³", "1·10⁻⁴", "VoIP, видеоконференции"],
              ["1", "400 мс", "50 мс", "1·10⁻³", "1·10⁻⁴", "интерактивные приложения"],
              ["2", "100 мс", "—", "1·10⁻³", "1·10⁻⁴", "сигнализация, транзакции"],
              ["3", "400 мс", "—", "1·10⁻³", "1·10⁻⁴", "транзакционные данные"],
              ["4", "1 с", "—", "1·10⁻³", "1·10⁻⁴", "массовая передача данных"],
              ["5", "—", "—", "—", "—", "best effort"],
              ["6", "100 мс", "50 мс", "1·10⁻⁵", "1·10⁻⁶", "условный, цифровое ТВ"],
              ["7", "400 мс", "50 мс", "1·10⁻⁵", "1·10⁻⁶", "условный, цифровое ТВ"],
          ],
          widths=[1.6, 1.7, 1.6, 1.8, 1.8, 7.0])
    para(document,
         "Символ «—» означает, что требование не определено. В модели "
         "используются проверки IPTD, IPLR и IPDV; IPER и процент "
         "переупорядочивания IPRR не рассчитываются, поскольку каналы "
         "предполагаются без битовых ошибок и переупорядочивания.")

    heading(document, "1.2 Смысл показателей и правила агрегирования", 2)
    para(document,
         "IPTD — средняя задержка передачи пакета, усреднённая по потоку. "
         "Требование является верхней границей среднего значения, а не "
         "максимальной задержки отдельного пакета. IPLR — доля потерянных "
         "пакетов; требование ограничивает вероятность потери. IPDV "
         "определяется как разность квантиля 1−10⁻³ распределения IPTD и "
         "минимального значения IPTD на интервале оценки.")
    para(document,
         "Для пути, состоящего из последовательности звеньев, задержки "
         "складываются, а вероятности потерь агрегируются через "
         "вероятность успешной передачи:")
    formula(document, "IPTDпуть = Σ IPTD_e", "1")
    formula(document, "IPLRпуть = 1 − Π (1 − IPLR_e)", "2")
    formula(document, "IPERпуть = 1 − Π (1 − IPER_e)", "3")
    formula(document, "IPDVпуть ≈ √(Σ IPDV_e²)", "4")
    para(document,
         "Формула (4) является инженерным приближением для независимых "
         "звеньев; точное агрегирование IPDV требует свёртки "
         "распределений. Путь удовлетворяет классу K, если одновременно "
         "выполняются все определённые для класса неравенства:")
    formula(document, "IPTDпуть ≤ IPTD_K,  IPLRпуть ≤ IPLR_K,  IPDVпуть ≤ IPDV_K", "5")

    # ------------------------------------------------------------------
    # 2. M/M/1
    # ------------------------------------------------------------------
    heading(document, "2 МАТЕМАТИЧЕСКАЯ МОДЕЛЬ КАНАЛА M/M/1", 1)
    heading(document, "2.1 Допущения и основные соотношения", 2)
    para(document,
         "Каждый направленный канал рассматривается как одноканальная "
         "система массового обслуживания M/M/1 с ограниченным буфером K. "
         "Заявки (пакеты) поступают пуассоновским потоком, время "
         "обслуживания распределено экспоненциально. Пуассоновское "
         "приближение оправдано агрегированием большого числа независимых "
         "источников, а экспоненциальное обслуживание даёт замкнутые "
         "формулы и консервативные оценки.")
    para(document,
         "Пусть C — пропускная способность канала (бит/с), L — размер "
         "пакета (бит), λ_b — загрузка (бит/с). Интенсивность обслуживания "
         "μ, интенсивность поступления λ и коэффициент загрузки ρ равны:")
    formula(document, "μ = C / L,   λ = λ_b / L,   ρ = λ / μ = λ_b / C", "6")
    para(document, "Условие устойчивости очереди: ρ < 1. Среднее время "
                   "пребывания пакета в системе и средняя задержка звена:")
    formula(document, "T = 1 / (μ − λ)", "7")
    formula(document, "IPTD_e = d_0 + 1 / (μ − λ) = d_0 + (1/μ) / (1 − ρ)", "8")
    para(document,
         "где d_0 — базовая задержка распространения и обработки, с. "
         "Средняя длина очереди и среднее время ожидания равны "
         "N_q = ρ²/(1−ρ) и W_q = ρ/(μ(1−ρ)) соответственно.")

    heading(document, "2.2 Потери в M/M/1/K", 2)
    para(document,
         "Вероятность того, что прибывший пакет застанет систему "
         "заполненной, определяется формулой Эрланга для M/M/1/K:")
    formula(document, "IPLR_e = (1 − ρ) ρ^K / (1 − ρ^(K+1)),  ρ ≠ 1", "9")
    formula(document, "IPLR_e = 1 / (K + 1),  ρ = 1", "10")
    para(document,
         "Формула (9) показывает, что потери определяются совместно "
         "загрузкой и размером буфера. При ρ = 0,9 и K = 20 потери "
         "составляют 1,37 %, а при K = 100 — 2,66·10⁻⁶. Следовательно, "
         "один и тот же канал может проходить или не проходить класс QoS "
         "в зависимости от выделенного буфера — это важный вывод для "
         "проектирования инфраструктуры.")

    heading(document, "2.3 Изменение задержки IPDV", 2)
    para(document,
         "Для экспоненциального распределения времени пребывания в M/M/1 "
         "квантиль уровня 1−10⁻³ равен t_p = −ln(10⁻³)/(μ−λ). Поскольку "
         "минимальная задержка в модели пренебрежимо мала по сравнению с "
         "квантилем, оценка IPDV звена имеет вид:")
    formula(document, "IPDV_e ≈ 6,9078 / (μ − λ)", "11")
    para(document,
         "При ρ → 1 величина (11) неограниченно растёт, что согласуется с "
         "физикой перегрузки. На пути из нескольких звеньев IPDV "
         "агрегируется по формуле (4).")

    document.add_page_break()
    heading(document, "2.4 Численный пример", 2)
    para(document,
         "Для C = 100 Мбит/с, L = 12 000 бит, μ = 8333,33 пакета/с, "
         "1/μ = 0,12 мс, K = 20 получены следующие значения:")
    caption(document, "Таблица 2 — Характеристики звена M/M/1 при K = 20")
    table(document,
          ["ρ", "IPTD_e − d_0, мс", "IPLR_e", "IPDV_e, мс"],
          [
              ["0,30", "0,171", "2,44·10⁻¹¹", "1,18"],
              ["0,50", "0,240", "4,77·10⁻⁷", "1,66"],
              ["0,70", "0,400", "2,40·10⁻⁴", "2,76"],
              ["0,80", "0,600", "2,33·10⁻³", "4,15"],
              ["0,90", "1,200", "1,37·10⁻²", "8,29"],
              ["0,95", "2,400", "2,72·10⁻²", "16,58"],
              ["0,98", "6,000", "3,86·10⁻²", "41,45"],
              ["0,99", "12,000", "4,30·10⁻²", "82,89"],
          ],
          widths=[2.0, 4.5, 4.5, 4.5])
    para(document,
         "Пусть путь состоит из пяти таких звеньев с d_0 = 2 мс и ρ = 0,7. "
         "Тогда IPTDпуть = 5·(2 + 0,4) = 12 мс; IPLRпуть = 1 − "
         "(1 − 2,395·10⁻⁴)^5 ≈ 1,196·10⁻³; IPDVпуть ≈ √5·2,763 ≈ 6,18 мс. "
         "Путь проходит по задержке и изменению задержки, но формально не "
         "проходит класс 1 по потерям: 1,196·10⁻³ > 1·10⁻³. Пример "
         "показывает, что накопление потерь по пути является "
         "самостоятельным ограничением, которое нельзя заменить проверкой "
         "средней задержки.")

    # ------------------------------------------------------------------
    # 3. Метрики
    # ------------------------------------------------------------------
    heading(document, "3 АДАПТАЦИЯ ГЛОБАЛЬНОЙ ЭФФЕКТИВНОСТИ", 1)
    heading(document, "3.1 Физическая эффективность", 2)
    para(document,
         "В теории сложных сетей глобальная эффективность определяется "
         "через среднее обратных кратчайших расстояний:")
    formula(document, "E_glob(t) = 1/(N(N−1)) · Σ_{i≠j} 1 / d_ij(t)", "12")
    para(document,
         "В разрабатываемой модели расстояние заменяется физической "
         "задержкой пути IPTD_ij(t), рассчитанной по формулам (1) и (8) на "
         "текущих загрузках каналов:")
    formula(document, "E_phys(t) = 1/(N(N−1)) · Σ_{i≠j} 1 / IPTD_ij(t)", "13")
    para(document,
         "Метрика (13) имеет размерность 1/с, поэтому используется "
         "нормированная величина E_phys_norm(t) = min(1, E_phys(t)/E_phys(0)). "
         "Ограничение сверху единицей отражает смысл индекса сохранённого "
         "потенциала: деградировавшая сеть не может быть «лучше» исходной. "
         "Без ограничения при сильной перегрузке базовой сети E_phys(0) "
         "мала, и после отказов отношение может превысить единицу.")

    heading(document, "3.2 QoS-эффективность", 2)
    para(document,
         "Главная метрика этапа — нормированная глобальная эффективность с "
         "фильтром требований Y.1541:")
    formula(document, "E_QoS(t) = 1/(N(N−1)) · Σ_{i≠j} [1/IPTD_ij(t)] · I_ij(t)", "14")
    formula(document, "E_QoS_norm(t) = min(1, E_QoS(t) / E_QoS(0))", "15")
    para(document,
         "где I_ij(t) = 1, если путь i→j достижим и удовлетворяет (5) для "
         "выбранного класса QoS, иначе 0. Смысл (14) состоит в том, что "
         "пара, не выполнившая SLA, не вносит вклад в коммуникационный "
         "потенциал, даже если топологически путь существует.")
    para(document,
         "Дополнительно вычисляются доля пар с выполненным QoS "
         "Q(t) = Σ I_ij(t)/(N(N−1)), топологическая достижимость R(t) и "
         "доля QoS среди достижимых Q|R(t) = Q(t)/R(t). Разделение Q, R и "
         "Q|R позволяет различить два механизма деградации: фрагментацию "
         "сети (падает R) и перегрузку/потери (падает Q|R при высоком R).")

    heading(document, "3.3 Взвешенная живучесть", 2)
    para(document,
         "Для учёта бизнес-ценности узлов используется метрика НИР:")
    formula(document, "LCC(t) = |V_LCC(t)| / N", "16")
    formula(document, "φ_w(t) = Σ_{i∈V_LCC(t)} w_i / Σ_{i∈V} w_i", "17")
    para(document,
         "где V_LCC(t) — наибольшая связная компонента подграфа "
         "работоспособных узлов. В отличие от прежней реализации, φ_w "
         "рассчитывается именно по LCC, как определено формулой (2.3) НИР. "
         "Это исключает ситуацию, когда изолированные, но работающие узлы "
         "дают высокую «живучесть».")

    # ------------------------------------------------------------------
    # 4. Трафик
    # ------------------------------------------------------------------
    heading(document, "4 МОДЕЛЬ ТРАФИКА И ЗАТОРОВ", 1)
    para(document,
         "Базовая интенсивность пары «источник–получатель» равна b бит/с; "
         "суммарный спрос B_total = N(N−1)b. При отказе части узлов "
         "используется режим сохранения суммарного трафика: при A активных "
         "узлах интенсивность пары пересчитывается как")
    formula(document, "b_A = b · N(N−1) / (A(A−1)),  A ≥ 2", "18")
    para(document,
         "Это моделирует перераспределение трафика выбывших узлов на "
         "оставшиеся — именно механизм возникновения заторов при отказах. "
         "На каждой итерации k для каждой дуги вычисляется стоимость "
         "c_e = IPTD_e(λ_e) + h, где h — плата за транзит (по умолчанию "
         "5 мс), подавляющая нефизичные обходные пути при насыщении. Затем "
         "для каждого активного источника запускается алгоритм Дейкстры, а "
         "нагрузки пересчитываются методом последовательных средних:")
    formula(document, "λ_e^(k+1) = (1 − α_k) λ_e^(k) + α_k λ̃_e,  α_k = 1/(k+1)", "19")
    para(document,
         "Плата за транзит добавляется только к стоимости выбора пути; "
         "фактическая IPTD пути считается суммой IPTD_e без h, поэтому "
         "метрики (13)–(15) остаются физическими. Итерации повторяются "
         "3–10 раз; сложность полного пересчёта составляет "
         "O(N(V+E)log V) на итерацию.")

    # ------------------------------------------------------------------
    # 5. DES
    # ------------------------------------------------------------------
    heading(document, "5 ДИСКРЕТНО-СОБЫТИЙНАЯ МОДЕЛЬ АТАК И ВОССТАНОВЛЕНИЯ", 1)
    para(document,
         "Атаки образуют пуассоновский поток с интенсивностью λ_a; время "
         "до следующей атаки распределено экспоненциально. Вектор атаки "
         "targeted выбирает узел с максимальной степенью, random — "
         "равновероятно, cascading — среди соседей уже отказавших узлов. "
         "Атака успешна с вероятностью P_attack узла.")
    para(document,
         "Отказавший узел помещается в очередь. Пул из repair_slots бригад "
         "обслуживает очередь: диспетчер FIFO выбирает узел по времени "
         "отказа, Triage — по максимальному весу w_i. Если ресурсов R "
         "хватает на стоимость C_repair, они списываются, бригада "
         "занимается ремонтом в течение Exp(μ_i); по завершении узел "
         "восстанавливается с вероятностью P_recovery, иначе возвращается "
         "в очередь. Triage стремится максимизировать φ_w, но расходует "
         "ресурсы быстрее; FIFO «слеп» к ценности узлов.")
    para(document,
         "Метрики фиксируются на равномерной сетке t_i = iT/(S−1). События "
         "обрабатываются в порядке времени; при изменении состояния узлов "
         "пересчитываются загрузки и матрица путей. Между событиями "
         "метрики не меняются, поэтому пересчёт выполняется только при "
         "изменении состояния — это существенно ускоряет Монте-Карло.")

    # ------------------------------------------------------------------
    # 6. Статистика
    # ------------------------------------------------------------------
    heading(document, "6 СТАТИСТИЧЕСКАЯ ОБРАБОТКА", 1)
    para(document,
         "Для M независимых прогонов вычисляется среднее и 95 % "
         "доверительный интервал по t-распределению Стьюдента:")
    formula(document, "CI = t_{0,975, M−1} · s / √M", "20")
    para(document,
         "Различия стратегий проверяются непараметрическим U-критерием "
         "Манна–Уитни; при множественных сравнениях применяется поправка "
         "Холма p_holm[k] = min(p[k](m−k), 1) с обеспечением монотонности. "
         "Гипотеза отвергается при p_holm < 0,05. Воспроизводимость "
         "обеспечивается детерминированным генератором std::mt19937_64 с "
         "сидом, зависящим от номера прогона.")

    # ------------------------------------------------------------------
    # 7. Выводы
    # ------------------------------------------------------------------
    heading(document, "7 ВЫВОДЫ ПО ЭТАПУ", 1)
    bullets = [
        "Проанализирована рекомендация МСЭ-Т Y.1541; формализованы классы "
        "0–7 и пороги IPTD, IPDV, IPLR, IPER. Для модели выбраны проверки "
        "IPTD, IPLR и IPDV; IPER и IPRR отнесены к не моделируемым факторам.",
        "Получена замкнутая связь физических параметров канала с задержкой "
        "и потерями на основе M/M/1(K). Показано, что буфер и загрузка "
        "совместно определяют выполнение IPLR: при ρ = 0,9 и K = 20 потери "
        "составляют 1,37 %, а при K = 100 — 2,66·10⁻⁶.",
        "Классическая глобальная эффективность адаптирована к физическим "
        "задержкам; введена QoS-эффективность E_QoS, учитывающая только "
        "пути, прошедшие SLA, и раздельные показатели R, Q и Q|R.",
        "Разработан итеративный алгоритм All-to-All с Дейкстрой и MSA, "
        "моделирующий заторы при перераспределении трафика; введена плата "
        "за транзит для стабилизации маршрутизации.",
        "Построена дискретно-событийная модель атак и восстановления с "
        "FIFO/Triage, позволяющая исследовать компромисс «взвешенная "
        "живучесть — расход ресурсов».",
        "Все соотношения реализованы в C++-ядре и проверены 65 модульными "
        "проверками, а также численными экспериментами.",
    ]
    for item in bullets:
        bullet(document, item)

    heading(document, "ЛИТЕРАТУРА", 1)
    references = [
        "Рекомендация МСЭ-Т Y.1541 (02/2006). Требования к сетевым "
        "показателям качества для служб, основанных на протоколе IP.",
        "Рекомендация МСЭ-Т Y.1540. Показатели качества передачи пакетов IP.",
        "Клейнрок Л. Теория массового обслуживания. — М.: Машиностроение, 1979.",
        "Ньюман М. Сети: введение. — М.: Вильямс, 2019.",
        "Барабаши А.-Л. Связи: сетевые структуры. — М.: Эксмо, 2010.",
        "Олифер В.Г., Олифер Н.А. Компьютерные сети. — СПб.: Питер, 2020.",
    ]
    for index, reference in enumerate(references, 1):
        paragraph = document.add_paragraph()
        paragraph.paragraph_format.first_line_indent = Cm(0)
        paragraph.paragraph_format.left_indent = Cm(0)
        paragraph.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
        run = paragraph.add_run(f"{index}. {reference}")
        set_run_font(run, 12)

    add_page_number(document)

    document.core_properties.title = (
        "Теоретическое обоснование и математическое моделирование "
        "оценки живучести гетерогенной информационной инфраструктуры")
    document.core_properties.subject = "ВКР, этап 1"
    document.core_properties.author = "NetSim"

    OUT.parent.mkdir(parents=True, exist_ok=True)
    document.save(OUT)
    print("SAVED", OUT)


if __name__ == "__main__":
    main()
