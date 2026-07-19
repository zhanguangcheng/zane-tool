#include "stopwatchtimer.h"

StopwatchTimer::StopwatchTimer(QObject *parent)
    : QObject(parent)
    , m_state(Idle)
    , m_pausedElapsed(0)
    , m_lastLapTime(0)
{
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(10);
    connect(m_tickTimer, &QTimer::timeout, this, &StopwatchTimer::tick);
}

void StopwatchTimer::start()
{
    if (m_state == Running)
        return;

    if (m_state == Paused) {
        m_elapsed.restart();
        m_tickTimer->start();
        m_state = Running;
        emit stateChanged(m_state);
        return;
    }

    m_pausedElapsed = 0;
    m_lastLapTime = 0;
    m_elapsed.start();
    m_tickTimer->start();
    m_state = Running;
    emit stateChanged(m_state);
}

void StopwatchTimer::pause()
{
    if (m_state != Running)
        return;

    m_pausedElapsed += m_elapsed.elapsed();
    m_tickTimer->stop();
    m_state = Paused;
    emit stateChanged(m_state);
}

void StopwatchTimer::stop()
{
    if (m_state == Idle)
        return;

    qint64 finalElapsed = elapsedMs();
    m_tickTimer->stop();
    m_state = Idle;
    emit stateChanged(m_state);
    emit timeUpdated(formatTime(finalElapsed));
}

void StopwatchTimer::lap()
{
    if (m_state != Running)
        return;

    qint64 now = elapsedMs();
    qint64 lapMs = now - m_lastLapTime;
    m_lastLapTime = now;

    LapEntry entry;
    entry.index = m_laps.size() + 1;
    entry.totalMs = now;
    entry.lapMs = lapMs;
    m_laps.append(entry);

    emit lapRecorded(entry);
}

void StopwatchTimer::reset()
{
    m_tickTimer->stop();
    m_state = Idle;
    m_pausedElapsed = 0;
    m_lastLapTime = 0;
    m_laps.clear();
    emit stateChanged(m_state);
    emit timeUpdated(formatTime(0));
}

StopwatchTimer::State StopwatchTimer::state() const
{
    return m_state;
}

bool StopwatchTimer::isRunning() const
{
    return m_state == Running;
}

qint64 StopwatchTimer::elapsedMs() const
{
    if (m_state == Running)
        return m_pausedElapsed + m_elapsed.elapsed();
    return m_pausedElapsed;
}

const QList<StopwatchTimer::LapEntry> &StopwatchTimer::laps() const
{
    return m_laps;
}

QString StopwatchTimer::formatTime(qint64 ms)
{
    qint64 totalCs = ms / 10;
    qint64 cs = totalCs % 100;
    qint64 totalSec = totalCs / 100;
    qint64 sec = totalSec % 60;
    qint64 totalMin = totalSec / 60;
    qint64 min = totalMin % 60;
    qint64 hours = totalMin / 60;

    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(min, 2, 10, QLatin1Char('0'))
        .arg(sec, 2, 10, QLatin1Char('0'))
        .arg(cs, 2, 10, QLatin1Char('0'));
}

void StopwatchTimer::tick()
{
    emit timeUpdated(formatTime(elapsedMs()));
}
