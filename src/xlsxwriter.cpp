#include "xlsxwriter.h"

#include <QByteArray>
#include <QFile>
#include <QVector>
#include <QChar>

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

QString columnName(int index)
{
    QString name;
    int n = index + 1;
    while (n > 0) {
        int rem = (n - 1) % 26;
        name.prepend(QChar(ushort('A') + rem));
        n = (n - 1) / 26;
    }
    return name;
}

quint32 crc32(const QByteArray &data)
{
    static quint32 table[256] = {0};
    static bool initialized = false;
    if (!initialized) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }

    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < data.size(); ++i)
        crc = table[(crc ^ static_cast<quint32>(static_cast<quint8>(data.at(i)))) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void putLe(QByteArray *buf, quint32 value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        buf->append(static_cast<char>((value >> (8 * i)) & 0xFF));
}

struct ZipEntry
{
    QString name;
    QByteArray data;
};

bool writeZipStore(const QString &filePath, const QVector<ZipEntry> &entries, QString *errorMessage)
{
    QByteArray zip;
    QVector<quint32> offsets;
    offsets.reserve(entries.size());

    for (const ZipEntry &entry : entries) {
        offsets.append(static_cast<quint32>(zip.size()));
        const QByteArray nameBytes = entry.name.toUtf8();

        putLe(&zip, 0x04034b50u, 4);   // local file header signature
        putLe(&zip, 20, 2);            // version needed to extract
        putLe(&zip, 0x0800, 2);        // general purpose bit flag: UTF-8 names
        putLe(&zip, 0, 2);             // compression method: stored
        putLe(&zip, 0, 2);             // last mod time
        putLe(&zip, 0x21, 2);          // last mod date (1980-01-01)
        putLe(&zip, crc32(entry.data), 4);
        putLe(&zip, entry.data.size(), 4);   // compressed size
        putLe(&zip, entry.data.size(), 4);   // uncompressed size
        putLe(&zip, nameBytes.size(), 2);
        putLe(&zip, 0, 2);             // extra field length
        zip.append(nameBytes);
        zip.append(entry.data);
    }

    const quint32 centralDirOffset = static_cast<quint32>(zip.size());
    for (int i = 0; i < entries.size(); ++i) {
        const ZipEntry &entry = entries.at(i);
        const QByteArray nameBytes = entry.name.toUtf8();

        putLe(&zip, 0x02014b50u, 4);   // central directory signature
        putLe(&zip, 20, 2);            // version made by
        putLe(&zip, 20, 2);            // version needed to extract
        putLe(&zip, 0x0800, 2);        // general purpose bit flag
        putLe(&zip, 0, 2);             // compression method: stored
        putLe(&zip, 0, 2);             // last mod time
        putLe(&zip, 0x21, 2);          // last mod date
        putLe(&zip, crc32(entry.data), 4);
        putLe(&zip, entry.data.size(), 4);
        putLe(&zip, entry.data.size(), 4);
        putLe(&zip, nameBytes.size(), 2);
        putLe(&zip, 0, 2);             // extra field length
        putLe(&zip, 0, 2);             // file comment length
        putLe(&zip, 0, 2);             // disk number start
        putLe(&zip, 0, 2);             // internal file attributes
        putLe(&zip, 0, 4);             // external file attributes
        putLe(&zip, offsets.at(i), 4); // relative offset of local header
        zip.append(nameBytes);
    }

    const quint32 centralDirSize = static_cast<quint32>(zip.size()) - centralDirOffset;
    putLe(&zip, 0x06054b50u, 4);       // end of central directory signature
    putLe(&zip, 0, 2);                 // disk number
    putLe(&zip, 0, 2);                 // disk with central directory
    putLe(&zip, entries.size(), 2);    // entries on this disk
    putLe(&zip, entries.size(), 2);    // total entries
    putLe(&zip, centralDirSize, 4);
    putLe(&zip, centralDirOffset, 4);
    putLe(&zip, 0, 2);                 // comment length

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    file.write(zip);
    file.close();
    return true;
}

QByteArray buildSheetXml(const QStringList &headers, const QList<QList<XlsxWriter::Cell>> &rows)
{
    QString xml;
    xml += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    xml += QStringLiteral("<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n");
    xml += QStringLiteral("<sheetData>\n");

    auto writeCell = [&xml](int row, int col, const XlsxWriter::Cell &cell, int style) {
        const QString ref = columnName(col) + QString::number(row);
        switch (cell.kind) {
        case XlsxWriter::Cell::Number: {
            double d = cell.number;
            qint64 i = static_cast<qint64>(d);
            const QString v = (d == static_cast<double>(i))
                ? QString::number(i)
                : QString::number(d, 'g', 15);
            xml += QStringLiteral("  <c r=\"%1\" s=\"%2\"><v>%3</v></c>\n")
                .arg(ref).arg(style).arg(v);
            break;
        }
        case XlsxWriter::Cell::Bool:
            xml += QStringLiteral("  <c r=\"%1\" s=\"%2\" t=\"b\"><v>%3</v></c>\n")
                .arg(ref).arg(style)
                .arg(cell.boolean ? QStringLiteral("1") : QStringLiteral("0"));
            break;
        case XlsxWriter::Cell::String:
            xml += QStringLiteral("  <c r=\"%1\" s=\"%2\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%3</t></is></c>\n")
                .arg(ref).arg(style).arg(escapeXml(cell.string));
            break;
        case XlsxWriter::Cell::Empty:
        default:
            xml += QStringLiteral("  <c r=\"%1\" s=\"%2\"/>\n").arg(ref).arg(style);
            break;
        }
    };

    if (!headers.isEmpty()) {
        xml += QStringLiteral("  <row r=\"1\">\n");
        for (int c = 0; c < headers.size(); ++c)
            writeCell(1, c, XlsxWriter::Cell::text(headers.at(c)), 1);
        xml += QStringLiteral("  </row>\n");
    }

    for (int r = 0; r < rows.size(); ++r) {
        const QList<XlsxWriter::Cell> &row = rows.at(r);
        const int rowNum = r + 2;
        xml += QStringLiteral("  <row r=\"%1\">\n").arg(rowNum);
        for (int c = 0; c < row.size(); ++c)
            writeCell(rowNum, c, row.at(c), 0);
        xml += QStringLiteral("  </row>\n");
    }

    xml += QStringLiteral("</sheetData>\n");
    xml += QStringLiteral("</worksheet>\n");
    return xml.toUtf8();
}

} // namespace

bool XlsxWriter::writeSheet(const QString &filePath,
                            const QStringList &headers,
                            const QList<QList<Cell>> &rows,
                            QString *errorMessage)
{
    QVector<ZipEntry> entries;

    entries.append(ZipEntry{
        QStringLiteral("[Content_Types].xml"),
        QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
            "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
            "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
            "  <Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
            "  <Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
            "  <Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n"
            "</Types>\n")
    });

    entries.append(ZipEntry{
        QStringLiteral("_rels/.rels"),
        QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
            "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
            "</Relationships>\n")
    });

    entries.append(ZipEntry{
        QStringLiteral("xl/workbook.xml"),
        QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
            "  <sheets>\n"
            "    <sheet name=\"Sheet1\" sheetId=\"1\" r:id=\"rId1\"/>\n"
            "  </sheets>\n"
            "</workbook>\n")
    });

    entries.append(ZipEntry{
        QStringLiteral("xl/_rels/workbook.xml.rels"),
        QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
            "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
            "  <Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n"
            "</Relationships>\n")
    });

    entries.append(ZipEntry{
        QStringLiteral("xl/styles.xml"),
        QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
            "  <fonts count=\"2\">\n"
            "    <font><sz val=\"11\"/><name val=\"Calibri\"/></font>\n"
            "    <font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font>\n"
            "  </fonts>\n"
            "  <fills count=\"1\"><fill><patternFill patternType=\"none\"/></fill></fills>\n"
            "  <borders count=\"1\"><border/></borders>\n"
            "  <cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/></cellStyleXfs>\n"
            "  <cellXfs count=\"2\">\n"
            "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>\n"
            "    <xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/>\n"
            "  </cellXfs>\n"
            "  <cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>\n"
            "</styleSheet>\n")
    });

    entries.append(ZipEntry{
        QStringLiteral("xl/worksheets/sheet1.xml"),
        buildSheetXml(headers, rows)
    });

    return writeZipStore(filePath, entries, errorMessage);
}