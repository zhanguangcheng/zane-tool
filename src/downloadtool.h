#ifndef DOWNLOADTOOL_H
#define DOWNLOADTOOL_H

#include <QWidget>
#include <QProcess>
#include <QList>
#include <QMap>

#include <QTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>

struct DownloadEntry {
    QString url;
    QString filename;
    int row;
    bool completed;
    bool failed;
};

class DownloadTool : public QWidget
{
    Q_OBJECT
public:
    explicit DownloadTool(const QString &aria2Path, QWidget *parent = nullptr);

    QProcess *process() const { return m_downloadProcess; }

private slots:
    void onDownloadAddFile();
    void onDownloadPaste();
    void onDownloadClear();
    void onDownloadOutputBrowse();
    void onDownloadStart();
    void onDownloadCancel();
    void onDownloadProcessOutput();

private:
    void setDownloadUiEnabled(bool enabled);

    QString m_aria2Path;

    QTextEdit *m_downloadUrlInput;
    QPushButton *m_downloadAddFileBtn;
    QPushButton *m_downloadPasteBtn;
    QPushButton *m_downloadClearBtn;
    QSpinBox *m_downloadMaxConcurrent;
    QSpinBox *m_downloadMaxConnections;
    QSpinBox *m_downloadSpeedLimit;
    QCheckBox *m_downloadAllowOverwrite;
    QLineEdit *m_downloadOutputDir;
    QPushButton *m_downloadOutputBrowse;
    QTableWidget *m_downloadProgressTable;
    QProgressBar *m_downloadProgressBar;
    QLabel *m_downloadStatusLabel;
    QPushButton *m_downloadStartBtn;
    QPushButton *m_downloadCancelBtn;

    QProcess *m_downloadProcess;
    QList<DownloadEntry> m_downloadEntries;
    QList<QProgressBar *> m_downloadBars;
    QMap<QString, int> m_gidToIndex;
    QString m_downloadPendingUrl;
    int m_downloadCompleted;
    int m_downloadFailed;
    bool m_downloadCancelling;
    QString m_downloadStdoutBuffer;
    QString m_downloadTempFile;
    QString m_downloadOutputDirPath;
};

#endif // DOWNLOADTOOL_H
