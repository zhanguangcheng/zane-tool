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
class CronTool;
class JwtTool;
class RandomStringTool;
class QrCodeTool;
class CertTool;
class QRadioButton;
class QDateTimeEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &ffmpegPath, const QString &aria2Path, const QString &mkcertPath, QWidget *parent = nullptr);

private slots:
    void onIpWanQuery();
    void onIpCopyLan();
    void onIpCopyWan();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();
    void setupSidebar();
    QWidget *createIpQueryPage();
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
    JwtTool *m_jwtTool;
    RandomStringTool *m_randomStringTool;
    QrCodeTool *m_qrCodeTool;
    CertTool *m_certTool;
    class DownloadTool *m_downloadTool;

    QLabel *m_aboutLabel;

    // ---- Task queues ----

    // ---- Statistics ----

    QString m_aria2Path;

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
