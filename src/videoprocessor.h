#ifndef VIDEOPROCESSOR_H
#define VIDEOPROCESSOR_H

#include <QString>
#include <QStringList>
#include <QMap>

struct VideoTask {
    QString inputPath;
    QString outputDir;
    QString outputName;
    QString format;
    int crf = 23;
    bool enableScale = false;
    QString presetRes = QStringLiteral("original");
};

struct VideoEncoderInfo {
    QString container;
    QString videoCodec;
    QStringList extraArgs;
};

namespace VideoProcessor {

QStringList buildArgs(const VideoTask &task);
QString buildOutputPath(const VideoTask &task);
int qualityToCrf(int quality);
int crfToAviQScale(int crf);
QString resolveFormat(const QString &inputPath, const QString &format);
QMap<QString, VideoEncoderInfo> encoderMap();

} // namespace VideoProcessor

#endif // VIDEOPROCESSOR_H
