#ifndef AUDIOPROCESSOR_H
#define AUDIOPROCESSOR_H

#include <QString>
#include <QStringList>
#include <QMap>

struct AudioTask {
    QString inputPath;
    QString outputDir;
    QString outputName;
    QString format;      // "", "mp3", "m4a", "flac", "wav", "ogg", "opus"
    QString bitrate;     // "64k", "128k", "192k", "256k", "320k"
    QString sampleRate;  // "", "8000", "11025", "22050", "44100", "48000"
    QString channels;    // "", "1", "2"
};

struct AudioEncoderInfo {
    QString container;
    QString audioCodec;
};

namespace AudioProcessor {

QStringList buildArgs(const AudioTask &task);
QString buildOutputPath(const AudioTask &task);
QString resolveFormat(const QString &inputPath, const QString &format);
QMap<QString, AudioEncoderInfo> encoderMap();

} // namespace AudioProcessor

#endif // AUDIOPROCESSOR_H
