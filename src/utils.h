#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QFileInfo>

namespace Utils {

QString formatFileSize(qint64 bytes);
bool isSupportedImageFile(const QString &filePath);
bool isSupportedVideoFile(const QString &filePath);
bool isSupportedAudioFile(const QString &filePath);
QString detectImageFormat(const QString &filePath);
QString detectVideoFormat(const QString &filePath);
QString detectAudioFormat(const QString &filePath);
QString defaultImageFormat(const QString &filePath);
QString defaultVideoFormat(const QString &filePath);
QString defaultAudioFormat(const QString &filePath);
void initLogging();
void logToFile(const QString &message);

} // namespace Utils

#endif // UTILS_H
