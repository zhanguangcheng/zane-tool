#ifndef FFMPEGPROCESS_H
#define FFMPEGPROCESS_H

#include <QObject>
#include <QProcess>
#include <QStringList>

class FFmpegProcess : public QObject
{
    Q_OBJECT

public:
    explicit FFmpegProcess(QObject *parent = nullptr);
    ~FFmpegProcess();

    void start(const QString &ffmpegPath, const QStringList &args);
    void cancel();
    bool isRunning() const;

signals:
    void finished(bool success, int exitCode);
    void errorOccurred(const QString &message);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessErrorOccurred(QProcess::ProcessError error);

private:
    QProcess *m_process;
};

#endif // FFMPEGPROCESS_H
