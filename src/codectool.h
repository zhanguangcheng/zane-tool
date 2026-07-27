#ifndef CODECTOOL_H
#define CODECTOOL_H

#include <QObject>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QButtonGroup>
#include <QRadioButton>
#include <QStackedWidget>

class CodecTool : public QObject
{
    Q_OBJECT
public:
    explicit CodecTool(QObject *parent = nullptr);

    QWidget *createPage();

private slots:
    void onDecode();
    void onEncode();
    void onClearDecode();
    void onClearEncode();
    void onCopyDecode();
    void onCopyEncode();
    void onDecodeTypeChanged(int index);

private:
    static QByteArray base64UrlDecode(const QByteArray &input);
    static QByteArray base64UrlEncode(const QByteArray &input);
    static QString phpEncode(const QJsonValue &value);

    QWidget *createDecodeTab();
    QWidget *createEncodeTab();

    QButtonGroup *m_decodeTypeGroup;
    int m_decodeType;
    QTextEdit *m_decodeInput;
    QPushButton *m_decodeBtn;
    QPushButton *m_decodeClearBtn;
    QPushButton *m_decodeCopyBtn;
    QStackedWidget *m_decodeOutputStack;
    QTextEdit *m_decodeTextOutput;
    QTabWidget *m_jwtResultTabs;
    QTextEdit *m_jwtHeaderEdit;
    QTextEdit *m_jwtPayloadEdit;
    QTextEdit *m_jwtSignatureEdit;

    QButtonGroup *m_encodeTypeGroup;
    int m_encodeType;
    QTextEdit *m_encodeInput;
    QPushButton *m_encodeBtn;
    QPushButton *m_encodeClearBtn;
    QPushButton *m_encodeCopyBtn;
    QTextEdit *m_encodeOutput;
};

#endif // CODECTOOL_H
