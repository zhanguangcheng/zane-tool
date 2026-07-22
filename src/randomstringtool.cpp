#include <QApplication>
#include <QClipboard>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <QRandomGenerator>

#include "randomstringtool.h"

RandomStringTool::RandomStringTool(QObject *parent)
    : QObject(parent)
    , m_upperCheck(nullptr)
    , m_lowerCheck(nullptr)
    , m_digitCheck(nullptr)
    , m_symbolCheck(nullptr)
    , m_excludeEdit(nullptr)
    , m_lengthSpin(nullptr)
    , m_countSpin(nullptr)
    , m_output(nullptr)
    , m_generateBtn(nullptr)
    , m_copyBtn(nullptr)
{
}

QWidget *RandomStringTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *charGroup = new QGroupBox(QStringLiteral("字符集合"), page);
    QVBoxLayout *charLayout = new QVBoxLayout(charGroup);
    charLayout->setSpacing(10);

    QGridLayout *checkGrid = new QGridLayout();
    checkGrid->setSpacing(8);
    checkGrid->setColumnStretch(1, 1);
    checkGrid->setColumnStretch(3, 1);

    m_upperCheck = new QCheckBox(QStringLiteral("A-Z 大写字母"), charGroup);
    m_upperCheck->setChecked(true);
    m_lowerCheck = new QCheckBox(QStringLiteral("a-z 小写字母"), charGroup);
    m_lowerCheck->setChecked(true);
    m_digitCheck = new QCheckBox(QStringLiteral("0-9 数字"), charGroup);
    m_digitCheck->setChecked(true);
    m_symbolCheck = new QCheckBox(QStringLiteral("!@#$ 特殊符号"), charGroup);

    checkGrid->addWidget(m_upperCheck, 0, 0);
    checkGrid->addWidget(m_lowerCheck, 0, 2);
    checkGrid->addWidget(m_digitCheck, 1, 0);
    checkGrid->addWidget(m_symbolCheck, 1, 2);

    QHBoxLayout *excludeRow = new QHBoxLayout();
    excludeRow->setSpacing(8);
    QLabel *excludeLabel = new QLabel(QStringLiteral("排除字符:"), charGroup);
    m_excludeEdit = new QLineEdit(charGroup);
    m_excludeEdit->setPlaceholderText(QStringLiteral("例如: 0O1lI  (这些字符不会出现在结果中)"));
    m_excludeEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    excludeRow->addWidget(excludeLabel);
    excludeRow->addWidget(m_excludeEdit, 1);

    charLayout->addLayout(checkGrid);
    charLayout->addLayout(excludeRow);

    QGroupBox *configGroup = new QGroupBox(QStringLiteral("生成设置"), page);
    QHBoxLayout *configLayout = new QHBoxLayout(configGroup);
    configLayout->setSpacing(20);

    QHBoxLayout *lengthRow = new QHBoxLayout();
    lengthRow->setSpacing(6);
    QLabel *lengthLabel = new QLabel(QStringLiteral("字符串长度:"), configGroup);
    m_lengthSpin = new QSpinBox(configGroup);
    m_lengthSpin->setRange(1, 256);
    m_lengthSpin->setValue(16);
    m_lengthSpin->setFixedWidth(80);
    lengthRow->addWidget(lengthLabel);
    lengthRow->addWidget(m_lengthSpin);

    QHBoxLayout *countRow = new QHBoxLayout();
    countRow->setSpacing(6);
    QLabel *countLabel = new QLabel(QStringLiteral("生成数量:"), configGroup);
    m_countSpin = new QSpinBox(configGroup);
    m_countSpin->setRange(1, 1000);
    m_countSpin->setValue(10);
    m_countSpin->setFixedWidth(80);
    countRow->addWidget(countLabel);
    countRow->addWidget(m_countSpin);

    m_generateBtn = new QPushButton(QStringLiteral("生成随机字符串"), configGroup);
    m_generateBtn->setCursor(Qt::PointingHandCursor);
    m_generateBtn->setFixedHeight(32);
    connect(m_generateBtn, &QPushButton::clicked, this, &RandomStringTool::onGenerate);

    m_copyBtn = new QPushButton(QStringLiteral("复制全部"), configGroup);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setFixedHeight(32);
    m_copyBtn->setFixedWidth(100);
    m_copyBtn->setEnabled(false);
    connect(m_copyBtn, &QPushButton::clicked, this, &RandomStringTool::onCopy);

    configLayout->addLayout(lengthRow);
    configLayout->addLayout(countRow);
    configLayout->addStretch();
    configLayout->addWidget(m_generateBtn);
    configLayout->addWidget(m_copyBtn);

    QGroupBox *resultGroup = new QGroupBox(QStringLiteral("结果"), page);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setSpacing(0);

    m_output = new QTextEdit(resultGroup);
    m_output->setReadOnly(true);
    m_output->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  border: 1px solid #ced4da;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  background-color: #ffffff;"
        "  color: #212529;"
        "}"
        "QTextEdit:focus { border-color: #86b7fe; }"));
    m_output->setMinimumHeight(200);
    m_output->setPlaceholderText(QStringLiteral("点击「生成随机字符串」按钮生成结果"));

    resultLayout->addWidget(m_output);

    mainLayout->addWidget(charGroup);
    mainLayout->addWidget(configGroup);
    mainLayout->addWidget(resultGroup, 1);

    return page;
}

void RandomStringTool::onGenerate()
{
    QString chars;
    if (m_upperCheck->isChecked())
        chars += QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (m_lowerCheck->isChecked())
        chars += QStringLiteral("abcdefghijklmnopqrstuvwxyz");
    if (m_digitCheck->isChecked())
        chars += QStringLiteral("0123456789");
    if (m_symbolCheck->isChecked())
        chars += QStringLiteral("!@#$%^&*()-_=+[]{};:'\",.<>?/\\|`~");

    QString excludes = m_excludeEdit->text();
    for (const QChar &ch : excludes)
        chars.remove(ch);

    if (chars.isEmpty()) {
        m_output->setPlainText(QStringLiteral("请至少选择一种字符类型"));
        m_copyBtn->setEnabled(false);
        return;
    }

    int length = m_lengthSpin->value();
    int count = m_countSpin->value();
    QRandomGenerator *rng = QRandomGenerator::global();

    QStringList results;
    results.reserve(count);
    int charCount = chars.size();
    for (int i = 0; i < count; ++i) {
        QString str;
        str.reserve(length);
        for (int j = 0; j < length; ++j)
            str.append(chars.at(rng->bounded(charCount)));
        results.append(str);
    }

    m_output->setPlainText(results.join(QStringLiteral("\n")));
    m_copyBtn->setEnabled(true);
}

void RandomStringTool::onCopy()
{
    QString text = m_output->toPlainText();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_copyBtn->text();
    m_copyBtn->setText(QStringLiteral("已复制"));
    m_copyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyBtn->setText(original);
        m_copyBtn->setEnabled(true);
    });
}
