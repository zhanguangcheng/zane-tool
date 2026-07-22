#ifndef TIMERTOOL_H
#define TIMERTOOL_H

#include <QObject>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>

#include "stopwatchtimer.h"

class TimerTool : public QObject
{
    Q_OBJECT
public:
    explicit TimerTool(QObject *parent = nullptr);

    QWidget *createPage();

private slots:
    void onTimerStart();
    void onTimerPause();
    void onTimerStop();
    void onTimerLap();
    void onTimerClearLaps();
    void onTimerTick(const QString &formatted);
    void onTimerStateChanged(StopwatchTimer::State state);
    void onTimerLapRecorded(const StopwatchTimer::LapEntry &entry);

private:
    StopwatchTimer *m_stopwatch;
    QLabel *m_timerDisplay;
    QPushButton *m_timerStartBtn;
    QPushButton *m_timerPauseBtn;
    QPushButton *m_timerStopBtn;
    QPushButton *m_timerLapBtn;
    QTableWidget *m_timerLapList;
    QPushButton *m_timerClearBtn;
};

#endif // TIMERTOOL_H
