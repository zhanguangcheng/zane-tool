#ifndef JWTTOOL_H
#define JWTTOOL_H

#include <QObject>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>

class JwtTool : public QObject
{
    Q_OBJECT
public:
    explicit JwtTool(QObject *parent = nullptr);

    QWidget *createPage();

private slots:
    void onParse();
    void onClear();
    void onCopyCurrent();
    void onCopyAll();

private:
    static QByteArray base64UrlDecode(const QByteArray &input);

    QTextEdit *m_inputEdit;
    QPushButton *m_parseBtn;
    QPushButton *m_clearBtn;
    QTabWidget *m_resultTabs;
    QTextEdit *m_headerEdit;
    QTextEdit *m_payloadEdit;
    QTextEdit *m_signatureEdit;
    QLabel *m_infoLabel;
    QPushButton *m_copyCurrentBtn;
    QPushButton *m_copyAllBtn;
};

#endif // JWTTOOL_H
