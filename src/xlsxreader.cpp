#include "xlsxreader.h"

#include <QXmlStreamReader>

namespace {

QString escapeXml(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    out.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    out.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    out.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    return out;
}

QString numToText(double d)
{
    const qint64 i = static_cast<qint64>(d);
    if (d == static_cast<double>(i))
        return QString::number(i);
    return QString::number(d, 'g', 15);
}

bool parseRef(const QString &ref, int *row, int *col)
{
    if (ref.isEmpty())
        return false;
    int i = 0;
    while (i < ref.size() && ref.at(i).isLetter())
        ++i;
    if (i == 0)
        return false;

    int c = 0;
    for (const QChar &ch : ref.left(i).toUpper()) {
        c = c * 26 + (ch.unicode() - 'A' + 1);
        if (c > 16384)
            return false;
    }
    bool ok = false;
    const int r = ref.mid(i).toInt(&ok);
    if (!ok || r <= 0 || r > 1048576 || c <= 0)
        return false;
    *row = r;
    *col = c;
    return true;
}

void emitStartElement(QByteArray *out, QXmlStreamReader &reader,
                      const QString &overrideAttr = QString(),
                      const QString &overrideValue = QString())
{
    out->append('<');
    out->append(reader.name().toString().toUtf8());

    const auto ns = reader.namespaceDeclarations();
    for (const auto &d : ns) {
        out->append(' ');
        if (d.prefix().isEmpty())
            out->append("xmlns");
        else {
            out->append("xmlns:");
            out->append(d.prefix().toString().toUtf8());
        }
        out->append("=\"");
        out->append(escapeXml(d.namespaceUri().toString()).toUtf8());
        out->append('"');
    }

    bool emittedOverride = false;
    const auto attrs = reader.attributes();
    for (const auto &a : attrs) {
        out->append(' ');
        const QString qn = a.qualifiedName().toString();
        if (!overrideAttr.isEmpty() && qn == overrideAttr) {
            out->append(overrideAttr.toUtf8());
            out->append("=\"");
            out->append(escapeXml(overrideValue).toUtf8());
            out->append('"');
            emittedOverride = true;
            continue;
        }
        out->append(qn.toUtf8());
        out->append("=\"");
        out->append(escapeXml(a.value().toString()).toUtf8());
        out->append('"');
    }
    if (!overrideAttr.isEmpty() && !emittedOverride) {
        out->append(' ');
        out->append(overrideAttr.toUtf8());
        out->append("=\"");
        out->append(escapeXml(overrideValue).toUtf8());
        out->append('"');
    }
    out->append('>');
}

void skipElement(QXmlStreamReader &reader)
{
    int depth = 1;
    while (depth > 0) {
        const QXmlStreamReader::TokenType tt = reader.readNext();
        if (tt == QXmlStreamReader::StartElement)
            ++depth;
        else if (tt == QXmlStreamReader::EndElement)
            --depth;
        else if (tt == QXmlStreamReader::Invalid)
            break;
    }
}

QByteArray buildCellXml(int row, int col, const Xlsx::Cell &cell)
{
    const QString ref = Xlsx::columnLetter(col) + QString::number(row);
    const QString sAttr = cell.style.isEmpty()
        ? QString()
        : QStringLiteral(" s=\"%1\"").arg(cell.style);

    QString tAttr;
    QString inner;
    switch (cell.kind) {
    case Xlsx::Cell::Text:
        tAttr = QStringLiteral(" t=\"inlineStr\"");
        inner = QStringLiteral("<is><t xml:space=\"preserve\">%1</t></is>").arg(escapeXml(cell.text));
        break;
    case Xlsx::Cell::Number:
        inner = QStringLiteral("<v>%1</v>").arg(numToText(cell.number));
        break;
    case Xlsx::Cell::Bool:
        tAttr = QStringLiteral(" t=\"b\"");
        inner = QStringLiteral("<v>%1</v>").arg(cell.boolean ? QLatin1String("1") : QLatin1String("0"));
        break;
    case Xlsx::Cell::Error:
        tAttr = QStringLiteral(" t=\"e\"");
        inner = QStringLiteral("<v>%1</v>").arg(escapeXml(cell.text));
        break;
    case Xlsx::Cell::Formula:
        inner = QStringLiteral("<f>%1</f><v>%2</v>")
                    .arg(escapeXml(cell.formula))
                    .arg(escapeXml(cell.text));
        break;
    case Xlsx::Cell::Empty:
    default:
        break;
    }

    return QStringLiteral("<c r=\"%1\"%2%3>%4</c>").arg(ref, sAttr, tAttr, inner).toUtf8();
}

QByteArray transformSheetXml(const QByteArray &raw, const Xlsx::Sheet &sheet)
{
    QByteArray out;
    out += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";

    const bool hasDeletes = !sheet.deletedRows.isEmpty();
    int curRow = 0;

    QXmlStreamReader reader(raw);
    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType tt = reader.readNext();
        switch (tt) {
        case QXmlStreamReader::StartElement: {
            const QString name = reader.name().toString();
            if (name == QLatin1String("row")) {
                bool ok = false;
                const int r = reader.attributes().value(QLatin1String("r")).toString().toInt(&ok);
                curRow = (ok && r > 0) ? r : curRow + 1;
                if (sheet.deletedRows.contains(curRow)) {
                    skipElement(reader);
                    break;
                }
                if (hasDeletes) {
                    const int newRow = sheet.rowMap.value(curRow, curRow);
                    if (newRow >= 1 && newRow != curRow) {
                        emitStartElement(&out, reader, QLatin1String("r"), QString::number(newRow));
                        break;
                    }
                }
                emitStartElement(&out, reader);
                break;
            }
            if (name == QLatin1String("c")) {
                const QString ref = reader.attributes().value(QLatin1String("r")).toString();
                int row = 0, col = 0;
                if (parseRef(ref, &row, &col)) {
                    const auto it = sheet.cells.constFind(Xlsx::cellKey(row, col));
                    if (it != sheet.cells.constEnd() && it->dirty) {
                        const int newRow = hasDeletes ? sheet.rowMap.value(row, row) : row;
                        out.append(buildCellXml(newRow, col, it.value()));
                        skipElement(reader);
                        break;
                    }
                    if (hasDeletes) {
                        const int newRow = sheet.rowMap.value(row, row);
                        if (newRow >= 1 && newRow != row) {
                            emitStartElement(&out, reader, QLatin1String("r"),
                                             Xlsx::columnLetter(col) + QString::number(newRow));
                            break;
                        }
                    }
                }
                emitStartElement(&out, reader);
                break;
            }
            if (name == QLatin1String("dimension") && hasDeletes) {
                QString ref = QLatin1String("A1");
                if (sheet.maxRow >= 1 && sheet.maxCol >= 1)
                    ref = QLatin1String("A1:") + Xlsx::columnLetter(sheet.maxCol)
                        + QString::number(sheet.maxRow);
                emitStartElement(&out, reader, QLatin1String("ref"), ref);
                break;
            }
            emitStartElement(&out, reader);
            break;
        }
        case QXmlStreamReader::Characters:
            out.append(escapeXml(reader.text().toString()).toUtf8());
            break;
        case QXmlStreamReader::Comment:
            out.append("<!--");
            out.append(reader.text().toString().toUtf8());
            out.append("-->");
            break;
        case QXmlStreamReader::EndElement:
            out.append("</");
            out.append(reader.name().toString().toUtf8());
            out.append('>');
            break;
        case QXmlStreamReader::ProcessingInstruction:
            out.append("<?").append(reader.processingInstructionTarget().toString().toUtf8());
            out.append(' ').append(reader.processingInstructionData().toString().toUtf8());
            out.append("?>");
            break;
        case QXmlStreamReader::DTD:
            out.append(reader.text().toString().toUtf8());
            break;
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
        case QXmlStreamReader::Invalid:
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::EntityReference:
            break;
        }
    }
    return out;
}

QStringList parseSharedStrings(const QByteArray &raw)
{
    QStringList list;
    QXmlStreamReader reader(raw);
    bool inSi = false;
    bool inRPh = false;
    int rphDepth = 0;
    bool inT = false;
    QString text;

    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType tt = reader.readNext();
        if (tt == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString();
            if (name == QLatin1String("si") && !inSi) {
                inSi = true;
                text.clear();
                continue;
            }
            if (inSi) {
                if (name == QLatin1String("rPh")) {
                    ++rphDepth;
                    inRPh = true;
                } else if (name == QLatin1String("t") && !inRPh) {
                    inT = true;
                }
            }
        } else if (tt == QXmlStreamReader::Characters) {
            if (inSi && inT)
                text += reader.text();
        } else if (tt == QXmlStreamReader::EndElement) {
            const QString name = reader.name().toString();
            if (inSi && inT && name == QLatin1String("t"))
                inT = false;
            if (inSi && inRPh && name == QLatin1String("rPh")) {
                --rphDepth;
                if (rphDepth <= 0) {
                    rphDepth = 0;
                    inRPh = false;
                }
            }
            if (inSi && name == QLatin1String("si")) {
                list.append(text);
                inSi = false;
                inT = false;
            }
        }
    }
    return list;
}

bool parseSheetXml(const QByteArray &raw, Xlsx::Sheet *sheet, const QStringList &sharedStrings, QString *errorMessage)
{
    QXmlStreamReader reader(raw);
    int cursorRow = 1;
    int cursorCol = 0;
    bool inCell = false;
    Xlsx::Cell cur;
    int curRow = 1, curCol = 0;
    bool inV = false, inT = false, inF = false;
    QString vText, tText, fText;

    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType tt = reader.readNext();
        switch (tt) {
        case QXmlStreamReader::StartElement: {
            const QString name = reader.name().toString();
            if (name == QLatin1String("row")) {
                bool ok = false;
                const int r = reader.attributes().value(QLatin1String("r")).toString().toInt(&ok);
                cursorRow = (ok && r > 0) ? r : cursorRow + 1;
                cursorCol = 0;
            } else if (name == QLatin1String("c")) {
                inCell = true;
                cur = Xlsx::Cell();
                curRow = cursorRow;
                curCol = cursorCol + 1;
                const QString ref = reader.attributes().value(QLatin1String("r")).toString();
                int rr = 0, cc = 0;
                if (parseRef(ref, &rr, &cc)) {
                    curRow = rr;
                    curCol = cc;
                }
                const QString t = reader.attributes().value(QLatin1String("t")).toString();
                const QString s = reader.attributes().value(QLatin1String("s")).toString();
                cur.typeHint = t;
                if (!s.isEmpty())
                    cur.style = s;
                vText.clear();
                tText.clear();
                fText.clear();
                inV = inT = inF = false;
            } else if (inCell) {
                if (name == QLatin1String("v"))
                    inV = true;
                else if (name == QLatin1String("t"))
                    inT = true;
                else if (name == QLatin1String("f"))
                    inF = true;
            }
            break;
        }
        case QXmlStreamReader::Characters:
            if (inV)
                vText += reader.text();
            else if (inT)
                tText += reader.text();
            else if (inF)
                fText += reader.text();
            break;
        case QXmlStreamReader::EndElement: {
            const QString name = reader.name().toString();
            if (name == QLatin1String("c") && inCell) {
                inCell = false;
                cursorRow = curRow;
                cursorCol = curCol;

                Xlsx::Cell cell = cur;
                const QString type = cell.typeHint;
                const bool hasFormula = !fText.isEmpty();

                if (hasFormula) {
                    cell.kind = Xlsx::Cell::Formula;
                    cell.formula = fText;
                    cell.text = vText;
                } else if (type == QLatin1String("s")) {
                    cell.kind = Xlsx::Cell::Text;
                    bool ok = false;
                    const int idx = vText.toInt(&ok);
                    cell.text = (ok && idx >= 0 && idx < sharedStrings.size())
                        ? sharedStrings.at(idx)
                        : vText;
                } else if (type == QLatin1String("inlineStr")) {
                    cell.kind = Xlsx::Cell::Text;
                    cell.text = tText;
                } else if (type == QLatin1String("b")) {
                    cell.kind = Xlsx::Cell::Bool;
                    cell.boolean = (vText == QLatin1String("1"));
                } else if (type == QLatin1String("e")) {
                    cell.kind = Xlsx::Cell::Error;
                    cell.text = vText;
                } else if (type == QLatin1String("str")) {
                    cell.kind = Xlsx::Cell::Text;
                    cell.text = vText;
                } else if (!vText.isEmpty()) {
                    bool ok = false;
                    const double d = vText.toDouble(&ok);
                    if (ok) {
                        cell.kind = Xlsx::Cell::Number;
                        cell.number = d;
                    } else {
                        cell.kind = Xlsx::Cell::Text;
                        cell.text = vText;
                    }
                } else {
                    cell.kind = Xlsx::Cell::Empty;
                }

                sheet->cells.insert(Xlsx::cellKey(curRow, curCol), cell);
                if (curRow > sheet->maxRow)
                    sheet->maxRow = curRow;
                if (curCol > sheet->maxCol)
                    sheet->maxCol = curCol;
            } else if (inCell) {
                if (name == QLatin1String("v"))
                    inV = false;
                else if (name == QLatin1String("t"))
                    inT = false;
                else if (name == QLatin1String("f"))
                    inF = false;
            }
            break;
        }
        default:
            break;
        }
    }

    if (reader.hasError()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("工作表 XML 解析失败: %1").arg(reader.errorString());
        return false;
    }
    return true;
}

} // namespace

namespace Xlsx {

int columnFromLetter(const QString &letters)
{
    const QString s = letters.trimmed().toUpper();
    if (s.isEmpty())
        return 0;
    int c = 0;
    for (const QChar &ch : s) {
        if (ch.unicode() < 'A' || ch.unicode() > 'Z')
            return 0;
        c = c * 26 + (ch.unicode() - 'A' + 1);
        if (c > 16384)
            return 0;
    }
    return c;
}

QString columnLetter(int column)
{
    QString name;
    int n = column;
    while (n > 0) {
        const int rem = (n - 1) % 26;
        name.prepend(QChar(ushort('A' + rem)));
        n = (n - 1) / 26;
    }
    return name;
}

QString cellText(const Sheet &sheet, int row, int column)
{
    const auto it = sheet.cells.constFind(cellKey(row, column));
    if (it == sheet.cells.constEnd())
        return QString();
    switch (it->kind) {
    case Cell::Text:
        return it->text;
    case Cell::Number:
        return numToText(it->number);
    case Cell::Bool:
        return it->boolean ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    case Cell::Formula:
    case Cell::Error:
        return it->text;
    case Cell::Empty:
    default:
        return QString();
    }
}

void setCellText(Sheet *sheet, int row, int column, const QString &text)
{
    Cell &cell = sheet->cells[cellKey(row, column)];
    if (cell.kind == Cell::Text && cell.text == text)
        return;
    Cell updated = cell;
    updated.kind = Cell::Text;
    updated.text = text;
    updated.formula.clear();
    updated.dirty = true;
    sheet->cells.insert(cellKey(row, column), updated);
    if (row > sheet->maxRow)
        sheet->maxRow = row;
    if (column > sheet->maxCol)
        sheet->maxCol = column;
}

void setCellEmpty(Sheet *sheet, int row, int column)
{
    const int key = cellKey(row, column);
    if (!sheet->cells.contains(key))
        return;
    Cell &cell = sheet->cells[key];
    if (cell.kind == Cell::Empty && !cell.dirty)
        return;
    Cell updated;
    updated.style = cell.style;
    updated.dirty = true;
    sheet->cells.insert(key, updated);
}

void deleteRows(Sheet *sheet, const QList<int> &rows)
{
    if (rows.isEmpty())
        return;

    QSet<int> del;
    QHash<int, int> map;
    int newRow = 0;
    for (int r = 1; r <= sheet->maxRow; ++r) {
        if (rows.contains(r)) {
            del.insert(r);
            continue;
        }
        ++newRow;
        map.insert(r, newRow);
    }
    if (del.isEmpty())
        return;

    sheet->deletedRows = del;
    sheet->rowMap = map;

    int mRow = 0, mCol = 0;
    for (auto it = sheet->cells.constBegin(); it != sheet->cells.constEnd(); ++it) {
        const int r = it.key() / 65536;
        const int c = it.key() % 65536;
        if (del.contains(r))
            continue;
        const int nr = map.value(r, r);
        if (nr > mRow)
            mRow = nr;
        if (c > mCol)
            mCol = c;
    }
    sheet->maxRow = mRow;
    sheet->maxCol = mCol;
}

namespace {

QByteArray rawForSheet(const Workbook &wb, const Sheet &sheet)
{
    for (const ZipEntry &e : wb.entries) {
        if (e.name == sheet.relPath)
            return e.data;
    }
    return QByteArray();
}

} // namespace

bool readWorkbook(const QString &filePath, Workbook *wb, QString *errorMessage)
{
    QList<ZipEntry> entries;
    if (!zipRead(filePath, &entries, errorMessage))
        return false;
    wb->entries = entries;

    QHash<QString, int> indexMap;
    for (int i = 0; i < entries.size(); ++i)
        indexMap.insert(entries.at(i).name, i);

    QStringList sharedStrings;
    const int ssIdx = indexMap.value(QStringLiteral("xl/sharedStrings.xml"), -1);
    if (ssIdx >= 0)
        sharedStrings = parseSharedStrings(entries.at(ssIdx).data);

    // map rId -> worksheet relative path
    QHash<QString, QString> relTargets;
    const int relsIdx = indexMap.value(QStringLiteral("xl/_rels/workbook.xml.rels"), -1);
    if (relsIdx >= 0) {
        QXmlStreamReader reader(entries.at(relsIdx).data);
        while (!reader.atEnd()) {
            const QXmlStreamReader::TokenType tt = reader.readNext();
            if (tt == QXmlStreamReader::StartElement && reader.name() == QLatin1String("Relationship")) {
                const QString id = reader.attributes().value(QLatin1String("Id")).toString();
                const QString type = reader.attributes().value(QLatin1String("Type")).toString();
                const QString target = reader.attributes().value(QLatin1String("Target")).toString();
                if (type.endsWith(QLatin1String("/worksheet")) && !target.isEmpty()) {
                    QString path = target;
                    if (path.startsWith(QLatin1Char('/')))
                        path.remove(0, 1);
                    else if (!path.startsWith(QLatin1String("xl/")))
                        path.prepend(QLatin1String("xl/"));
                    relTargets.insert(id, path);
                }
            }
        }
    }

    QStringList sheetNames;
    QStringList firstTargets;
    const int wbIdx = indexMap.value(QStringLiteral("xl/workbook.xml"), -1);
    if (wbIdx >= 0) {
        QXmlStreamReader reader(entries.at(wbIdx).data);
        while (!reader.atEnd()) {
            const QXmlStreamReader::TokenType tt = reader.readNext();
            if (tt == QXmlStreamReader::StartElement && reader.name() == QLatin1String("sheet")) {
                const QString name = reader.attributes().value(QLatin1String("name")).toString();
                const QString rid = reader.attributes().value(QLatin1String("id")).toString();
                const QString rid2 = reader.attributes().value(QLatin1String("r:id")).toString();
                sheetNames.append(name);
                if (!rid2.isEmpty())
                    firstTargets.append(relTargets.value(rid2, QStringLiteral("xl/worksheets/sheet%1.xml").arg(sheetNames.size())));
                else
                    firstTargets.append(QStringLiteral("xl/worksheets/sheet%1.xml").arg(sheetNames.size()));
                (void)rid;
            }
        }
    }

    if (firstTargets.isEmpty()) {
        for (int i = 1; i <= 10; ++i) {
            const QString cand = QStringLiteral("xl/worksheets/sheet%1.xml").arg(i);
            if (indexMap.contains(cand)) {
                firstTargets.append(cand);
                sheetNames.append(QStringLiteral("Sheet%1").arg(i));
                break;
            }
        }
    }

    // 批处理只处理第一个工作表
    if (!firstTargets.isEmpty()) {
        const QString relPath = firstTargets.first();
        const int idx = indexMap.value(relPath, -1);
        if (idx >= 0) {
            Sheet sheet;
            sheet.name = sheetNames.isEmpty() ? QStringLiteral("Sheet1") : sheetNames.first();
            sheet.relPath = relPath;
            if (!parseSheetXml(entries.at(idx).data, &sheet, sharedStrings, errorMessage))
                return false;
            wb->sheets.append(sheet);
        }
    }

    if (wb->sheets.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未找到工作表");
        return false;
    }
    return true;
}

bool writeWorkbook(const QString &filePath, const Workbook &wb, QString *errorMessage)
{
    QList<ZipEntry> out = wb.entries;
    for (const Sheet &sheet : wb.sheets) {
        const QByteArray transformed = transformSheetXml(rawForSheet(wb, sheet), sheet);
        for (int i = 0; i < out.size(); ++i) {
            if (out.at(i).name == sheet.relPath) {
                out[i].data = transformed;
                break;
            }
        }
    }
    return zipWrite(filePath, out, errorMessage);
}

} // namespace Xlsx