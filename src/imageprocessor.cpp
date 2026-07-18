#include "imageprocessor.h"
#include "utils.h"

#include <QFileInfo>
#include <QtMath>

namespace ImageProcessor {

int qualityToQScale(int quality)
{
    return qBound(1, static_cast<int>(qRound(31.0 - quality * 29.0 / 100.0)), 31);
}

QStringList buildArgs(const ImageTask &task)
{
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-i") << task.inputPath
         << QStringLiteral("-map_metadata") << QStringLiteral("-1");

    QString fmt = task.format.isEmpty()
        ? Utils::defaultImageFormat(task.inputPath)
        : task.format.toLower();

    QStringList vfFilters;

    if (task.enableScale && task.scaleWidth > 0) {
        vfFilters << QStringLiteral("scale='if(gt(iw,%1),%1,iw)':-1").arg(task.scaleWidth);
    }

    if (fmt == QStringLiteral("jpg") || fmt == QStringLiteral("jpeg")) {
        int q = qualityToQScale(task.quality);
        args << QStringLiteral("-q:v") << QString::number(q)
             << QStringLiteral("-huffman") << QStringLiteral("optimal");
    } else if (fmt == QStringLiteral("webp")) {
        args << QStringLiteral("-quality") << QString::number(task.quality)
             << QStringLiteral("-lossless") << QStringLiteral("0");
    } else if (fmt == QStringLiteral("png")) {
        args << QStringLiteral("-pred") << QStringLiteral("mixed");
    }

    if (!vfFilters.isEmpty()) {
        args << QStringLiteral("-vf") << vfFilters.join(QStringLiteral(","));
    }

    return args;
}

QString buildOutputPath(const ImageTask &task)
{
    QString fmt = task.format.isEmpty()
        ? Utils::defaultImageFormat(task.inputPath)
        : task.format.toLower();

    QFileInfo fi(task.inputPath);
    QString baseName = task.outputName.isEmpty() ? fi.completeBaseName() : task.outputName;

    QString dir = task.outputDir;
    if (dir.isEmpty())
        dir = fi.absolutePath();

    if (!dir.endsWith(QLatin1Char('/')) && !dir.endsWith(QLatin1Char('\\')))
        dir += QLatin1Char('/');

    return dir + baseName + QLatin1Char('.') + fmt;
}

bool hasMultipleFilesFormatMismatch(const QStringList &paths, const QString &targetFormat)
{
    Q_UNUSED(targetFormat)
    if (paths.size() <= 1)
        return false;

    QString firstExt;
    for (const QString &p : paths) {
        QFileInfo fi(p);
        QString ext = fi.suffix().toLower();
        if (ext == QStringLiteral("jpeg"))
            ext = QStringLiteral("jpg");
        if (firstExt.isEmpty()) {
            firstExt = ext;
        } else if (ext != firstExt) {
            return true;
        }
    }
    return false;
}

} // namespace ImageProcessor
