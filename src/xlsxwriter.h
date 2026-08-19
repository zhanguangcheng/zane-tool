#ifndef XLSXWRITER_H
#define XLSXWRITER_H

#include <QString>
#include <QStringList>
#include <QList>

class XlsxWriter
{
public:
    struct Cell
    {
        enum Kind { String, Number, Bool, Empty };

        Kind kind = Empty;
        QString string;
        double number = 0.0;
        bool boolean = false;

        static Cell text(const QString &text)
        {
            Cell c;
            c.kind = String;
            c.string = text;
            return c;
        }

        static Cell num(double n)
        {
            Cell c;
            c.kind = Number;
            c.number = n;
            return c;
        }

        static Cell flag(bool b)
        {
            Cell c;
            c.kind = Bool;
            c.boolean = b;
            return c;
        }
    };

    static bool writeSheet(const QString &filePath,
                           const QStringList &headers,
                           const QList<QList<Cell>> &rows,
                           QString *errorMessage = nullptr);
};

#endif // XLSXWRITER_H