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

namespace Xlsx { struct Sheet; }

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
    void onShowSyntaxHelp();
    void onAddFiles();
    void onRemoveFiles();
    void onClearFiles();
    void onChooseOutputDir();
    void onProcess();

private:
    struct HeaderReplace { QString find; QString replace; };
    struct DataReplace { QString find; QString replace; };
    struct ClearCondition
    {
        enum class Op { Eq, Ne, Empty, NotEmpty };
        QString column;
        Op op = Op::Eq;
        QString value;
    };
    struct ClearRule
    {
        enum class Action { Clear, DeleteRow };
        Action action = Action::Clear;
        QString column;                  // 主条件列
        QString value;                   // 主条件等于值
        QList<QList<ClearCondition>> extraGroups;  // 附加条件：每组内 OR，组间 AND
        QStringList clearColumns;        // 仅 action==Clear 时使用
    };

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
    static bool parseExtraConditions(const QString &text,
                                     QList<QList<ClearCondition>> *groups,
                                     QString *err);
    static QString serializeExtraConditions(const QList<QList<ClearCondition>> &groups);
    static bool evalExtraCondition(const Xlsx::Sheet &sheet, int row, const ClearCondition &cond);

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