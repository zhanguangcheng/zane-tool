#ifndef RANDOMSTRINGTOOL_H
#define RANDOMSTRINGTOOL_H

#include <QObject>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QPushButton>

class RandomStringTool : public QObject
{
    Q_OBJECT
public:
    explicit RandomStringTool(QObject *parent = nullptr);

    QWidget *createPage();

private slots:
    void onGenerate();
    void onCopy();

private:
    QCheckBox *m_upperCheck;
    QCheckBox *m_lowerCheck;
    QCheckBox *m_digitCheck;
    QCheckBox *m_symbolCheck;
    QLineEdit *m_excludeEdit;
    QSpinBox *m_lengthSpin;
    QSpinBox *m_countSpin;
    QTextEdit *m_output;
    QPushButton *m_generateBtn;
    QPushButton *m_copyBtn;
};

#endif // RANDOMSTRINGTOOL_H
