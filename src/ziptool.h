#ifndef ZIPTOOL_H
#define ZIPTOOL_H

#include <QByteArray>
#include <QList>
#include <QString>

struct ZipEntry
{
    QString name;
    QByteArray data;
};

bool zipRead(const QString &filePath, QList<ZipEntry> *entries, QString *errorMessage);
bool zipWrite(const QString &filePath, const QList<ZipEntry> &entries, QString *errorMessage);

#endif // ZIPTOOL_H