#ifndef STOPWATCHTIMER_H
#define STOPWATCHTIMER_H

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

class StopwatchTimer : public QObject
{
    Q_OBJECT

public:
    enum State { Idle, Running, Paused };
    Q_ENUM(State)

    struct LapEntry {
        int index;
        qint64 totalMs;
        qint64 lapMs;
    };

    explicit StopwatchTimer(QObject *parent = nullptr);

    void start();
    void pause();
    void stop();
    void lap();
    void reset();

    State state() const;
    bool isRunning() const;
    qint64 elapsedMs() const;

    const QList<LapEntry> &laps() const;

    static QString formatTime(qint64 ms);

signals:
    void timeUpdated(const QString &formatted);
    void stateChanged(StopwatchTimer::State state);
    void lapRecorded(const StopwatchTimer::LapEntry &entry);

private slots:
    void tick();

private:
    QElapsedTimer m_elapsed;
    QTimer *m_tickTimer;
    State m_state;
    qint64 m_pausedElapsed;
    qint64 m_lastLapTime;
    QList<LapEntry> m_laps;
};

#endif // STOPWATCHTIMER_H
