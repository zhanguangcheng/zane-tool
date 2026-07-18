#include "ffmpegprocess.h"

#include <QDebug>

FFmpegProcess::FFmpegProcess(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FFmpegProcess::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &FFmpegProcess::onProcessErrorOccurred);
}

FFmpegProcess::~FFmpegProcess()
{
    cancel();
}

void FFmpegProcess::start(const QString &ffmpegPath, const QStringList &args)
{
    m_process->start(ffmpegPath, args);
}

void FFmpegProcess::cancel()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool FFmpegProcess::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void FFmpegProcess::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    emit finished(success, exitCode);
}

void FFmpegProcess::onProcessErrorOccurred(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    QString msg = m_process->errorString();
    emit errorOccurred(msg);
}
