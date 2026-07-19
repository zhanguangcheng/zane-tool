#include "audioprocessor.h"
#include "utils.h"

#include <QFileInfo>

namespace AudioProcessor {

QMap<QString, AudioEncoderInfo> encoderMap()
{
    QMap<QString, AudioEncoderInfo> map;
    map[QStringLiteral("mp3")]  = { QStringLiteral("mp3"), QStringLiteral("libmp3lame") };
    map[QStringLiteral("m4a")]  = { QStringLiteral("m4a"), QStringLiteral("aac") };
    map[QStringLiteral("flac")] = { QStringLiteral("flac"), QStringLiteral("flac") };
    map[QStringLiteral("wav")]  = { QStringLiteral("wav"), QStringLiteral("pcm_s16le") };
    map[QStringLiteral("ogg")]  = { QStringLiteral("ogg"), QStringLiteral("libvorbis") };
    map[QStringLiteral("opus")] = { QStringLiteral("opus"), QStringLiteral("libopus") };
    return map;
}

QString resolveFormat(const QString &inputPath, const QString &format)
{
    if (format.isEmpty())
        return Utils::defaultAudioFormat(inputPath);

    const auto map = encoderMap();
    if (map.contains(format))
        return format;

    return Utils::defaultAudioFormat(inputPath);
}

QStringList buildArgs(const AudioTask &task)
{
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-i") << task.inputPath
         << QStringLiteral("-map_metadata") << QStringLiteral("-1")
         << QStringLiteral("-vn");

    QString fmt = resolveFormat(task.inputPath, task.format);
    const auto map = encoderMap();
    auto it = map.constFind(fmt);

    if (it != map.constEnd()) {
        args << QStringLiteral("-c:a") << it->audioCodec;
    } else {
        args << QStringLiteral("-c:a") << QStringLiteral("libmp3lame");
    }

    bool isLossless = (fmt == QStringLiteral("flac") || fmt == QStringLiteral("wav"));
    if (!isLossless && !task.bitrate.isEmpty()) {
        args << QStringLiteral("-b:a") << task.bitrate;
    }

    if (!task.sampleRate.isEmpty()) {
        args << QStringLiteral("-ar") << task.sampleRate;
    }

    if (!task.channels.isEmpty()) {
        args << QStringLiteral("-ac") << task.channels;
    }

    return args;
}

QString buildOutputPath(const AudioTask &task)
{
    QString fmt = resolveFormat(task.inputPath, task.format);
    const auto map = encoderMap();
    auto it = map.constFind(fmt);
    QString ext = (it != map.constEnd()) ? it->container : QStringLiteral("mp3");

    QFileInfo fi(task.inputPath);
    QString baseName = task.outputName.isEmpty() ? fi.completeBaseName() : task.outputName;

    QString dir = task.outputDir;
    if (dir.isEmpty())
        dir = fi.absolutePath();

    if (!dir.endsWith(QLatin1Char('/')) && !dir.endsWith(QLatin1Char('\\')))
        dir += QLatin1Char('/');

    return dir + baseName + QLatin1Char('.') + ext;
}

} // namespace AudioProcessor
