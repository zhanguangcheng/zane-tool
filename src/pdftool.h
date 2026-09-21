#ifndef PDFTOOL_H
#define PDFTOOL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

#include <functional>

class QWidget;
class QTabWidget;
class QListWidget;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QTextEdit;
class QRadioButton;

class PdfTool : public QObject
{
    Q_OBJECT
public:
    explicit PdfTool(const QString &qpdfPath, QObject *parent = nullptr);

    QWidget *createPage();
    void addFiles(const QStringList &paths);

private slots:
    void onMergeAdd();
    void onMergeRemove();
    void onMergeClear();
    void onMergeMoveUp();
    void onMergeMoveDown();
    void onMergeRun();

    void onSplitBrowseIn();
    void onSplitBrowseOut();
    void onSplitModeChanged();
    void onSplitRun();

    void onOrgBrowseIn();
    void onOrgBrowseOut();
    void onOrgRun();

    void onEncBrowseIn();
    void onEncBrowseOut();
    void onEncModeChanged();
    void onEncRun();

    void onOptBrowseIn();
    void onOptBrowseOut();
    void onOptRun();

    void onInfoBrowse();
    void onInfoRead();
    void onInfoCopy();

    void onStop();

private:
    void buildMergeTab(QTabWidget *tabs);
    void buildSplitTab(QTabWidget *tabs);
    void buildOrganizeTab(QTabWidget *tabs);
    void buildEncryptTab(QTabWidget *tabs);
    void buildOptimizeTab(QTabWidget *tabs);
    void buildInfoTab(QTabWidget *tabs);

    void registerActionButton(QPushButton *btn);
    void setBusy(bool busy);
    void log(const QString &text);
    void setStatus(const QString &text, bool error = false);

    void runQpdf(const QStringList &args, const QString &title,
                 const std::function<void(bool, const QString &, const QString &)> &onDone);
    bool runQpdfSync(const QStringList &args, QString *out, QString *err, int timeoutMs = 30000) const;

    QString pickOpenPdf(QLineEdit *edit, const QString &title);
    QString pickSavePdf(QLineEdit *edit, const QString &defaultName);
    QString pickDirectory(QLineEdit *edit, const QString &title);
    static QString defaultOutPath(const QString &input, const QString &suffix);
    static QString safeOutput(const QString &input, const QString &out);
    static QString maskPassword(const QStringList &args);
    void showResult(const QString &title, const QString &text, bool error);

    QString m_qpdfPath;
    bool m_available = false;
    QWidget *m_page = nullptr;
    QTabWidget *m_tabs = nullptr;
    QProcess *m_process = nullptr;
    bool m_running = false;
    bool m_stopping = false;
    QList<QPushButton *> m_actionButtons;
    QPushButton *m_stopBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTextEdit *m_log = nullptr;

    // 合并
    QListWidget *m_mergeList = nullptr;
    QLineEdit *m_mergeOut = nullptr;
    QLineEdit *m_mergePassword = nullptr;

    // 拆分 / 提取
    QLineEdit *m_splitIn = nullptr;
    QLineEdit *m_splitRange = nullptr;
    QLineEdit *m_splitOut = nullptr;
    QLineEdit *m_splitPassword = nullptr;
    QComboBox *m_splitMode = nullptr;
    QSpinBox *m_splitEvery = nullptr;
    QLabel *m_splitOutLabel = nullptr;

    // 页面编排
    QLineEdit *m_orgIn = nullptr;
    QLineEdit *m_orgOrder = nullptr;
    QLineEdit *m_orgRotRange = nullptr;
    QLineEdit *m_orgOut = nullptr;
    QLineEdit *m_orgPassword = nullptr;
    QCheckBox *m_orgRotate = nullptr;
    QComboBox *m_orgAngle = nullptr;

    // 加密 / 解密
    QRadioButton *m_encModeEncrypt = nullptr;
    QRadioButton *m_encModeDecrypt = nullptr;
    QLineEdit *m_encIn = nullptr;
    QLineEdit *m_encUser = nullptr;
    QLineEdit *m_encOwner = nullptr;
    QLineEdit *m_encPassword = nullptr;
    QLineEdit *m_encOut = nullptr;
    QCheckBox *m_encAllowPrint = nullptr;
    QCheckBox *m_encAllowCopy = nullptr;
    QCheckBox *m_encAllowModify = nullptr;
    QLabel *m_encHint = nullptr;

    // 压缩 / 优化
    QLineEdit *m_optIn = nullptr;
    QLineEdit *m_optOut = nullptr;
    QLineEdit *m_optPassword = nullptr;
    QCheckBox *m_optLinearize = nullptr;
    QCheckBox *m_optObjectStreams = nullptr;
    QCheckBox *m_optRecompress = nullptr;
    QCheckBox *m_optImages = nullptr;
    QSpinBox *m_optLevel = nullptr;
    QLabel *m_optResult = nullptr;

    // 信息 / 元数据
    QLineEdit *m_infoIn = nullptr;
    QLineEdit *m_infoPassword = nullptr;
    QTextEdit *m_infoOut = nullptr;
};

#endif // PDFTOOL_H
