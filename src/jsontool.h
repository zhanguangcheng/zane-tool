#ifndef JSONTOOL_H
#define JSONTOOL_H

#include <QObject>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QJsonValue>

class JsonTool : public QObject
{
    Q_OBJECT
public:
    explicit JsonTool(QObject *parent = nullptr);

    QWidget *createPage();

private slots:
    void onFormat();
    void onCompress();
    void onCopyResult();
    void onClear();
    void onInputChanged();
    void onToggleView();

private:
    void updateStatus(const QString &text, bool isError);
    QJsonDocument parseInput();
    void buildTree(const QJsonDocument &doc);
    void showOutput();

    static void addJsonValue(QTreeWidgetItem *parent, const QString &key, const QJsonValue &value);
    static QTreeWidgetItem *createItem(QTreeWidgetItem *parent, const QString &key, const QString &value, const QColor &color);

    QTextEdit *m_inputEdit;
    QStackedWidget *m_outputStack;
    QTreeWidget *m_outputTree;
    QTextEdit *m_outputText;
    QPushButton *m_formatBtn;
    QPushButton *m_compressBtn;
    QPushButton *m_toggleBtn;
    QPushButton *m_copyBtn;
    QPushButton *m_clearBtn;
    QLabel *m_statusLabel;
    QString m_lastFormattedJson;
};

#endif // JSONTOOL_H
