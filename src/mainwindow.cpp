#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QHeaderView>
#include <QDateTimeEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTabWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkInterface>

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
    m_stackedWidget->addWidget(createIpQueryPage());
    m_stackedWidget->addWidget(createCalcPage());

    m_stackedWidget->setCurrentIndex(0);

    m_aboutLabel = new QLabel(QStringLiteral("<a href='about' style='color:#6c757d;text-decoration:none;'>v0.1.1</a>"), this);
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
        "<h3>Zane Tool v0.1.1</h3>"
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

// ==================== IP Query Page ====================

QWidget *MainWindow::createIpQueryPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *lanGroup = new QGroupBox(QStringLiteral("局域网IP"), page);
    QVBoxLayout *lanLayout = new QVBoxLayout(lanGroup);
    lanLayout->setSpacing(8);

    QHBoxLayout *lanRow = new QHBoxLayout();
    m_ipLanEdit = new QTextEdit(lanGroup);
    m_ipLanEdit->setReadOnly(true);
    m_ipLanEdit->setFocusPolicy(Qt::NoFocus);
    m_ipLanEdit->setPlaceholderText(QStringLiteral("自动检测中..."));
    m_ipLanEdit->setMaximumHeight(144);
    m_ipLanEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_ipLanEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  border: 1px solid #ced4da;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  background-color: #ffffff;"
        "  color: #212529;"
        "}"
        "QTextEdit:focus { border-color: #86b7fe; }"));
    m_ipCopyLanBtn = new QPushButton(QStringLiteral("复制"), lanGroup);
    m_ipCopyLanBtn->setCursor(Qt::PointingHandCursor);
    m_ipCopyLanBtn->setFixedHeight(28);
    m_ipCopyLanBtn->setFixedWidth(60);
    m_ipCopyLanBtn->setEnabled(false);
    m_ipCopyLanBtn->setStyleSheet(QStringLiteral("font-size: 12px;"));
    connect(m_ipCopyLanBtn, &QPushButton::clicked, this, &MainWindow::onIpCopyLan);
    lanRow->addWidget(m_ipLanEdit, 1);
    lanRow->addWidget(m_ipCopyLanBtn);
    lanLayout->addLayout(lanRow);

    mainLayout->addWidget(lanGroup);

    QGroupBox *wanGroup = new QGroupBox(QStringLiteral("外网IP"), page);
    QVBoxLayout *wanLayout = new QVBoxLayout(wanGroup);
    wanLayout->setSpacing(8);

    QHBoxLayout *wanSourceRow = new QHBoxLayout();
    QLabel *wanSourceLabel = new QLabel(QStringLiteral("查询源:"), wanGroup);
    wanSourceLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_ipWanSourceCombo = new QComboBox(wanGroup);
    m_ipWanSourceCombo->addItem(QStringLiteral("icanhazip.com"), QStringLiteral("http://icanhazip.com"));
    m_ipWanSourceCombo->addItem(QStringLiteral("ifconfig.me/ip"), QStringLiteral("http://ifconfig.me/ip"));
    m_ipWanSourceCombo->addItem(QStringLiteral("ipinfo.io/ip"), QStringLiteral("http://ipinfo.io/ip"));
    m_ipWanSourceCombo->addItem(QStringLiteral("ipecho.net/plain"), QStringLiteral("http://ipecho.net/plain"));
    m_ipWanSourceCombo->addItem(QStringLiteral("www.trackip.net/ip"), QStringLiteral("http://www.trackip.net/ip"));
    m_ipWanSourceCombo->addItem(QStringLiteral("httpbin.org/ip"), QStringLiteral("http://httpbin.org/ip"));
    m_ipWanSourceCombo->addItem(QStringLiteral("ip.sb"), QStringLiteral("http://ip.sb"));
    m_ipWanQueryBtn = new QPushButton(QStringLiteral("查询"), wanGroup);
    m_ipWanQueryBtn->setCursor(Qt::PointingHandCursor);
    m_ipWanQueryBtn->setFixedHeight(28);
    m_ipWanQueryBtn->setFixedWidth(60);
    m_ipWanQueryBtn->setStyleSheet(QStringLiteral("font-size: 12px;"));
    connect(m_ipWanQueryBtn, &QPushButton::clicked, this, &MainWindow::onIpWanQuery);
    wanSourceRow->addWidget(wanSourceLabel);
    wanSourceRow->addWidget(m_ipWanSourceCombo, 1);
    wanSourceRow->addWidget(m_ipWanQueryBtn);
    wanLayout->addLayout(wanSourceRow);

    QHBoxLayout *wanRow = new QHBoxLayout();
    m_ipWanEdit = new QLineEdit(wanGroup);
    m_ipWanEdit->setReadOnly(true);
    m_ipWanEdit->setFocusPolicy(Qt::NoFocus);
    m_ipWanEdit->setPlaceholderText(QStringLiteral("点击查询获取外网IP"));
    m_ipCopyWanBtn = new QPushButton(QStringLiteral("复制"), wanGroup);
    m_ipCopyWanBtn->setCursor(Qt::PointingHandCursor);
    m_ipCopyWanBtn->setFixedHeight(28);
    m_ipCopyWanBtn->setFixedWidth(60);
    m_ipCopyWanBtn->setEnabled(false);
    m_ipCopyWanBtn->setStyleSheet(QStringLiteral("font-size: 12px;"));
    connect(m_ipCopyWanBtn, &QPushButton::clicked, this, &MainWindow::onIpCopyWan);
    wanRow->addWidget(m_ipWanEdit, 1);
    wanRow->addWidget(m_ipCopyWanBtn);
    wanLayout->addLayout(wanRow);

    mainLayout->addWidget(wanGroup);
    mainLayout->addStretch(1);

    m_ipNetworkManager = new QNetworkAccessManager(this);

    // 自动检测局域网IP
    QStringList lanIps;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp)
            && iface.flags().testFlag(QNetworkInterface::IsRunning)
            && !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            const QList<QNetworkAddressEntry> entries = iface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                QHostAddress addr = entry.ip();
                if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                    lanIps.append(iface.humanReadableName() + QStringLiteral(": ") + addr.toString());
                }
            }
        }
    }

    if (lanIps.isEmpty()) {
        m_ipLanText.clear();
        m_ipLanEdit->setPlainText(QStringLiteral("未检测到局域网IP"));
        m_ipLanEdit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px;"
            "  border: 1px solid #ced4da;"
            "  border-radius: 6px;"
            "  padding: 10px;"
            "  background-color: #ffffff;"
            "  color: #dc3545;"
            "}"
            "QTextEdit:focus { border-color: #86b7fe; }"));
        m_ipCopyLanBtn->setEnabled(false);
    } else {
        m_ipLanText = lanIps.join(QStringLiteral("\n"));
        m_ipLanEdit->setPlainText(m_ipLanText);
        m_ipLanEdit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px;"
            "  border: 1px solid #ced4da;"
            "  border-radius: 6px;"
            "  padding: 10px;"
            "  background-color: #ffffff;"
            "  color: #198754;"
            "}"
            "QTextEdit:focus { border-color: #86b7fe; }"));
        m_ipCopyLanBtn->setEnabled(true);
    }

    return page;
}

void MainWindow::onIpWanQuery()
{
    QString sourceUrl = m_ipWanSourceCombo->currentData().toString();
    m_ipWanEdit->setText(QStringLiteral("查询中..."));
    m_ipWanEdit->setStyleSheet(QStringLiteral("color: #6c757d;"));
    m_ipCopyWanBtn->setEnabled(false);
    m_ipWanQueryBtn->setEnabled(false);

    QUrl url(sourceUrl);
    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    QNetworkReply *reply = m_ipNetworkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_ipWanQueryBtn->setEnabled(true);

        if (reply->error() != QNetworkReply::NoError) {
            m_ipWanEdit->setText(QStringLiteral("查询失败: %1").arg(reply->errorString()));
            m_ipWanEdit->setStyleSheet(QStringLiteral("color: #dc3545;"));
            m_ipCopyWanBtn->setEnabled(false);
            return;
        }

        QByteArray data = reply->readAll();
        QString sourceUrl = reply->url().toString();

        if (sourceUrl.contains(QStringLiteral("httpbin.org"))) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString ip = obj.value(QStringLiteral("origin")).toString();
                if (!ip.isEmpty()) {
                    int commaPos = ip.indexOf(',');
                    if (commaPos > 0)
                        ip = ip.left(commaPos).trimmed();
                    m_ipWanEdit->setText(ip);
                    m_ipWanEdit->setStyleSheet(QStringLiteral("color: #198754;"));
                    m_ipCopyWanBtn->setEnabled(true);
                    return;
                }
            }
            m_ipWanEdit->setText(QStringLiteral("解析失败"));
            m_ipWanEdit->setStyleSheet(QStringLiteral("color: #dc3545;"));
        } else {
            QString ip = QString::fromUtf8(data).trimmed();
            if (ip.isEmpty()) {
                m_ipWanEdit->setText(QStringLiteral("查询结果为空"));
                m_ipWanEdit->setStyleSheet(QStringLiteral("color: #dc3545;"));
                m_ipCopyWanBtn->setEnabled(false);
            } else {
                m_ipWanEdit->setText(ip);
                m_ipWanEdit->setStyleSheet(QStringLiteral("color: #198754;"));
                m_ipCopyWanBtn->setEnabled(true);
            }
        }
    });
}

void MainWindow::onIpCopyLan()
{
    QString text = m_ipLanEdit->toPlainText();
    if (text.isEmpty() || m_ipLanText.isEmpty())
        return;
    QApplication::clipboard()->setText(m_ipLanText);
    QString original = m_ipCopyLanBtn->text();
    m_ipCopyLanBtn->setText(QStringLiteral("已复制"));
    m_ipCopyLanBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_ipCopyLanBtn->setText(original);
        m_ipCopyLanBtn->setEnabled(true);
    });
}

void MainWindow::onIpCopyWan()
{
    QString text = m_ipWanEdit->text();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_ipCopyWanBtn->text();
    m_ipCopyWanBtn->setText(QStringLiteral("已复制"));
    m_ipCopyWanBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_ipCopyWanBtn->setText(original);
        m_ipCopyWanBtn->setEnabled(true);
    });
}

QWidget *MainWindow::createCalcPage()
{
    m_calcPage = new CalculatorPage(this);
    return m_calcPage;
}
