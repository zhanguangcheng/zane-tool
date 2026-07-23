#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QHeaderView>
#include <QDateTimeEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTabWidget>

#include <set>
#include <algorithm>

#include <windows.h>
#include <shellapi.h>

#include "mainwindow.h"
#include "image_tool.h"
#include "video_tool.h"
#include "audio_tool.h"
#include "colorpickerpage.h"
#include "ffmpegprocess.h"
#include "utils.h"
#include "colorpicker.h"
#include "screenshottool.h"
#include "transparencytool.h"
#include "timertool.h"
#include "base64tool.h"
#include "timestamptool.h"
#include "crontool.h"
#include "jwttool.h"
#include "randomstringtool.h"
#include "qrcodetool.h"
#include "certtool.h"
#include "downloadtool.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QStatusBar>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QDesktopServices>
#include <QImage>
#include <QPainter>

MainWindow::MainWindow(const QString &ffmpegPath, const QString &aria2Path, const QString &mkcertPath, QWidget *parent)
    : QMainWindow(parent)
    , m_ffmpegPath(ffmpegPath)
    , m_aria2Path(aria2Path)
    , m_downloadTool(nullptr)
    , m_ipTool(nullptr)
    , m_calcPage(nullptr)
    , m_screenshotTool(new ScreenshotTool(this))
    , m_transparencyTool(new TransparencyTool(this))
    , m_timerTool(new TimerTool(this))
    , m_base64Tool(new Base64Tool(this))
    , m_timestampTool(new TimestampTool(this))
    , m_cronTool(new CronTool(this))
    , m_jwtTool(new JwtTool(this))
    , m_randomStringTool(new RandomStringTool(this))
    , m_qrCodeTool(new QrCodeTool(m_screenshotTool, this))
    , m_certTool(new CertTool(this, mkcertPath, this))
    , m_imageTool(nullptr)
    , m_videoTool(nullptr)
    , m_audioTool(nullptr)
{
    setWindowTitle(QStringLiteral("Zane Tool"));
    resize(820, 620);
    setAcceptDrops(true);
    setupUi();

    m_screenshotTool->loadConfig();
    m_screenshotTool->registerHotkey();
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_sidebar = new QListWidget(centralWidget);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(160);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sidebar->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_stackedWidget = new QStackedWidget(centralWidget);

    mainLayout->addWidget(m_sidebar);
    mainLayout->addWidget(m_stackedWidget, 1);

    setCentralWidget(centralWidget);

    setupSidebar();

    m_imageTool = new ImageTool(m_ffmpegPath, this);
    m_videoTool = new VideoTool(m_ffmpegPath, this);
    m_audioTool = new AudioTool(m_ffmpegPath, this);

    m_stackedWidget->addWidget(m_imageTool);
    m_stackedWidget->addWidget(m_videoTool);
    m_stackedWidget->addWidget(m_audioTool);
    m_colorPickerPage = new ColorPickerPage(this);
    m_stackedWidget->addWidget(m_colorPickerPage);
    m_stackedWidget->addWidget(m_screenshotTool->createPage());
    m_stackedWidget->addWidget(m_transparencyTool->createPage());
    m_stackedWidget->addWidget(m_timerTool->createPage());
    m_stackedWidget->addWidget(m_base64Tool->createPage());
    m_stackedWidget->addWidget(m_timestampTool->createPage());
    m_stackedWidget->addWidget(m_cronTool->createPage());
    m_stackedWidget->addWidget(m_jwtTool->createPage());
    m_downloadTool = new DownloadTool(m_aria2Path, this);
    m_stackedWidget->addWidget(m_downloadTool);
    m_stackedWidget->addWidget(m_randomStringTool->createPage());
    m_stackedWidget->addWidget(m_qrCodeTool->createPage());
    m_stackedWidget->addWidget(m_certTool->createPage());
    m_ipTool = new IpTool(this);
    m_stackedWidget->addWidget(m_ipTool);
    m_stackedWidget->addWidget(createCalcPage());

    m_stackedWidget->setCurrentIndex(0);

    m_aboutLabel = new QLabel(QStringLiteral("<a href='about' style='color:#6c757d;text-decoration:none;'>v1.0.0</a>"), this);
    m_aboutLabel->setCursor(Qt::PointingHandCursor);
    connect(m_aboutLabel, &QLabel::linkActivated, this, &MainWindow::showAbout);
    statusBar()->addPermanentWidget(m_aboutLabel);
    statusBar()->setSizeGripEnabled(false);
}

void MainWindow::setupSidebar()
{
    auto addCategory = [this](const QString &name) {
        QListWidgetItem *item = new QListWidgetItem(name);
        item->setFlags(Qt::NoItemFlags);
        item->setData(Qt::UserRole, -1);
        QFont f = item->font();
        f.setBold(true);
        f.setPointSize(10);
        item->setFont(f);
        item->setForeground(QColor(QStringLiteral("#6c757d")));
        m_sidebar->addItem(item);
    };

    auto addTool = [this](const QString &name, int pageIndex) {
        QListWidgetItem *item = new QListWidgetItem(QStringLiteral("      ") + name);
        item->setData(Qt::UserRole, pageIndex);
        m_sidebar->addItem(item);
    };

    addCategory(QStringLiteral("\U0001F3AC 媒体工具"));
    addTool(QStringLiteral("图片处理"), 0);
    addTool(QStringLiteral("视频处理"), 1);
    addTool(QStringLiteral("音频处理"), 2);

    addCategory(QStringLiteral("\u2699\uFE0F 系统工具"));
    addTool(QStringLiteral("屏幕取色"), 3);
    addTool(QStringLiteral("截图贴图"), 4);
    addTool(QStringLiteral("窗口透明"), 5);
    addTool(QStringLiteral("秒表计时"), 6);
    addTool(QStringLiteral("计算器"), 16);

    addCategory(QStringLiteral("\U0001F527 开发工具"));
    addTool(QStringLiteral("图片转Base64"), 7);
    addTool(QStringLiteral("时间戳转换"), 8);
    addTool(QStringLiteral("Cron 解析"), 9);
    addTool(QStringLiteral("JWT 解析"), 10);
    addTool(QStringLiteral("随机字符串"), 12);
    addTool(QStringLiteral("二维码工具"), 13);
    addTool(QStringLiteral("HTTPS证书"), 14);

    addCategory(QStringLiteral("\U0001F310 网络工具"));
    addTool(QStringLiteral("文件批量下载"), 11);
    addTool(QStringLiteral("本机IP查询"), 15);

    connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        QListWidgetItem *item = m_sidebar->item(row);
        int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0)
            m_stackedWidget->setCurrentIndex(idx);
    });

    m_sidebar->setCurrentRow(1);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_screenshotTool->unregisterHotkey();
    if (QProcess *cp = m_certTool->process()) {
        if (cp->state() != QProcess::NotRunning) {
            cp->kill();
            cp->waitForFinished(3000);
        }
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == m_screenshotTool->hotkeyId()) {
            m_screenshotTool->startScreenshot();
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    return QMainWindow::eventFilter(obj, event);
}

// ==================== Results ====================

void MainWindow::showAbout()
{
    QString msg = QStringLiteral(
        "<h3>Zane Tool v1.0.0</h3>"
        "<p>集成 ffmpeg 与 aria2c 的桌面端效率工具箱。</p>"
        "<p><b>媒体工具</b><br>"
        "图片/视频/音频批量处理：压缩、缩放、格式转换</p>"
        "<p><b>系统工具</b><br>"
        "屏幕取色 &middot; 截图贴图 &middot; 窗口透明 &middot; 秒表计时</p>"
        "<p><b>开发工具</b><br>"
        "图片转Base64 &middot; 时间戳转换 &middot; Cron 解析 &middot; JWT 解析 &middot; 随机字符串 &middot; 二维码工具 &middot; HTTPS证书</p>"
        "<p><b>网络工具</b><br>"
        "批量文件下载（aria2c）</p>"
        "<p><b>技术栈</b><br>"
        "Qt 6 (Widgets) &middot; C++17 &middot; ffmpeg &middot; aria2c<br>"
        "MinGW GCC 13.1 &middot; CMake 3.16+</p>"
        "<p><b>作者:</b> Zane</p>"
    );
    QMessageBox::about(this, QStringLiteral("关于"), msg);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (!mimeData->hasUrls()) return;

    int pageIndex = m_stackedWidget->currentIndex();

    if (pageIndex == 7) {
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile()) {
                m_base64Tool->processFile(url.toLocalFile());
                return;
            }
        }
    }

    QListWidget *list = nullptr;
    if (pageIndex == 0)
        list = m_imageTool ? m_imageTool->fileListWidget() : nullptr;
    else if (pageIndex == 1)
        list = m_videoTool ? m_videoTool->fileListWidget() : nullptr;
    else if (pageIndex == 2)
        list = m_audioTool ? m_audioTool->fileListWidget() : nullptr;

    if (!list) return;

    int skipped = 0;
    for (const QUrl &url : mimeData->urls()) {
        if (!url.isLocalFile())
            continue;
        QString path = url.toLocalFile();
        bool ok = false;
        if (pageIndex == 0)
            ok = Utils::isSupportedImageFile(path);
        else if (pageIndex == 1)
            ok = Utils::isSupportedVideoFile(path);
        else if (pageIndex == 2)
            ok = Utils::isSupportedAudioFile(path);
        if (ok)
            list->addItem(path);
        else
            ++skipped;
    }
    if (skipped > 0)
        statusBar()->showMessage(QStringLiteral("已跳过 %1 个不支持的文件").arg(skipped), 3000);
}

QWidget *MainWindow::createCalcPage()
{
    m_calcPage = new CalculatorPage(this);
    return m_calcPage;
}
