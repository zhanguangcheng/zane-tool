#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>

class CalculatorPage : public QWidget
{
    Q_OBJECT

public:
    explicit CalculatorPage(QWidget *parent = nullptr);

signals:
    void statusMessage(const QString &msg);

private slots:
    void onEvaluate();
    void onClear();
    void onCopyResult();
    void onClearHistory();
    void onHistoryItemClicked(QListWidgetItem *item);
    void onInputTextChanged(const QString &text);

private:
    void setupUi();

    static bool tokenize(const QString &expr, QList<QPair<int, double>> &tokens);
    static double parseExpression(const QList<QPair<int, double>> &tokens, int &pos);
    static double parseTerm(const QList<QPair<int, double>> &tokens, int &pos);
    static double parseFactor(const QList<QPair<int, double>> &tokens, int &pos);

    enum TokenType {
        Number = 0, Plus, Minus, Mul, Div, LParen, RParen, End
    };

    QLineEdit *m_input;
    QLineEdit *m_result;
    QPushButton *m_evalBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_copyBtn;
    QPushButton *m_clearHistoryBtn;
    QListWidget *m_history;
    QStringList m_historyData;
};

#endif // CALCULATOR_H
