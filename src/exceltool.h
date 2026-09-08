#ifndef EXCELTOOL_H
#define EXCELTOOL_H

#include <QObject>
#include <QString>
#include <QStringList>

class QWidget;
class QTableWidget;
class QTabWidget;
class QPushButton;
class QLineEdit;
class QListWidget;
class QLabel;

class ExcelTool : public QObject
{
    Q_OBJECT
public:
    explicit ExcelTool(QObject *parent = nullptr);

    QWidget *createPage();
    void addFiles(const QStringList &paths);

private slots:
    void onAddRule();
    void onRemoveRules();
    void onImportRules();
    void onExportRules();
    void onClearRules();
    void onAddFiles();
    void onRemoveFiles();
    void onClearFiles();
    void onChooseOutputDir();
    void onProcess();

private:
    struct HeaderReplace { QString find; QString replace; };
    struct DataReplace { QString find; QString replace; };
    struct ClearRule { QString column; QString value; QStringList clearColumns; };

    void readRules(QList<HeaderReplace> &headers,
                   QList<DataReplace> &data,
                   QList<ClearRule> &clears,
                   QString &warnings) const;
    void loadRulesToTables(const QList<HeaderReplace> &headers,
                           const QList<DataReplace> &data,
                           const QList<ClearRule> &clears);
    bool processOne(const QString &src, const QString &outDir,
                    const QList<HeaderReplace> &headers,
                    const QList<DataReplace> &data,
                    const QList<ClearRule> &clears,
                    QString *outPath, QString *err);
    void updateStatus(const QString &text, bool isError);
    QTableWidget *activeRuleTable() const;
    static QList<int> parseClearColumns(const QString &input, bool *allOk);

    QTabWidget *m_tabWidget;
    QTableWidget *m_headerTable;
    QTableWidget *m_dataTable;
    QTableWidget *m_clearTable;
    QListWidget *m_fileList;
    QLineEdit *m_outputDirEdit;
    QLabel *m_statusLabel;
    bool m_processing = false;
};

#endif // EXCELTOOL_H