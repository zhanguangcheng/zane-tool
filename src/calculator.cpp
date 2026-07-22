#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

#include "calculator.h"

CalculatorPage::CalculatorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void CalculatorPage::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *exprGroup = new QGroupBox(QStringLiteral("\u8BA1\u7B97\u8868\u8FBE\u5F0F"), this);
    QVBoxLayout *exprLayout = new QVBoxLayout(exprGroup);
    exprLayout->setSpacing(10);

    QHBoxLayout *inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);

    m_input = new QLineEdit(exprGroup);
    m_input->setPlaceholderText(QStringLiteral("\u8F93\u5165\u8868\u8FBE\u5F0F\uFF0C\u4F8B\u5982: (1+2)*3.5"));
    m_input->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 16px;"
        "}"));
    m_input->setClearButtonEnabled(true);
    QRegularExpression validChars(QStringLiteral("[0-9+\\-*/.() ]*"));
    m_input->setValidator(new QRegularExpressionValidator(validChars, m_input));
    connect(m_input, &QLineEdit::returnPressed, this, &CalculatorPage::onEvaluate);

    m_evalBtn = new QPushButton(QStringLiteral("= \u8BA1\u7B97"), exprGroup);
    m_evalBtn->setCursor(Qt::PointingHandCursor);
    m_evalBtn->setFixedHeight(34);
    m_evalBtn->setFixedWidth(90);
    connect(m_evalBtn, &QPushButton::clicked, this, &CalculatorPage::onEvaluate);

    m_clearBtn = new QPushButton(QStringLiteral("\u6E05\u7A7A"), exprGroup);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setFixedHeight(34);
    m_clearBtn->setFixedWidth(72);
    connect(m_clearBtn, &QPushButton::clicked, this, &CalculatorPage::onClear);

    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_evalBtn);
    inputRow->addWidget(m_clearBtn);

    QHBoxLayout *resultRow = new QHBoxLayout();
    resultRow->setSpacing(8);

    m_result = new QLineEdit(exprGroup);
    m_result->setReadOnly(true);
    m_result->setPlaceholderText(QStringLiteral("\u7ED3\u679C"));
    m_result->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  background-color: #f0f4ff;"
        "  border-color: #a0c4ff;"
        "}"));

    m_copyBtn = new QPushButton(QStringLiteral("\u590D\u5236\u7ED3\u679C"), exprGroup);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setFixedHeight(34);
    m_copyBtn->setFixedWidth(90);
    m_copyBtn->setEnabled(false);
    connect(m_copyBtn, &QPushButton::clicked, this, &CalculatorPage::onCopyResult);

    resultRow->addWidget(m_result, 1);
    resultRow->addWidget(m_copyBtn);

    exprLayout->addLayout(inputRow);
    exprLayout->addLayout(resultRow);

    QGroupBox *historyGroup = new QGroupBox(QStringLiteral("\u5386\u53F2\u8BB0\u5F55"), this);
    QVBoxLayout *historyLayout = new QVBoxLayout(historyGroup);
    historyLayout->setSpacing(8);

    m_history = new QListWidget(historyGroup);
    m_history->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 13px;"
        "  min-height: 120px;"
        "}"));
    connect(m_history, &QListWidget::itemClicked, this, &CalculatorPage::onHistoryItemClicked);

    m_clearHistoryBtn = new QPushButton(QStringLiteral("\u6E05\u7A7A\u5386\u53F2"), historyGroup);
    m_clearHistoryBtn->setCursor(Qt::PointingHandCursor);
    m_clearHistoryBtn->setFixedWidth(96);
    m_clearHistoryBtn->setEnabled(false);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, &CalculatorPage::onClearHistory);

    historyLayout->addWidget(m_history);
    historyLayout->addWidget(m_clearHistoryBtn, 0, Qt::AlignRight);

    mainLayout->addWidget(exprGroup);
    mainLayout->addWidget(historyGroup, 1);
}

void CalculatorPage::onEvaluate()
{
    QString expr = m_input->text().trimmed();
    if (expr.isEmpty()) {
        m_result->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 16px;"
            "  font-weight: bold;"
            "  background-color: #f0f4ff;"
            "  border-color: #a0c4ff;"
            "  color: #dc3545;"
            "}"));
        m_result->setText(QStringLiteral("\u8BF7\u8F93\u5165\u8868\u8FBE\u5F0F"));
        m_copyBtn->setEnabled(false);
        return;
    }

    QList<QPair<int, double>> tokens;
    if (!tokenize(expr, tokens)) {
        m_result->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 16px;"
            "  font-weight: bold;"
            "  background-color: #fff0f0;"
            "  border-color: #f5c6cb;"
            "  color: #dc3545;"
            "}"));
        m_result->setText(QStringLiteral("\u975E\u6CD5\u5B57\u7B26"));
        m_copyBtn->setEnabled(false);
        return;
    }

    int pos = 0;
    double result;
    try {
        result = parseExpression(tokens, pos);
        if (pos < tokens.size() - 1) {
            throw std::runtime_error("unexpected token");
        }
    } catch (const std::exception &) {
        m_result->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 16px;"
            "  font-weight: bold;"
            "  background-color: #fff0f0;"
            "  border-color: #f5c6cb;"
            "  color: #dc3545;"
            "}"));
        m_result->setText(QStringLiteral("\u8868\u8FBE\u5F0F\u683C\u5F0F\u9519\u8BEF"));
        m_copyBtn->setEnabled(false);
        return;
    }

    m_result->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  background-color: #f0f4ff;"
        "  border-color: #a0c4ff;"
        "  color: #212529;"
        "}"));

    QString resultStr = QString::number(result, 'g', 15);
    m_result->setText(QStringLiteral("= ") + resultStr);
    m_copyBtn->setEnabled(true);

    QString historyEntry = expr + QStringLiteral(" = ") + resultStr;
    m_historyData.prepend(historyEntry);
    QListWidgetItem *item = new QListWidgetItem(historyEntry, m_history);
    item->setData(Qt::UserRole, expr);
    m_history->insertItem(0, item);
    m_clearHistoryBtn->setEnabled(true);

    if (m_historyData.size() > 50) {
        m_historyData.removeLast();
        delete m_history->takeItem(m_history->count() - 1);
    }

    emit statusMessage(QStringLiteral("\u5DF2\u8BA1\u7B97"));
}

void CalculatorPage::onClear()
{
    m_input->clear();
    m_result->clear();
    m_result->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  background-color: #f0f4ff;"
        "  border-color: #a0c4ff;"
        "  color: #212529;"
        "}"));
    m_copyBtn->setEnabled(false);
    m_input->setFocus();
}

void CalculatorPage::onCopyResult()
{
    QString text = m_result->text();
    if (text.isEmpty())
        return;

    QString resultText = text.mid(text.indexOf(QStringLiteral("= ")) + 2);
    QApplication::clipboard()->setText(resultText);
    QString original = m_copyBtn->text();
    m_copyBtn->setText(QStringLiteral("\u5DF2\u590D\u5236"));
    m_copyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyBtn->setText(original);
        m_copyBtn->setEnabled(true);
    });
}

void CalculatorPage::onClearHistory()
{
    m_history->clear();
    m_historyData.clear();
    m_clearHistoryBtn->setEnabled(false);
}

void CalculatorPage::onHistoryItemClicked(QListWidgetItem *item)
{
    QString expr = item->data(Qt::UserRole).toString();
    if (!expr.isEmpty()) {
        m_input->setText(expr);
        m_input->setFocus();
    }
}

void CalculatorPage::onInputTextChanged(const QString &)
{
}

bool CalculatorPage::tokenize(const QString &expr, QList<QPair<int, double>> &tokens)
{
    tokens.clear();

    for (int i = 0; i < expr.size(); ++i) {
        QChar c = expr.at(i);

        if (c.isSpace())
            continue;

        if (c == '+') {
            tokens.append({Plus, 0.0});
        } else if (c == '-') {
            tokens.append({Minus, 0.0});
        } else if (c == '*') {
            tokens.append({Mul, 0.0});
        } else if (c == '/') {
            tokens.append({Div, 0.0});
        } else if (c == '(') {
            tokens.append({LParen, 0.0});
        } else if (c == ')') {
            tokens.append({RParen, 0.0});
        } else if (c.isDigit() || c == '.') {
            int start = i;
            bool hasDot = (c == '.');
            while (i + 1 < expr.size()) {
                QChar nxt = expr.at(i + 1);
                if (nxt.isDigit()) {
                    ++i;
                } else if (nxt == '.' && !hasDot) {
                    hasDot = true;
                    ++i;
                } else {
                    break;
                }
            }
            bool ok = false;
            double val = QStringView(expr).mid(start, i - start + 1).toDouble(&ok);
            if (!ok)
                return false;
            tokens.append({Number, val});
        } else {
            return false;
        }
    }

    tokens.append({End, 0.0});
    return true;
}

double CalculatorPage::parseExpression(const QList<QPair<int, double>> &tokens, int &pos)
{
    double result = parseTerm(tokens, pos);

    while (tokens[pos].first == Plus || tokens[pos].first == Minus) {
        int op = tokens[pos].first;
        ++pos;
        double rhs = parseTerm(tokens, pos);
        if (op == Plus)
            result += rhs;
        else
            result -= rhs;
    }

    return result;
}

double CalculatorPage::parseTerm(const QList<QPair<int, double>> &tokens, int &pos)
{
    double result = parseFactor(tokens, pos);

    while (tokens[pos].first == Mul || tokens[pos].first == Div) {
        int op = tokens[pos].first;
        ++pos;
        double rhs = parseFactor(tokens, pos);
        if (op == Mul) {
            result *= rhs;
        } else {
            if (rhs == 0.0)
                throw std::runtime_error("division by zero");
            result /= rhs;
        }
    }

    return result;
}

double CalculatorPage::parseFactor(const QList<QPair<int, double>> &tokens, int &pos)
{
    if (tokens[pos].first == Minus) {
        ++pos;
        return -parseFactor(tokens, pos);
    }

    if (tokens[pos].first == LParen) {
        ++pos;
        double result = parseExpression(tokens, pos);
        if (tokens[pos].first != RParen)
            throw std::runtime_error("missing )");
        ++pos;
        return result;
    }

    if (tokens[pos].first == Number) {
        double val = tokens[pos].second;
        ++pos;
        return val;
    }

    throw std::runtime_error("unexpected token");
}
