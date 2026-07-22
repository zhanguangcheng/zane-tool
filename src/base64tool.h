#ifndef BASE64TOOL_H
#define BASE64TOOL_H

#include <QObject>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>

class Base64Tool : public QObject
{
    Q_OBJECT
public:
    explicit Base64Tool(QObject *parent = nullptr);

    QWidget *createPage();
    void processFile(const QString &filePath);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSelectFile();
    void onClear();
    void onCopy();

private:
    QString mimeTypeForFile(const QString &filePath) const;

    QLineEdit *m_filePath;
    QPushButton *m_selectBtn;
    QPushButton *m_clearBtn;
    QLabel *m_dropZone;
    QTextEdit *m_output;
    QLabel *m_infoLabel;
    QPushButton *m_copyBtn;
};

#endif // BASE64TOOL_H
