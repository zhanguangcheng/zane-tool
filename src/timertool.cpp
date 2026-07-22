#include <QHeaderView>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "timertool.h"

TimerTool::TimerTool(QObject *parent)
    : QObject(parent)
    , m_stopwatch(new StopwatchTimer(this))
    , m_timerDisplay(nullptr)
    , m_timerStartBtn(nullptr)
    , m_timerPauseBtn(nullptr)
    , m_timerStopBtn(nullptr)
    , m_timerLapBtn(nullptr)
    , m_timerLapList(nullptr)
    , m_timerClearBtn(nullptr)
{
    connect(m_stopwatch, &StopwatchTimer::timeUpdated,
            this, &TimerTool::onTimerTick);
    connect(m_stopwatch, &StopwatchTimer::stateChanged,
            this, &TimerTool::onTimerStateChanged);
    connect(m_stopwatch, &StopwatchTimer::lapRecorded,
            this, &TimerTool::onTimerLapRecorded);
}

QWidget *TimerTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *displayGroup = new QGroupBox(QStringLiteral("计时"), page);
    QVBoxLayout *displayLayout = new QVBoxLayout(displayGroup);
    displayLayout->setAlignment(Qt::AlignCenter);

    m_timerDisplay = new QLabel(QStringLiteral("00:00:00.00"), displayGroup);
    m_timerDisplay->setAlignment(Qt::AlignCenter);
    m_timerDisplay->setStyleSheet(QStringLiteral(
        "font-family: 'Consolas', 'Courier New', monospace;"
        "font-size: 52px;"
        "font-weight: bold;"
        "color: #212529;"
        "padding: 16px 0;"
    ));

    displayLayout->addWidget(m_timerDisplay);

    QGroupBox *controlsGroup = new QGroupBox(QStringLiteral("控制"), page);
    QHBoxLayout *controlsLayout = new QHBoxLayout(controlsGroup);
    controlsLayout->setSpacing(10);

    m_timerStartBtn = new QPushButton(QStringLiteral("开始"), controlsGroup);
    m_timerStartBtn->setFixedHeight(40);
    m_timerStartBtn->setMinimumWidth(80);
    m_timerStartBtn->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 15px; font-weight: bold; background-color: #198754; }"
        "QPushButton:hover { background-color: #157347; }"
        "QPushButton:pressed { background-color: #146c43; }"
        "QPushButton:disabled { background-color: #6c757d; color: #ced4da; }"
    ));
    connect(m_timerStartBtn, &QPushButton::clicked, this, &TimerTool::onTimerStart);

    m_timerPauseBtn = new QPushButton(QStringLiteral("暂停"), controlsGroup);
    m_timerPauseBtn->setFixedHeight(40);
    m_timerPauseBtn->setMinimumWidth(80);
    m_timerPauseBtn->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 15px; font-weight: bold; background-color: #fd7e14; }"
        "QPushButton:hover { background-color: #e06b0c; }"
        "QPushButton:pressed { background-color: #c95e0a; }"
        "QPushButton:disabled { background-color: #6c757d; color: #ced4da; }"
    ));
    m_timerPauseBtn->setEnabled(false);
    connect(m_timerPauseBtn, &QPushButton::clicked, this, &TimerTool::onTimerPause);

    m_timerStopBtn = new QPushButton(QStringLiteral("停止"), controlsGroup);
    m_timerStopBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_timerStopBtn->setFixedHeight(40);
    m_timerStopBtn->setMinimumWidth(80);
    m_timerStopBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_timerStopBtn->setEnabled(false);
    connect(m_timerStopBtn, &QPushButton::clicked, this, &TimerTool::onTimerStop);

    m_timerLapBtn = new QPushButton(QStringLiteral("计次"), controlsGroup);
    m_timerLapBtn->setFixedHeight(40);
    m_timerLapBtn->setMinimumWidth(80);
    m_timerLapBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_timerLapBtn->setEnabled(false);
    connect(m_timerLapBtn, &QPushButton::clicked, this, &TimerTool::onTimerLap);

    controlsLayout->addWidget(m_timerStartBtn);
    controlsLayout->addWidget(m_timerPauseBtn);
    controlsLayout->addWidget(m_timerStopBtn);
    controlsLayout->addWidget(m_timerLapBtn);

    QGroupBox *lapsGroup = new QGroupBox(QStringLiteral("计次记录"), page);
    QVBoxLayout *lapsLayout = new QVBoxLayout(lapsGroup);
    lapsLayout->setSpacing(8);

    m_timerLapList = new QTableWidget(0, 3, lapsGroup);
    m_timerLapList->setMinimumHeight(180);
    m_timerLapList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_timerLapList->setSelectionMode(QAbstractItemView::NoSelection);
    m_timerLapList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_timerLapList->setFocusPolicy(Qt::NoFocus);
    m_timerLapList->verticalHeader()->setVisible(false);
    m_timerLapList->horizontalHeader()->setStretchLastSection(true);
    m_timerLapList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_timerLapList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_timerLapList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    QStringList headers;
    headers << QStringLiteral("序号") << QStringLiteral("分段时间") << QStringLiteral("累计时间");
    m_timerLapList->setHorizontalHeaderLabels(headers);

    m_timerLapList->setStyleSheet(QStringLiteral(
        "QTableWidget { font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f3f5; color: #495057; font-weight: bold; "
        "  padding: 4px 8px; border: none; border-bottom: 2px solid #dee2e6; font-size: 12px; }"
    ));

    m_timerClearBtn = new QPushButton(QStringLiteral("清除记录"), lapsGroup);
    m_timerClearBtn->setFixedHeight(32);
    m_timerClearBtn->setFixedWidth(100);
    m_timerClearBtn->setStyleSheet(QStringLiteral("font-size: 12px;"));
    connect(m_timerClearBtn, &QPushButton::clicked, this, &TimerTool::onTimerClearLaps);

    lapsLayout->addWidget(m_timerLapList);
    lapsLayout->addWidget(m_timerClearBtn, 0, Qt::AlignLeft);

    mainLayout->addWidget(displayGroup);
    mainLayout->addWidget(controlsGroup);
    mainLayout->addWidget(lapsGroup, 1);

    return page;
}

void TimerTool::onTimerStart()
{
    m_stopwatch->start();
}

void TimerTool::onTimerPause()
{
    m_stopwatch->pause();
}

void TimerTool::onTimerStop()
{
    m_stopwatch->stop();
}

void TimerTool::onTimerLap()
{
    m_stopwatch->lap();
}

void TimerTool::onTimerClearLaps()
{
    m_stopwatch->reset();
    m_timerLapList->setRowCount(0);
}

void TimerTool::onTimerTick(const QString &formatted)
{
    m_timerDisplay->setText(formatted);
}

void TimerTool::onTimerStateChanged(StopwatchTimer::State state)
{
    bool running = (state == StopwatchTimer::Running);
    bool idle = (state == StopwatchTimer::Idle);
    bool paused = (state == StopwatchTimer::Paused);

    m_timerStartBtn->setEnabled(!running);
    m_timerStartBtn->setText(paused ? QStringLiteral("继续") : QStringLiteral("开始"));
    m_timerPauseBtn->setEnabled(running);
    m_timerStopBtn->setEnabled(!idle);
    m_timerLapBtn->setEnabled(running);
    m_timerClearBtn->setEnabled(!running);
}

void TimerTool::onTimerLapRecorded(const StopwatchTimer::LapEntry &entry)
{
    int row = m_timerLapList->rowCount();
    m_timerLapList->insertRow(row);

    QTableWidgetItem *indexItem = new QTableWidgetItem(
        QStringLiteral("#%1").arg(entry.index, 2));
    indexItem->setTextAlignment(Qt::AlignCenter);

    QTableWidgetItem *lapItem = new QTableWidgetItem(
        StopwatchTimer::formatTime(entry.lapMs));

    QTableWidgetItem *totalItem = new QTableWidgetItem(
        StopwatchTimer::formatTime(entry.totalMs));

    m_timerLapList->setItem(row, 0, indexItem);
    m_timerLapList->setItem(row, 1, lapItem);
    m_timerLapList->setItem(row, 2, totalItem);
    m_timerLapList->scrollToBottom();
}
