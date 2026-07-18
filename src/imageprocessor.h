#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QString>
#include <QStringList>

struct ImageTask {
    QString inputPath;
    QString outputDir;
    QString outputName;
    QString format;
    int quality = 75;
    bool enableScale = false;
    int scaleWidth = 1920;
};

namespace ImageProcessor {

QStringList buildArgs(const ImageTask &task);
QString buildOutputPath(const ImageTask &task);
int qualityToQScale(int quality);
bool hasMultipleFilesFormatMismatch(const QStringList &paths, const QString &targetFormat);

} // namespace ImageProcessor

#endif // IMAGEPROCESSOR_H
