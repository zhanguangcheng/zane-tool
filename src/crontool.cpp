#include <QApplication>
#include <QClipboard>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QDateTime>

#include "crontool.h"

CronTool::CronTool(QObject *parent)
    : QObject(parent)
    , m_cronInputEdit(nullptr)
    , m_cronPresetCombo(nullptr)
    , m_cronCountCombo(nullptr)
    , m_cronCopyBtn(nullptr)
    , m_cronNextTable(nullptr)
    , m_cronMinField(nullptr)
    , m_cronHourField(nullptr)
    , m_cronDomField(nullptr)
    , m_cronMonthField(nullptr)
    , m_cronDowField(nullptr)
    , m_cronErrorLabel(nullptr)
    , m_cronDescLabel(nullptr)
    , m_cronTimer(nullptr)
{
}

QWidget *CronTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *exprGroup = new QGroupBox(QStringLiteral("Cron 表达式"), page);
    QVBoxLayout *exprLayout = new QVBoxLayout(exprGroup);
    exprLayout->setSpacing(10);

    QHBoxLayout *inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);

    m_cronInputEdit = new QLineEdit(exprGroup);
    m_cronInputEdit->setPlaceholderText(QStringLiteral("分 时 日 月 周，如: */15 8-18 * * 1-5"));
    m_cronInputEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    connect(m_cronInputEdit, &QLineEdit::textChanged,
            this, &CronTool::onCronInputChanged);

    m_cronPresetCombo = new QComboBox(exprGroup);
    m_cronPresetCombo->setFixedWidth(130);
    m_cronPresetCombo->addItem(QStringLiteral("自定义"));
    m_cronPresetCombo->addItem(QStringLiteral("每1分钟"));
    m_cronPresetCombo->addItem(QStringLiteral("每5分钟"));
    m_cronPresetCombo->addItem(QStringLiteral("每15分钟"));
    m_cronPresetCombo->addItem(QStringLiteral("每30分钟"));
    m_cronPresetCombo->addItem(QStringLiteral("每小时"));
    m_cronPresetCombo->addItem(QStringLiteral("每天零点"));
    m_cronPresetCombo->addItem(QStringLiteral("每周一零点"));
    m_cronPresetCombo->addItem(QStringLiteral("每月1号零点"));
    m_cronPresetCombo->addItem(QStringLiteral("工作日每小时"));
    connect(m_cronPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CronTool::onCronPresetChanged);

    inputRow->addWidget(m_cronInputEdit, 1);
    inputRow->addWidget(m_cronPresetCombo);

    m_cronErrorLabel = new QLabel(exprGroup);
    m_cronErrorLabel->setStyleSheet(
        QStringLiteral("color: #dc3545; font-size: 12px; font-weight: bold;"));
    m_cronErrorLabel->setVisible(false);

    exprLayout->addLayout(inputRow);
    exprLayout->addWidget(m_cronErrorLabel);

    QGroupBox *fieldGroup = new QGroupBox(QStringLiteral("字段解析"), page);
    QGridLayout *fieldGrid = new QGridLayout(fieldGroup);
    fieldGrid->setSpacing(6);
    fieldGrid->setColumnStretch(1, 1);

    auto addFieldRow = [&](int row, const QString &name, QLabel *&valueLabel) {
        QLabel *nameLabel = new QLabel(name, fieldGroup);
        nameLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #495057;"));
        valueLabel = new QLabel(QStringLiteral("-"), fieldGroup);
        valueLabel->setStyleSheet(
            QStringLiteral("font-family: 'Consolas', 'Courier New', monospace; "
                           "color: #212529;"));
        fieldGrid->addWidget(nameLabel, row, 0);
        fieldGrid->addWidget(valueLabel, row, 1);
    };

    QLabel *descTitle = new QLabel(QStringLiteral("描述:"), fieldGroup);
    descTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #495057;"));
    m_cronDescLabel = new QLabel(QStringLiteral("-"), fieldGroup);
    m_cronDescLabel->setStyleSheet(
        QStringLiteral("color: #0d6efd; font-size: 13px;"));
    m_cronDescLabel->setWordWrap(true);
    fieldGrid->addWidget(descTitle, 5, 0);
    fieldGrid->addWidget(m_cronDescLabel, 5, 1);

    addFieldRow(0, QStringLiteral("分钟:"), m_cronMinField);
    addFieldRow(1, QStringLiteral("小时:"), m_cronHourField);
    addFieldRow(2, QStringLiteral("日期:"), m_cronDomField);
    addFieldRow(3, QStringLiteral("月份:"), m_cronMonthField);
    addFieldRow(4, QStringLiteral("星期:"), m_cronDowField);

    QGroupBox *nextGroup = new QGroupBox(QStringLiteral("未来执行时间"), page);
    QVBoxLayout *nextLayout = new QVBoxLayout(nextGroup);
    nextLayout->setSpacing(8);

    QHBoxLayout *nextTopRow = new QHBoxLayout();
    nextTopRow->setSpacing(4);
    QLabel *showLabel = new QLabel(QStringLiteral("显示最近"), nextGroup);
    m_cronCountCombo = new QComboBox(nextGroup);
    m_cronCountCombo->setFixedWidth(70);
    m_cronCountCombo->addItems({QStringLiteral("5"), QStringLiteral("10"),
                                QStringLiteral("20"), QStringLiteral("50")});
    m_cronCountCombo->setCurrentIndex(1);
    connect(m_cronCountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CronTool::onCronCountChanged);
    QLabel *timesLabel = new QLabel(QStringLiteral("次执行"), nextGroup);
    nextTopRow->addWidget(showLabel);
    nextTopRow->addWidget(m_cronCountCombo);
    nextTopRow->addWidget(timesLabel);
    nextTopRow->addStretch();

    m_cronNextTable = new QTableWidget(0, 3, nextGroup);
    m_cronNextTable->setMinimumHeight(180);
    m_cronNextTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cronNextTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_cronNextTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cronNextTable->setFocusPolicy(Qt::NoFocus);
    m_cronNextTable->verticalHeader()->setVisible(false);
    m_cronNextTable->horizontalHeader()->setStretchLastSection(true);
    m_cronNextTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_cronNextTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_cronNextTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    QStringList headers;
    headers << QStringLiteral("序号") << QStringLiteral("执行时间") << QStringLiteral("相对时间");
    m_cronNextTable->setHorizontalHeaderLabels(headers);

    m_cronNextTable->setStyleSheet(QStringLiteral(
        "QTableWidget { font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f3f5; color: #495057; font-weight: bold; "
        "  padding: 4px 8px; border: none; border-bottom: 2px solid #dee2e6; font-size: 12px; }"));

    m_cronCopyBtn = new QPushButton(QStringLiteral("复制全部"), nextGroup);
    m_cronCopyBtn->setFixedHeight(32);
    m_cronCopyBtn->setFixedWidth(100);
    m_cronCopyBtn->setCursor(Qt::PointingHandCursor);
    m_cronCopyBtn->setEnabled(false);
    connect(m_cronCopyBtn, &QPushButton::clicked, this, &CronTool::onCronCopyAll);

    nextLayout->addLayout(nextTopRow);
    nextLayout->addWidget(m_cronNextTable, 1);
    nextLayout->addWidget(m_cronCopyBtn, 0, Qt::AlignLeft);

    mainLayout->addWidget(exprGroup);
    mainLayout->addWidget(fieldGroup);
    mainLayout->addWidget(nextGroup, 1);

    m_cronTimer = new QTimer(this);
    connect(m_cronTimer, &QTimer::timeout, this, &CronTool::onCronUpdateTimes);

    return page;
}

void CronTool::startTimer()
{
    if (m_cronTimer && !m_cronTimer->isActive())
        m_cronTimer->start(30000);
}

void CronTool::stopTimer()
{
    if (m_cronTimer)
        m_cronTimer->stop();
}

// ==================== Cron helpers ====================

std::set<int> CronTool::parseCronField(const QString &field, int minVal, int maxVal, bool &ok)
{
    std::set<int> result;
    ok = true;

    if (field.isEmpty()) {
        ok = false;
        return result;
    }

    if (field == QStringLiteral("*")) {
        return result;
    }

    const QStringList parts = field.split(',');
    for (const QString &part : parts) {
        QString p = part.trimmed();
        if (p.isEmpty()) { ok = false; return {}; }

        int step = 1;
        int slashPos = p.indexOf('/');
        if (slashPos != -1) {
            bool stepOk = false;
            step = p.mid(slashPos + 1).toInt(&stepOk);
            if (!stepOk || step < 1) { ok = false; return {}; }
            p = p.left(slashPos);
        }

        if (p == QStringLiteral("*")) {
            for (int i = minVal; i <= maxVal; i += step) {
                result.insert(i);
            }
        } else if (p.contains('-')) {
            QStringList range = p.split('-');
            if (range.size() != 2) { ok = false; return {}; }
            bool ok1 = false, ok2 = false;
            int start = range[0].toInt(&ok1);
            int end = range[1].toInt(&ok2);
            if (!ok1 || !ok2 || start < minVal || end > maxVal || start > end) {
                ok = false;
                return {};
            }
            for (int i = start; i <= end; i += step) {
                result.insert(i);
            }
        } else {
            bool valOk = false;
            int val = p.toInt(&valOk);
            if (!valOk || val < minVal || val > maxVal) { ok = false; return {}; }
            result.insert(val);
        }
    }

    return result;
}

CronTool::CronFields CronTool::parseCron(const QString &expr)
{
    CronFields cf;
    QString trimmed = expr.trimmed();

    if (trimmed == QStringLiteral("@yearly") || trimmed == QStringLiteral("@annually")) {
        trimmed = QStringLiteral("0 0 1 1 *");
    } else if (trimmed == QStringLiteral("@monthly")) {
        trimmed = QStringLiteral("0 0 1 * *");
    } else if (trimmed == QStringLiteral("@weekly")) {
        trimmed = QStringLiteral("0 0 * * 0");
    } else if (trimmed == QStringLiteral("@daily") || trimmed == QStringLiteral("@midnight")) {
        trimmed = QStringLiteral("0 0 * * *");
    } else if (trimmed == QStringLiteral("@hourly")) {
        trimmed = QStringLiteral("0 * * * *");
    }

    QStringList fields = trimmed.split(' ', Qt::SkipEmptyParts);
    if (fields.size() != 5) {
        cf.error = QStringLiteral("需要5个字段（分 时 日 月 周），当前 %1 个字段").arg(fields.size());
        return cf;
    }

    bool ok = false;
    cf.minutes = parseCronField(fields[0], 0, 59, ok);
    if (!ok) { cf.error = QStringLiteral("分钟字段无效: ") + fields[0]; return cf; }

    cf.hours = parseCronField(fields[1], 0, 23, ok);
    if (!ok) { cf.error = QStringLiteral("小时字段无效: ") + fields[1]; return cf; }

    cf.doms = parseCronField(fields[2], 1, 31, ok);
    if (!ok) { cf.error = QStringLiteral("日期字段无效: ") + fields[2]; return cf; }

    cf.months = parseCronField(fields[3], 1, 12, ok);
    if (!ok) { cf.error = QStringLiteral("月份字段无效: ") + fields[3]; return cf; }

    cf.dows = parseCronField(fields[4], 0, 6, ok);
    if (!ok) { cf.error = QStringLiteral("星期字段无效: ") + fields[4]; return cf; }

    cf.valid = true;
    return cf;
}

int CronTool::firstVal(const std::set<int> &s, int defVal)
{
    return s.empty() ? defVal : *s.begin();
}

int CronTool::nextGe(const std::set<int> &s, int val)
{
    auto it = s.lower_bound(val);
    return (it == s.end()) ? -1 : *it;
}

QVector<QDateTime> CronTool::computeNextTimes(const CronFields &cf, int count, const QDateTime &from)
{
    QVector<QDateTime> result;
    if (count <= 0) return result;

    QDateTime t(from.date(), QTime(from.time().hour(), from.time().minute()));
    t = t.addSecs(60);

    const int maxIter = 525600 * 10;

    for (int iter = 0; iter < maxIter && result.size() < count; ++iter) {
        int minute = t.time().minute();
        int hour = t.time().hour();
        int day = t.date().day();
        int month = t.date().month();
        int dow = t.date().dayOfWeek() % 7;

        if (!cf.minutes.empty()) {
            int next = nextGe(cf.minutes, minute);
            if (next == -1) {
                t = t.addSecs((60 - minute) * 60);
                t.setTime(QTime(t.time().hour(), firstVal(cf.minutes, 0), 0));
                continue;
            }
            if (next != minute) {
                t.setTime(QTime(hour, next, 0));
                continue;
            }
        }

        if (!cf.hours.empty()) {
            int next = nextGe(cf.hours, hour);
            if (next == -1) {
                int fm = firstVal(cf.minutes, 0);
                int fh = firstVal(cf.hours, 0);
                t = t.addDays(1);
                t.setTime(QTime(fh, fm, 0));
                continue;
            }
            if (next != hour) {
                int fm = firstVal(cf.minutes, 0);
                t.setTime(QTime(next, fm, 0));
                continue;
            }
        }

        bool domR = !cf.doms.empty();
        bool dowR = !cf.dows.empty();
        bool dayOk = false;
        if (!domR && !dowR) {
            dayOk = true;
        } else if (domR && dowR) {
            dayOk = cf.doms.find(day) != cf.doms.end()
                 || cf.dows.find(dow) != cf.dows.end();
        } else if (domR) {
            dayOk = cf.doms.find(day) != cf.doms.end();
        } else {
            dayOk = cf.dows.find(dow) != cf.dows.end();
        }

        if (!dayOk) {
            int fm = firstVal(cf.minutes, 0);
            int fh = firstVal(cf.hours, 0);
            t = t.addDays(1);
            t.setTime(QTime(fh, fm, 0));
            continue;
        }

        if (!cf.months.empty()) {
            int next = nextGe(cf.months, month);
            if (next == -1) {
                int fm = firstVal(cf.minutes, 0);
                int fh = firstVal(cf.hours, 0);
                int fmon = firstVal(cf.months, 1);
                t.setDate(QDate(t.date().year() + 1, fmon, 1));
                t.setTime(QTime(fh, fm, 0));
                continue;
            }
            if (next != month) {
                int fm = firstVal(cf.minutes, 0);
                int fh = firstVal(cf.hours, 0);
                t.setDate(QDate(t.date().year(), next, 1));
                t.setTime(QTime(fh, fm, 0));
                continue;
            }
        }

        result.append(t);
        t = t.addSecs(60);
    }

    return result;
}

QString CronTool::describeFieldValues(const std::set<int> &values, int minVal, int maxVal,
                                      const QString &unitSingular)
{
    if (values.empty())
        return QStringLiteral("每%1").arg(unitSingular);

    if (values.size() == 1)
        return QString::number(*values.begin());

    if (values.size() >= 2) {
        auto it = values.begin();
        int first = *it;
        ++it;
        int second = *it;
        int step = second - first;
        bool isUniform = (step > 0);
        int prev = second;
        ++it;
        while (it != values.end() && isUniform) {
            if (*it - prev != step) isUniform = false;
            prev = *it;
            ++it;
        }
        if (isUniform && first == minVal) {
            return QStringLiteral("每%1%2").arg(step).arg(unitSingular);
        }
    }

    int first = *values.begin();
    int last = *values.rbegin();
    bool isRange = true;
    int expected = first;
    for (int v : values) {
        if (v != expected) { isRange = false; break; }
        expected++;
    }
    if (isRange && first == minVal && last == maxVal)
        return QStringLiteral("每%1").arg(unitSingular);
    if (isRange)
        return QStringLiteral("%1-%2").arg(first).arg(last);

    QStringList parts;
    for (int v : values)
        parts.append(QString::number(v));
    return parts.join(',');
}

QString CronTool::describeDow(const std::set<int> &values)
{
    if (values.empty()) return QStringLiteral("每天");

    static const QString dowNames[] = {
        QStringLiteral("日"), QStringLiteral("一"), QStringLiteral("二"),
        QStringLiteral("三"), QStringLiteral("四"), QStringLiteral("五"),
        QStringLiteral("六")
    };
    QStringList parts;
    for (int v : values)
        parts.append(QStringLiteral("周%1").arg(dowNames[v]));

    if (parts.size() == 7) return QStringLiteral("每天");
    if (parts.size() == 2 && values.find(0) != values.end()
        && values.find(6) != values.end())
        return QStringLiteral("周末");
    if (values.find(0) == values.end() && values.find(6) == values.end()
        && values.size() == 5)
        return QStringLiteral("工作日");

    return parts.join(QStringLiteral(", "));
}

QString CronTool::describeDom(const std::set<int> &values)
{
    if (values.empty()) return QStringLiteral("每天");
    if (values.size() == 1) return QStringLiteral("%1号").arg(*values.begin());
    QStringList parts;
    for (int v : values)
        parts.append(QStringLiteral("%1号").arg(v));
    return parts.join(QStringLiteral(", "));
}

QString CronTool::describeMonth(const std::set<int> &values)
{
    if (values.empty()) return QStringLiteral("每月");
    if (values.size() == 1) return QStringLiteral("%1月").arg(*values.begin());
    QStringList parts;
    for (int v : values)
        parts.append(QStringLiteral("%1月").arg(v));
    return parts.join(QStringLiteral(", "));
}

QString CronTool::relativeTimeDesc(const QDateTime &target, const QDateTime &now)
{
    qint64 secs = now.secsTo(target);
    if (secs < 0) return QStringLiteral("已过期");
    if (secs < 60) return QStringLiteral("不到 1 分钟");
    if (secs < 3600) return QStringLiteral("约 %1 分钟后").arg(secs / 60);
    if (secs < 86400) return QStringLiteral("约 %1 小时 %2 分钟后")
                            .arg(secs / 3600).arg((secs % 3600) / 60);
    if (secs < 2592000) return QStringLiteral("约 %1 天后").arg(secs / 86400);
    if (secs < 31536000) return QStringLiteral("约 %1 个月后").arg(secs / 2592000);
    return QStringLiteral("约 %1 年后").arg(secs / 31536000);
}

// ==================== Cron Slots ====================

void CronTool::onCronInputChanged()
{
    const QString text = m_cronInputEdit->text().trimmed();

    if (text.isEmpty()) {
        m_cronErrorLabel->setVisible(false);
        m_cronMinField->setText(QStringLiteral("-"));
        m_cronHourField->setText(QStringLiteral("-"));
        m_cronDomField->setText(QStringLiteral("-"));
        m_cronMonthField->setText(QStringLiteral("-"));
        m_cronDowField->setText(QStringLiteral("-"));
        m_cronDescLabel->setText(QStringLiteral("-"));
        m_cronNextTable->setRowCount(0);
        m_cronCopyBtn->setEnabled(false);
        return;
    }

    CronFields cf = parseCron(text);

    if (!cf.valid) {
        m_cronErrorLabel->setText(cf.error);
        m_cronErrorLabel->setVisible(true);
        m_cronMinField->setText(QStringLiteral("-"));
        m_cronHourField->setText(QStringLiteral("-"));
        m_cronDomField->setText(QStringLiteral("-"));
        m_cronMonthField->setText(QStringLiteral("-"));
        m_cronDowField->setText(QStringLiteral("-"));
        m_cronDescLabel->setText(QStringLiteral("-"));
        m_cronNextTable->setRowCount(0);
        m_cronCopyBtn->setEnabled(false);
        return;
    }

    m_cronErrorLabel->setVisible(false);

    m_cronMinField->setText(
        describeFieldValues(cf.minutes, 0, 59, QStringLiteral("分钟")));
    m_cronHourField->setText(
        describeFieldValues(cf.hours, 0, 23, QStringLiteral("小时")));
    m_cronDomField->setText(describeDom(cf.doms));
    m_cronMonthField->setText(describeMonth(cf.months));
    m_cronDowField->setText(describeDow(cf.dows));

    QStringList descParts;
    if (!cf.dows.empty()) descParts.append(describeDow(cf.dows));
    if (!cf.doms.empty()) descParts.append(describeDom(cf.doms));
    if (!cf.months.empty() && cf.months.size() < 12)
        descParts.append(describeMonth(cf.months));
    QString timePart;
    if (!cf.hours.empty() && !cf.minutes.empty()) {
        timePart = describeFieldValues(cf.hours, 0, 23, QStringLiteral("小时"));
        if (!cf.minutes.empty())
            timePart += QStringLiteral(":") + describeFieldValues(cf.minutes, 0, 59,
                                                                    QStringLiteral("分钟"));
    } else if (!cf.hours.empty()) {
        timePart = describeFieldValues(cf.hours, 0, 23, QStringLiteral("小时"));
    } else if (!cf.minutes.empty()) {
        timePart = describeFieldValues(cf.minutes, 0, 59, QStringLiteral("分钟"));
    }
    if (!timePart.isEmpty()) descParts.append(timePart);
    m_cronDescLabel->setText(descParts.isEmpty() ? QStringLiteral("每分钟执行")
                                                  : descParts.join(QStringLiteral(", ")));

    onCronUpdateTimes();
}

void CronTool::onCronPresetChanged(int index)
{
    if (index == 0) return;

    const char *presets[] = {
        "",
        "* * * * *",
        "*/5 * * * *",
        "*/15 * * * *",
        "*/30 * * * *",
        "0 * * * *",
        "0 0 * * *",
        "0 0 * * 1",
        "0 0 1 * *",
        "0 * * * 1-5",
    };
    if (index >= 1 && index <= 9) {
        m_cronInputEdit->setText(QString::fromLatin1(presets[index]));
    }
}

void CronTool::onCronCountChanged(int /*index*/)
{
    onCronUpdateTimes();
}

void CronTool::onCronCopyAll()
{
    QStringList lines;
    for (int row = 0; row < m_cronNextTable->rowCount(); ++row) {
        QTableWidgetItem *idxItem = m_cronNextTable->item(row, 0);
        QTableWidgetItem *timeItem = m_cronNextTable->item(row, 1);
        QTableWidgetItem *relItem = m_cronNextTable->item(row, 2);
        if (idxItem && timeItem && relItem) {
            lines.append(QStringLiteral("%1  %2  %3")
                             .arg(idxItem->text(), timeItem->text(), relItem->text()));
        }
    }

    if (lines.isEmpty()) return;

    QApplication::clipboard()->setText(lines.join('\n'));
    QString original = m_cronCopyBtn->text();
    m_cronCopyBtn->setText(QStringLiteral("已复制"));
    m_cronCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_cronCopyBtn->setText(original);
        m_cronCopyBtn->setEnabled(true);
    });
}

void CronTool::onCronUpdateTimes()
{
    const QString text = m_cronInputEdit->text().trimmed();
    if (text.isEmpty()) {
        m_cronNextTable->setRowCount(0);
        m_cronCopyBtn->setEnabled(false);
        return;
    }

    CronFields cf = parseCron(text);
    if (!cf.valid) {
        m_cronNextTable->setRowCount(0);
        m_cronCopyBtn->setEnabled(false);
        return;
    }

    int count = m_cronCountCombo->currentText().toInt();
    QDateTime now = QDateTime::currentDateTime();
    QVector<QDateTime> times = computeNextTimes(cf, count, now);

    m_cronNextTable->setRowCount(0);
    m_cronNextTable->setRowCount(times.size());

    for (int i = 0; i < times.size(); ++i) {
        QTableWidgetItem *idxItem = new QTableWidgetItem(
            QStringLiteral("#%1").arg(i + 1, 2));
        idxItem->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *timeItem = new QTableWidgetItem(
            times[i].toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

        QTableWidgetItem *relItem = new QTableWidgetItem(
            relativeTimeDesc(times[i], now));

        m_cronNextTable->setItem(i, 0, idxItem);
        m_cronNextTable->setItem(i, 1, timeItem);
        m_cronNextTable->setItem(i, 2, relItem);
    }

    m_cronCopyBtn->setEnabled(times.size() > 0);
}
