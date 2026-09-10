#ifndef XLSXREADER_H
#define XLSXREADER_H

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "ziptool.h"

namespace Xlsx {

inline int cellKey(int row, int col)
{
    return row * 65536 + col;
}

struct Cell
{
    enum Kind { Empty, Text, Number, Bool, Formula, Error };

    Kind kind = Empty;
    QString text;       // Text content / formula computed value / error text
    QString formula;    // formula source (Formula kind)
    double number = 0.0;
    bool boolean = false;
    QString style;      // original cell style index (s="..") to reuse when rewriting
    bool dirty = false;

    QString typeHint;   // raw t attribute while parsing
};

struct Sheet
{
    QString name;       // display name
    QString relPath;    // zip entry path, e.g. xl/worksheets/sheet1.xml
    int maxRow = 0;
    int maxCol = 0;
    QHash<int, Cell> cells;

    QSet<int> deletedRows;                          // rows removed by deleteRows()
    QHash<int, int> rowMap;                         // originalRow -> newRow (after deletions)
};

struct Workbook
{
    QList<ZipEntry> entries;   // all zip parts in original order (preserved/passed through)
    QList<Sheet> sheets;       // sheets parsed into the model
};

bool readWorkbook(const QString &filePath, Workbook *wb, QString *errorMessage);
bool writeWorkbook(const QString &filePath, const Workbook &wb, QString *errorMessage);

int columnFromLetter(const QString &letters);   // 1-based column index, 0 if invalid
QString columnLetter(int column);               // 1-based column -> "A"

QString cellText(const Sheet &sheet, int row, int column);
void setCellText(Sheet *sheet, int row, int column, const QString &text);
void setCellEmpty(Sheet *sheet, int row, int column);
void deleteRows(Sheet *sheet, const QList<int> &rows);

} // namespace Xlsx

#endif // XLSXREADER_H