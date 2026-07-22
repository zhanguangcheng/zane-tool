#ifndef CRONTOOL_H
#define CRONTOOL_H

#include <QObject>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QDateTime>

#include <set>
#include <QVector>

class CronTool : public QObject
{
    Q_OBJECT
public:
    explicit CronTool(QObject *parent = nullptr);

    QWidget *createPage();

private slots:
    void onCronInputChanged();
    void onCronPresetChanged(int index);
    void onCronCountChanged(int index);
    void onCronCopyAll();
    void onCronUpdateTimes();

private:
    struct CronFields {
        std::set<int> minutes;
        std::set<int> hours;
        std::set<int> doms;
        std::set<int> months;
        std::set<int> dows;
        bool valid = false;
        QString error;
    };

    static std::set<int> parseCronField(const QString &field, int minVal, int maxVal, bool &ok);
    static CronFields parseCron(const QString &expr);
    static int firstVal(const std::set<int> &s, int defVal);
    static int nextGe(const std::set<int> &s, int val);
    static QVector<QDateTime> computeNextTimes(const CronFields &cf, int count, const QDateTime &from);
    static QString describeFieldValues(const std::set<int> &values, int minVal, int maxVal, const QString &unitSingular);
    static QString describeDow(const std::set<int> &values);
    static QString describeDom(const std::set<int> &values);
    static QString describeMonth(const std::set<int> &values);
    static QString relativeTimeDesc(const QDateTime &target, const QDateTime &now);

    QLineEdit *m_cronInputEdit;
    QComboBox *m_cronPresetCombo;
    QComboBox *m_cronCountCombo;
    QPushButton *m_cronCopyBtn;
    QTableWidget *m_cronNextTable;
    QLabel *m_cronMinField;
    QLabel *m_cronHourField;
    QLabel *m_cronDomField;
    QLabel *m_cronMonthField;
    QLabel *m_cronDowField;
    QLabel *m_cronErrorLabel;
    QLabel *m_cronDescLabel;
    QTimer *m_cronTimer;
};

#endif // CRONTOOL_H
