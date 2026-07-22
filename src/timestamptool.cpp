#include <QApplication>
#include <QClipboard>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QDateTime>

#include "timestamptool.h"

TimestampTool::TimestampTool(QObject *parent)
    : QObject(parent)
    , m_timestampTimer(nullptr)
    , m_timestampNowLabel(nullptr)
    , m_timestampNowSecEdit(nullptr)
    , m_timestampNowSecCopyBtn(nullptr)
    , m_timestampInputEdit(nullptr)
    , m_timestampSecRadio(nullptr)
    , m_timestampMsRadio(nullptr)
    , m_timestampResultLabel(nullptr)
    , m_timestampResultCopyBtn(nullptr)
    , m_datetimeInputEdit(nullptr)
    , m_datetimeSecResultEdit(nullptr)
    , m_datetimeSecCopyBtn(nullptr)
{
}

QWidget *TimestampTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *nowGroup = new QGroupBox(QStringLiteral("当前时间戳"), page);
    QVBoxLayout *nowLayout = new QVBoxLayout(nowGroup);
    nowLayout->setSpacing(10);

    QHBoxLayout *secRow = new QHBoxLayout();
    secRow->setSpacing(8);
    QLabel *secLabel = new QLabel(QStringLiteral("秒级:"), nowGroup);
    secLabel->setFixedWidth(60);
    m_timestampNowSecEdit = new QLineEdit(nowGroup);
    m_timestampNowSecEdit->setReadOnly(true);
    m_timestampNowSecEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    m_timestampNowSecCopyBtn = new QPushButton(QStringLiteral("复制"), nowGroup);
    m_timestampNowSecCopyBtn->setFixedHeight(30);
    m_timestampNowSecCopyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_timestampNowSecCopyBtn, &QPushButton::clicked,
            this, &TimestampTool::onTimestampNowSecCopy);
    secRow->addWidget(secLabel);
    secRow->addWidget(m_timestampNowSecEdit, 1);
    secRow->addWidget(m_timestampNowSecCopyBtn);

    QHBoxLayout *localRow = new QHBoxLayout();
    localRow->setSpacing(8);
    QLabel *localLabel = new QLabel(QStringLiteral("本地时间:"), nowGroup);
    localLabel->setFixedWidth(60);
    m_timestampNowLabel = new QLabel(nowGroup);
    m_timestampNowLabel->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace; "
                       "font-size: 14px; font-weight: bold;"));
    localRow->addWidget(localLabel);
    localRow->addWidget(m_timestampNowLabel, 1);

    nowLayout->addLayout(secRow);
    nowLayout->addLayout(localRow);

    QGroupBox *tsToDtGroup = new QGroupBox(QStringLiteral("时间戳 → 日期时间"), page);
    QVBoxLayout *tsToDtLayout = new QVBoxLayout(tsToDtGroup);
    tsToDtLayout->setSpacing(10);

    QHBoxLayout *inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);
    m_timestampInputEdit = new QLineEdit(tsToDtGroup);
    m_timestampInputEdit->setPlaceholderText(QStringLiteral("输入时间戳..."));
    m_timestampInputEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    m_timestampInputEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("-?\\d+")),
                                        m_timestampInputEdit));
    connect(m_timestampInputEdit, &QLineEdit::textChanged,
            this, &TimestampTool::onTimestampInputChanged);

    m_timestampSecRadio = new QRadioButton(QStringLiteral("秒"), tsToDtGroup);
    m_timestampMsRadio = new QRadioButton(QStringLiteral("毫秒"), tsToDtGroup);
    m_timestampSecRadio->setChecked(true);
    QButtonGroup *unitGroup = new QButtonGroup(page);
    unitGroup->addButton(m_timestampSecRadio);
    unitGroup->addButton(m_timestampMsRadio);
    connect(unitGroup, &QButtonGroup::buttonClicked,
            this, [this](QAbstractButton *) { onTimestampInputChanged(); });

    inputRow->addWidget(m_timestampInputEdit, 1);
    inputRow->addWidget(m_timestampSecRadio);
    inputRow->addWidget(m_timestampMsRadio);

    QHBoxLayout *tsResultRow = new QHBoxLayout();
    tsResultRow->setSpacing(8);
    QLabel *tsResultLabel = new QLabel(QStringLiteral("结果:"), tsToDtGroup);
    tsResultLabel->setFixedWidth(60);
    m_timestampResultLabel = new QLabel(QStringLiteral("-"), tsToDtGroup);
    m_timestampResultLabel->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace; "
                       "font-size: 14px; font-weight: bold;"));
    m_timestampResultCopyBtn = new QPushButton(QStringLiteral("复制"), tsToDtGroup);
    m_timestampResultCopyBtn->setFixedHeight(30);
    m_timestampResultCopyBtn->setCursor(Qt::PointingHandCursor);
    m_timestampResultCopyBtn->setEnabled(false);
    connect(m_timestampResultCopyBtn, &QPushButton::clicked,
            this, &TimestampTool::onTimestampResultCopy);
    tsResultRow->addWidget(tsResultLabel);
    tsResultRow->addWidget(m_timestampResultLabel, 1);
    tsResultRow->addWidget(m_timestampResultCopyBtn);

    tsToDtLayout->addLayout(inputRow);
    tsToDtLayout->addLayout(tsResultRow);

    QGroupBox *dtToTsGroup = new QGroupBox(QStringLiteral("日期时间 → 时间戳"), page);
    QVBoxLayout *dtToTsLayout = new QVBoxLayout(dtToTsGroup);
    dtToTsLayout->setSpacing(10);

    m_datetimeInputEdit = new QDateTimeEdit(QDateTime::currentDateTime(), dtToTsGroup);
    m_datetimeInputEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_datetimeInputEdit->setCalendarPopup(true);
    connect(m_datetimeInputEdit, &QDateTimeEdit::dateTimeChanged,
            this, &TimestampTool::onDatetimeInputChanged);

    QHBoxLayout *dtSecRow = new QHBoxLayout();
    dtSecRow->setSpacing(8);
    QLabel *dtSecLabel = new QLabel(QStringLiteral("秒级:"), dtToTsGroup);
    dtSecLabel->setFixedWidth(60);
    m_datetimeSecResultEdit = new QLineEdit(dtToTsGroup);
    m_datetimeSecResultEdit->setReadOnly(true);
    m_datetimeSecResultEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    m_datetimeSecCopyBtn = new QPushButton(QStringLiteral("复制"), dtToTsGroup);
    m_datetimeSecCopyBtn->setFixedHeight(30);
    m_datetimeSecCopyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_datetimeSecCopyBtn, &QPushButton::clicked,
            this, &TimestampTool::onDatetimeSecCopy);
    dtSecRow->addWidget(dtSecLabel);
    dtSecRow->addWidget(m_datetimeSecResultEdit, 1);
    dtSecRow->addWidget(m_datetimeSecCopyBtn);

    dtToTsLayout->addWidget(m_datetimeInputEdit);
    dtToTsLayout->addLayout(dtSecRow);

    mainLayout->addWidget(nowGroup);
    mainLayout->addWidget(tsToDtGroup);
    mainLayout->addWidget(dtToTsGroup);
    mainLayout->addStretch();

    m_timestampTimer = new QTimer(this);
    connect(m_timestampTimer, &QTimer::timeout,
            this, &TimestampTool::onTimestampUpdate);
    m_timestampTimer->start(200);

    onTimestampUpdate();
    onDatetimeInputChanged();

    return page;
}

void TimestampTool::onTimestampUpdate()
{
    QDateTime now = QDateTime::currentDateTime();
    m_timestampNowSecEdit->setText(QString::number(now.toSecsSinceEpoch()));
    m_timestampNowLabel->setText(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

void TimestampTool::onTimestampNowSecCopy()
{
    QString text = m_timestampNowSecEdit->text();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_timestampNowSecCopyBtn->text();
    m_timestampNowSecCopyBtn->setText(QStringLiteral("已复制"));
    m_timestampNowSecCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_timestampNowSecCopyBtn->setText(original);
        m_timestampNowSecCopyBtn->setEnabled(true);
    });
}

void TimestampTool::onTimestampInputChanged()
{
    QString text = m_timestampInputEdit->text();
    if (text.isEmpty()) {
        m_timestampResultLabel->setText(QStringLiteral("-"));
        m_timestampResultCopyBtn->setEnabled(false);
        return;
    }

    bool ok;
    qint64 ts = text.toLongLong(&ok);
    if (!ok) {
        m_timestampResultLabel->setText(QStringLiteral("无效的时间戳"));
        m_timestampResultCopyBtn->setEnabled(false);
        return;
    }

    QDateTime dt;
    if (m_timestampMsRadio->isChecked()) {
        dt = QDateTime::fromMSecsSinceEpoch(ts);
    } else {
        dt = QDateTime::fromSecsSinceEpoch(ts);
    }

    if (!dt.isValid()) {
        m_timestampResultLabel->setText(QStringLiteral("无效的时间戳"));
        m_timestampResultCopyBtn->setEnabled(false);
        return;
    }

    m_timestampResultLabel->setText(dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    m_timestampResultCopyBtn->setEnabled(true);
}

void TimestampTool::onTimestampResultCopy()
{
    QString text = m_timestampResultLabel->text();
    if (text.isEmpty() || text == QStringLiteral("-"))
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_timestampResultCopyBtn->text();
    m_timestampResultCopyBtn->setText(QStringLiteral("已复制"));
    m_timestampResultCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_timestampResultCopyBtn->setText(original);
        m_timestampResultCopyBtn->setEnabled(true);
    });
}

void TimestampTool::onDatetimeInputChanged()
{
    QDateTime dt = m_datetimeInputEdit->dateTime();
    m_datetimeSecResultEdit->setText(QString::number(dt.toSecsSinceEpoch()));
}

void TimestampTool::onDatetimeSecCopy()
{
    QString text = m_datetimeSecResultEdit->text();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_datetimeSecCopyBtn->text();
    m_datetimeSecCopyBtn->setText(QStringLiteral("已复制"));
    m_datetimeSecCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_datetimeSecCopyBtn->setText(original);
        m_datetimeSecCopyBtn->setEnabled(true);
    });
}
