#ifndef JSONTOOL_H
#define JSONTOOL_H

#include <QObject>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QJsonValue>
#include <QJsonDocument>

#include "xlsxwriter.h"

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
    void onTreeSelectionChanged();
    void onExportExcel();

private:
    void updateStatus(const QString &text, bool isError);
    QJsonDocument parseInput();
    void buildTree(const QJsonDocument &doc);
    void showOutput();
    QJsonValue valueByPath(const QStringList &path) const;

    static void addJsonValue(QTreeWidgetItem *parent, const QString &key, const QJsonValue &value, const QStringList &path);
    static QTreeWidgetItem *createItem(QTreeWidgetItem *parent, const QString &key, const QString &value, const QColor &color, const QStringList &path);
    static XlsxWriter::Cell cellFromJsonValue(const QJsonValue &value);

    QTextEdit *m_inputEdit;
    QStackedWidget *m_outputStack;
    QTreeWidget *m_outputTree;
    QTextEdit *m_outputText;
    QPushButton *m_formatBtn;
    QPushButton *m_compressBtn;
    QPushButton *m_toggleBtn;
    QPushButton *m_copyBtn;
    QPushButton *m_exportBtn;
    QPushButton *m_clearBtn;
    QLabel *m_statusLabel;
    QString m_lastFormattedJson;
    QJsonDocument m_doc;
};

#endif // JSONTOOL_H
