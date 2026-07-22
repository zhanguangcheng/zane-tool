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
#include "stopwatchtimer.h"

class ColorPicker;
class WindowPicker;
class ScreenshotPicker;
class PinWindow;
class QRadioButton;
class QDateTimeEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &ffmpegPath, const QString &aria2Path, const QString &mkcertPath, QWidget *parent = nullptr);

private slots:
    void onStartPickColor();
    void onColorPicked(const QColor &color);
    void onPickCancelled();
    void onCopyHex();
    void onCopyRgb();
    void onHistoryColorClicked(int index);

    void onTransparencyRefresh();
    void onTransparencySelectionChanged();
    void onTransparencySliderChanged(int value);
    void onTransparencyPickWindow();
    void onWindowPicked(HWND hwnd);

    void onTimerStart();
    void onTimerPause();
    void onTimerStop();
    void onTimerLap();
    void onTimerClearLaps();
    void onTimerTick(const QString &formatted);
    void onTimerStateChanged(StopwatchTimer::State state);
    void onTimerLapRecorded(const StopwatchTimer::LapEntry &entry);

    void onBase64SelectFile();
    void onBase64Clear();
    void onBase64Copy();

    void onTimestampUpdate();
    void onTimestampNowSecCopy();

    void onTimestampInputChanged();
    void onTimestampResultCopy();
    void onDatetimeInputChanged();
    void onDatetimeSecCopy();


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

    void onStartScreenshot();
    void onScreenshotCaptured(const QPixmap &pixmap, QPoint globalPos);

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
    QWidget *createColorPickerPage();
    QWidget *createStickyNotePage();
    QWidget *createTransparencyPage();
    QWidget *createTimerPage();
    QWidget *createBase64Page();
    QWidget *createTimestampPage();
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
    void updateColorDisplay();
    void updateColorHistory();

    void registerGlobalHotkey();
    void unregisterGlobalHotkey();
    QString hotkeyDisplayText() const;
    void onChangeScreenshotHotkey();
    QString mimeTypeForFile(const QString &filePath) const;
    void processBase64File(const QString &filePath);
    void processQrDecodeImage(const QImage &image, const QString &sourceDesc);
    void refreshCertCaStatus();

    QString m_ffmpegPath;
    QListWidget *m_sidebar;
    QStackedWidget *m_stackedWidget;

    // ---- Tool pages ----
    class ImageTool *m_imageTool;
    class VideoTool *m_videoTool;
    class AudioTool *m_audioTool;

    QLabel *m_aboutLabel;

    // ---- ColorPicker tab widgets ----
    QPushButton *m_pickColorBtn;
    QLabel *m_colorSwatch;
    QLabel *m_hexLabel;
    QLabel *m_hexValue;
    QLabel *m_rgbLabel;
    QLabel *m_rgbValue;
    QLabel *m_hslLabel;
    QLabel *m_hslValue;
    QPushButton *m_copyHexBtn;
    QPushButton *m_copyRgbBtn;
    QWidget *m_historyContainer;
    QHBoxLayout *m_historyLayout;
    QColor m_pickedColor;
    QList<QColor> m_colorHistory;
    ColorPicker *m_colorPicker;

    // ---- Transparency tab widgets ----
    QListWidget *m_transparencyWindowList;
    QPushButton *m_transparencyRefreshBtn;
    QPushButton *m_transparencyPickBtn;
    QSlider *m_transparencySlider;
    QLabel *m_transparencyValueLabel;
    QLabel *m_transparencyStatusLabel;
    HWND m_transparencyTargetHwnd;
    LONG_PTR m_transparencyOriginalExStyle;
    WindowPicker *m_windowPicker;

    // ---- Timer tab widgets ----
    StopwatchTimer *m_stopwatch;
    QLabel *m_timerDisplay;
    QPushButton *m_timerStartBtn;
    QPushButton *m_timerPauseBtn;
    QPushButton *m_timerStopBtn;
    QPushButton *m_timerLapBtn;
    QTableWidget *m_timerLapList;
    QPushButton *m_timerClearBtn;

    ScreenshotPicker *m_screenshotPicker;
    QList<PinWindow *> m_pinnedWindows;
    QLabel *m_screenshotHotkeyLabel;

    // ---- Base64 tab widgets ----
    QLineEdit *m_base64FilePath;
    QPushButton *m_base64SelectBtn;
    QPushButton *m_base64ClearBtn;
    QLabel *m_base64DropZone;
    QTextEdit *m_base64Output;
    QLabel *m_base64InfoLabel;
    QPushButton *m_base64CopyBtn;

    QTimer *m_timestampTimer;
    QLabel *m_timestampNowLabel;
    QLineEdit *m_timestampNowSecEdit;
    QPushButton *m_timestampNowSecCopyBtn;
    QLineEdit *m_timestampInputEdit;
    QRadioButton *m_timestampSecRadio;
    QRadioButton *m_timestampMsRadio;
    QLabel *m_timestampResultLabel;
    QPushButton *m_timestampResultCopyBtn;
    QDateTimeEdit *m_datetimeInputEdit;
    QLineEdit *m_datetimeSecResultEdit;
    QPushButton *m_datetimeSecCopyBtn;

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

    int m_hotkeyId = 1;
    UINT m_hotkeyVk = VK_F4;
    UINT m_hotkeyModifiers = 0;

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
    bool m_screenshotForQr = false;

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
