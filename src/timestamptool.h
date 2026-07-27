#ifndef TIMESTAMPTOOL_H
#define TIMESTAMPTOOL_H

#include <QObject>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDateTimeEdit>

class QRadioButton;

class TimestampTool : public QObject
{
    Q_OBJECT
public:
    explicit TimestampTool(QObject *parent = nullptr);

    QWidget *createPage();

    void startTimer();
    void stopTimer();

private slots:
    void onTimestampUpdate();
    void onTimestampNowSecCopy();
    void onTimestampInputChanged();
    void onTimestampResultCopy();
    void onDatetimeInputChanged();
    void onDatetimeSecCopy();

private:
    QTimer *m_timestampTimer;
    QLabel *m_timestampNowLabel;
    QLineEdit *m_timestampNowSecEdit;
    QPushButton *m_timestampNowSecCopyBtn;
    QLineEdit *m_timestampInputEdit;
    QRadioButton *m_timestampSecRadio;
    QRadioButton *m_timestampMsRadio;
    QLabel *m_timestampResultLabel;
    QPushButton *m_timestampResultCopyBtn;
    QDateTimeEdit *m_datetimeInputEdit;
    QLineEdit *m_datetimeSecResultEdit;
    QPushButton *m_datetimeSecCopyBtn;
};

#endif // TIMESTAMPTOOL_H
