#include "videoprocessor.h"
#include "utils.h"

#include <QFileInfo>
#include <QtMath>

namespace VideoProcessor {

QMap<QString, VideoEncoderInfo> encoderMap()
{
    QMap<QString, VideoEncoderInfo> map;
    map[QStringLiteral("mp4")]       = { QStringLiteral("mp4"), QStringLiteral("libx264"), {} };
    map[QStringLiteral("mp4_hevc")]  = { QStringLiteral("mp4"), QStringLiteral("libx265"), { QStringLiteral("-tag:v"), QStringLiteral("hvc1") } };
    map[QStringLiteral("webm")]      = { QStringLiteral("webm"), QStringLiteral("libvpx-vp9"), { QStringLiteral("-b:v"), QStringLiteral("0") } };
    map[QStringLiteral("avi")]       = { QStringLiteral("avi"), QStringLiteral("mpeg4"), {} };
    map[QStringLiteral("mov")]       = { QStringLiteral("mov"), QStringLiteral("libx264"), {} };
    return map;
}

int crfToAviQScale(int crf)
{
    return qBound(1, static_cast<int>(qRound(1.0 + crf * 30.0 / 51.0)), 31);
}

QString resolveFormat(const QString &inputPath, const QString &format)
{
    if (format.isEmpty())
        return Utils::defaultVideoFormat(inputPath);
    if (format == QStringLiteral("mp4") || format == QStringLiteral("mp4_hevc")
        || format == QStringLiteral("webm") || format == QStringLiteral("avi")
        || format == QStringLiteral("mov"))
        return format;
    return Utils::defaultVideoFormat(inputPath);
}

QStringList buildArgs(const VideoTask &task)
{
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-i") << task.inputPath
         << QStringLiteral("-map_metadata") << QStringLiteral("-1");

    QString fmt = resolveFormat(task.inputPath, task.format);
    const auto map = encoderMap();
    auto it = map.constFind(fmt);

    QStringList vfFilters;

    if (task.enableScale && task.presetRes != QStringLiteral("original")) {
        if (task.presetRes == QStringLiteral("1080")) {
            vfFilters << QStringLiteral("scale=-2:1080");
        } else if (task.presetRes == QStringLiteral("720")) {
            vfFilters << QStringLiteral("scale=-2:720");
        } else if (task.presetRes == QStringLiteral("480")) {
            vfFilters << QStringLiteral("scale=-2:480");
        }
    }

    if (!vfFilters.isEmpty()) {
        args << QStringLiteral("-vf") << vfFilters.join(QStringLiteral(","));
    }

    if (it != map.constEnd()) {
        args << QStringLiteral("-c:v") << it->videoCodec;

        if (fmt == QStringLiteral("avi")) {
            args << QStringLiteral("-q:v") << QString::number(crfToAviQScale(task.crf));
        } else {
            args << QStringLiteral("-crf") << QString::number(task.crf);
        }

        for (const QString &ea : it->extraArgs) {
            args << ea;
        }
    } else {
        args << QStringLiteral("-c:v") << QStringLiteral("libx264")
             << QStringLiteral("-crf") << QString::number(task.crf);
    }

    args << QStringLiteral("-c:a") << QStringLiteral("copy");

    return args;
}

QString buildOutputPath(const VideoTask &task)
{
    QString fmt = resolveFormat(task.inputPath, task.format);
    const auto map = encoderMap();
    auto it = map.constFind(fmt);
    QString ext = (it != map.constEnd()) ? it->container : QStringLiteral("mp4");

    QFileInfo fi(task.inputPath);
    QString baseName = task.outputName.isEmpty() ? fi.completeBaseName() : task.outputName;

    QString dir = task.outputDir;
    if (dir.isEmpty())
        dir = fi.absolutePath();

    if (!dir.endsWith(QLatin1Char('/')) && !dir.endsWith(QLatin1Char('\\')))
        dir += QLatin1Char('/');

    return dir + baseName + QLatin1Char('.') + ext;
}

} // namespace VideoProcessor
