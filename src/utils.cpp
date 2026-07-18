#include "utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QFile>

namespace Utils {

QString formatFileSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");
    double v = static_cast<double>(bytes) / 1024.0;
    if (v < 1024.0)
        return QString::number(v, 'f', 1) + QStringLiteral(" KB");
    v /= 1024.0;
    if (v < 1024.0)
        return QString::number(v, 'f', 2) + QStringLiteral(" MB");
    v /= 1024.0;
    return QString::number(v, 'f', 2) + QStringLiteral(" GB");
}

QString detectImageFormat(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    static const QStringList imageExts = {"jpg", "jpeg", "png", "webp", "bmp"};
    if (imageExts.contains(ext))
        return ext == QStringLiteral("jpeg") ? QStringLiteral("jpg") : ext;
    return QStringLiteral("jpg");
}

QString detectVideoFormat(const QString &filePath)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    static const QStringList videoExts = {"mp4", "webm", "avi", "mov", "mkv"};
    if (videoExts.contains(ext))
        return ext;
    return QStringLiteral("mp4");
}

QString defaultImageFormat(const QString &filePath)
{
    QString fmt = detectImageFormat(filePath);
    if (fmt == QStringLiteral("jpeg"))
        return QStringLiteral("jpg");
    return fmt;
}

QString defaultVideoFormat(const QString &filePath)
{
    QString fmt = detectVideoFormat(filePath);
    if (fmt == QStringLiteral("mkv"))
        return QStringLiteral("mp4");
    return fmt;
}

static QFile g_logFile;
static QTextStream g_logStream;

void initLogging()
{
    QString logDir = QCoreApplication::applicationDirPath() + QStringLiteral("/logs");
    QDir().mkpath(logDir);
    g_logFile.setFileName(logDir + QStringLiteral("/app.log"));
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    g_logStream.setDevice(&g_logFile);
}

void logToFile(const QString &message)
{
    if (!g_logFile.isOpen()) return;
    g_logStream << QDateTime::currentDateTime().toString(QStringLiteral("[yyyy-MM-dd hh:mm:ss] "))
                << message << QStringLiteral("\n");
    g_logStream.flush();
}

} // namespace Utils
