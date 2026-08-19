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

#include <windows.h>

#include <QDragEnterEvent>
#include <QDropEvent>

#include "calculator.h"
#include "iptool.h"

class WindowPicker;
class ScreenshotTool;
class TransparencyTool;
class TimerTool;
class Base64Tool;
class TimestampTool;
class CronTool;
class CodecTool;
class RandomStringTool;
class QrCodeTool;
class CertTool;
class JsonTool;
class CurlTool;
class QRadioButton;
class QDateTimeEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &ffmpegPath, const QString &aria2Path, const QString &mkcertPath, QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();
    void setupSidebar();
    void ensurePage(int index);
    QWidget *createCalcPage();

    void showAbout();

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
    CronTool *m_cronTool;
    CodecTool *m_codecTool;
    RandomStringTool *m_randomStringTool;
    QrCodeTool *m_qrCodeTool;
    CertTool *m_certTool;
    JsonTool *m_jsonTool;
    CurlTool *m_curlTool;
    class DownloadTool *m_downloadTool;

    QLabel *m_aboutLabel;

    // ---- Task queues ----

    // ---- Statistics ----

    QString m_aria2Path;

    IpTool *m_ipTool;
    CalculatorPage *m_calcPage;

    bool m_pageCreated[19] = {};
};

#endif // MAINWINDOW_H
