#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <windows.h>

#include <QDragEnterEvent>
#include <QDropEvent>

#include "calculator.h"

class WindowPicker;
class ScreenshotTool;
class TransparencyTool;
class TimerTool;
class Base64Tool;
class TimestampTool;
class QRadioButton;
class QDateTimeEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &ffmpegPath, const QString &aria2Path, const QString &mkcertPath, QWidget *parent = nullptr);

private slots:
    void onCronInputChanged();
    void onCronPresetChanged(int index);
    void onCronCountChanged(int index);
    void onCronCopyAll();
    void onCronUpdateTimes();

    void onJwtParse();
    void onJwtClear();
    void onJwtCopyCurrent();
    void onJwtCopyAll();

    void onRandomGenerate();
    void onRandomCopy();

    void onDownloadAddFile();
    void onDownloadPaste();
    void onDownloadClear();
    void onDownloadOutputBrowse();
    void onDownloadStart();
    void onDownloadCancel();
    void onDownloadProcessOutput();

    void onQrGenerate();
    void onQrSaveImage();
    void onQrCopyImage();
    void onQrSelectImage();
    void onQrScreenCapture();
    void onQrCopyResult();
    void onQrOpenLink();

    void onIpWanQuery();
    void onIpCopyLan();
    void onIpCopyWan();

    void onCertGenerate();
    void onCertInstallCa();
    void onCertUninstallCa();
    void onCertOutputBrowse();
    void onCertOpenOutputDir();
    void onCertOpenCaroot();
    void onCertProcessOutput();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();
    void setupSidebar();
    QWidget *createCronPage();
    QWidget *createJwtPage();
    QWidget *createDownloadPage();
    QWidget *createRandomStringPage();
    QWidget *createQrCodePage();
    QWidget *createCertPage();
    QWidget *createIpQueryPage();
    QWidget *createCalcPage();

    void showAbout();

    void setDownloadUiEnabled(bool enabled);

    void processQrDecodeImage(const QImage &image, const QString &sourceDesc);
    void refreshCertCaStatus();

    QString m_ffmpegPath;
    QListWidget *m_sidebar;
    QStackedWidget *m_stackedWidget;

    // ---- Tool pages ----
    class ImageTool *m_imageTool;
    class VideoTool *m_videoTool;
    class AudioTool *m_audioTool;
    class ColorPickerPage *m_colorPickerPage;
    ScreenshotTool *m_screenshotTool;
    TransparencyTool *m_transparencyTool;
    TimerTool *m_timerTool;
    Base64Tool *m_base64Tool;
    TimestampTool *m_timestampTool;

    QLabel *m_aboutLabel;

    // ---- Cron tab widgets ----
    QLineEdit *m_cronInputEdit;
    QComboBox *m_cronPresetCombo;
    QComboBox *m_cronCountCombo;
    QPushButton *m_cronCopyBtn;
    QTableWidget *m_cronNextTable;
    QLabel *m_cronMinField;
    QLabel *m_cronHourField;
    QLabel *m_cronDomField;
    QLabel *m_cronMonthField;
    QLabel *m_cronDowField;
    QLabel *m_cronErrorLabel;
    QLabel *m_cronDescLabel;
    QTimer *m_cronTimer;

    // ---- JWT tab widgets ----
    QTextEdit *m_jwtInputEdit;
    QPushButton *m_jwtParseBtn;
    QPushButton *m_jwtClearBtn;
    QTabWidget *m_jwtResultTabs;
    QTextEdit *m_jwtHeaderEdit;
    QTextEdit *m_jwtPayloadEdit;
    QTextEdit *m_jwtSignatureEdit;
    QLabel *m_jwtInfoLabel;
    QPushButton *m_jwtCopyCurrentBtn;
    QPushButton *m_jwtCopyAllBtn;

    // ---- RandomString tab widgets ----
    QCheckBox *m_randomUpperCheck;
    QCheckBox *m_randomLowerCheck;
    QCheckBox *m_randomDigitCheck;
    QCheckBox *m_randomSymbolCheck;
    QLineEdit *m_randomExcludeEdit;
    QSpinBox *m_randomLengthSpin;
    QSpinBox *m_randomCountSpin;
    QTextEdit *m_randomOutput;
    QPushButton *m_randomGenerateBtn;
    QPushButton *m_randomCopyBtn;

    // ---- Download tab widgets ----
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

    // ---- Task queues ----

    // ---- Statistics ----

    QProcess *m_downloadProcess;
    struct DownloadEntry { QString url; QString filename; int row; bool completed; bool failed; };
    QList<DownloadEntry> m_downloadEntries;
    QList<QProgressBar *> m_downloadBars;
    QMap<QString, int> m_gidToIndex;
    QString m_downloadPendingUrl;
    int m_downloadCompleted;
    int m_downloadFailed;
    bool m_downloadCancelling;
    QString m_downloadStdoutBuffer;
    QString m_aria2Path;
    QString m_downloadTempFile;
    QString m_downloadOutputDirPath;

    // ---- QR code tab widgets ----
    QTextEdit *m_qrGenInput;
    QComboBox *m_qrGenEccCombo;
    QSpinBox *m_qrGenScaleSpin;
    QLabel *m_qrGenPreview;
    QLabel *m_qrGenStatusLabel;
    QPushButton *m_qrGenSaveBtn;
    QPushButton *m_qrGenCopyBtn;
    QPixmap m_qrGenPixmap;

    QLineEdit *m_qrDecFilePath;
    QPushButton *m_qrDecSelectBtn;
    QPushButton *m_qrDecScreenBtn;
    QLabel *m_qrDecDropZone;
    QLabel *m_qrDecPreview;
    QTextEdit *m_qrDecOutput;
    QLabel *m_qrDecInfoLabel;
    QPushButton *m_qrDecCopyBtn;
    QPushButton *m_qrDecOpenBtn;

    // ---- Cert (HTTPS证书) tab widgets ----
    QLabel *m_certCaStatusLabel;
    QLabel *m_certCarootLabel;
    QPushButton *m_certInstallCaBtn;
    QPushButton *m_certUninstallCaBtn;
    QPushButton *m_certOpenCarootBtn;
    QTextEdit *m_certDomainsInput;
    QLineEdit *m_certNameEdit;
    QLineEdit *m_certOutputDir;
    QPushButton *m_certOutputBrowseBtn;
    QPushButton *m_certGenerateBtn;
    QPushButton *m_certOpenOutputBtn;
    QTextEdit *m_certLogOutput;
    QProcess *m_certProcess;
    QString m_mkcertPath;
    QString m_certCarootPath;
    bool m_certRunning;

    // ---- IP Query tab widgets ----
    QTextEdit *m_ipLanEdit;
    QPushButton *m_ipCopyLanBtn;
    QComboBox *m_ipWanSourceCombo;
    QPushButton *m_ipWanQueryBtn;
    QLineEdit *m_ipWanEdit;
    QPushButton *m_ipCopyWanBtn;
    QNetworkAccessManager *m_ipNetworkManager;
    QString m_ipLanText;

    CalculatorPage *m_calcPage;
};

#endif // MAINWINDOW_H
