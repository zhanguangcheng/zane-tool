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
    , m_downloadProcess(nullptr)
    , m_downloadCompleted(0)
    , m_downloadFailed(0)
    , m_downloadCancelling(false)
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
    m_stackedWidget->addWidget(createDownloadPage());
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

QWidget *MainWindow::createDownloadPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(10);

    // ---- URL Input Group ----
    QGroupBox *urlGroup = new QGroupBox(QStringLiteral("下载地址"), page);
    QVBoxLayout *urlLayout = new QVBoxLayout(urlGroup);
    urlLayout->setSpacing(6);

    QLabel *urlHint = new QLabel(
        QStringLiteral("每行一个URL，可在URL后加空格或Tab指定自定义文件名"), urlGroup);
    urlHint->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 12px;"));

    m_downloadUrlInput = new QTextEdit(urlGroup);
    m_downloadUrlInput->setPlaceholderText(QStringLiteral(
        "http://example.com/file.zip\n"
        "http://example.com/report.pdf 月报.pdf\n"
        "http://example.com/data 数据包.zip"));
    m_downloadUrlInput->setMinimumHeight(80);
    m_downloadUrlInput->setAcceptRichText(false);
    m_downloadUrlInput->setStyleSheet(QStringLiteral(
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

    QHBoxLayout *urlBtnRow = new QHBoxLayout();
    urlBtnRow->setSpacing(8);
    m_downloadAddFileBtn = new QPushButton(QStringLiteral("添加文本文件"), urlGroup);
    m_downloadAddFileBtn->setFixedHeight(30);
    m_downloadAddFileBtn->setCursor(Qt::PointingHandCursor);
    m_downloadAddFileBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 4px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(m_downloadAddFileBtn, &QPushButton::clicked, this, &MainWindow::onDownloadAddFile);

    m_downloadPasteBtn = new QPushButton(QStringLiteral("从剪贴板粘贴"), urlGroup);
    m_downloadPasteBtn->setFixedHeight(30);
    m_downloadPasteBtn->setCursor(Qt::PointingHandCursor);
    m_downloadPasteBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 4px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(m_downloadPasteBtn, &QPushButton::clicked, this, &MainWindow::onDownloadPaste);

    m_downloadClearBtn = new QPushButton(QStringLiteral("清空"), urlGroup);
    m_downloadClearBtn->setFixedHeight(30);
    m_downloadClearBtn->setCursor(Qt::PointingHandCursor);
    m_downloadClearBtn->setObjectName(QStringLiteral("dangerBtn"));
    connect(m_downloadClearBtn, &QPushButton::clicked, this, &MainWindow::onDownloadClear);

    urlBtnRow->addWidget(m_downloadAddFileBtn);
    urlBtnRow->addWidget(m_downloadPasteBtn);
    urlBtnRow->addWidget(m_downloadClearBtn);
    urlBtnRow->addStretch();

    urlLayout->addWidget(urlHint);
    urlLayout->addWidget(m_downloadUrlInput);
    urlLayout->addLayout(urlBtnRow);
    mainLayout->addWidget(urlGroup);

    // ---- Settings Row ----
    QHBoxLayout *settingsRow = new QHBoxLayout();
    settingsRow->setSpacing(12);

    QGroupBox *settingsGroup = new QGroupBox(QStringLiteral("下载设置"), page);
    QGridLayout *settingsGrid = new QGridLayout(settingsGroup);
    settingsGrid->setSpacing(8);

    settingsGrid->addWidget(new QLabel(QStringLiteral("最大并发下载:"), settingsGroup), 0, 0);
    m_downloadMaxConcurrent = new QSpinBox(settingsGroup);
    m_downloadMaxConcurrent->setRange(1, 32);
    m_downloadMaxConcurrent->setValue(5);
    settingsGrid->addWidget(m_downloadMaxConcurrent, 0, 1);

    settingsGrid->addWidget(new QLabel(QStringLiteral("单文件连接数:"), settingsGroup), 1, 0);
    m_downloadMaxConnections = new QSpinBox(settingsGroup);
    m_downloadMaxConnections->setRange(1, 32);
    m_downloadMaxConnections->setValue(16);
    settingsGrid->addWidget(m_downloadMaxConnections, 1, 1);

    settingsGrid->addWidget(new QLabel(QStringLiteral("限速(KB/s):"), settingsGroup), 2, 0);
    m_downloadSpeedLimit = new QSpinBox(settingsGroup);
    m_downloadSpeedLimit->setRange(0, 999999);
    m_downloadSpeedLimit->setValue(0);
    m_downloadSpeedLimit->setSpecialValueText(QStringLiteral("不限速"));
    m_downloadSpeedLimit->setSuffix(QStringLiteral(" KB/s"));
    settingsGrid->addWidget(m_downloadSpeedLimit, 2, 1);

    m_downloadAllowOverwrite = new QCheckBox(QStringLiteral("允许覆盖已存在文件"), settingsGroup);
    settingsGrid->addWidget(m_downloadAllowOverwrite, 3, 0, 1, 2);

    QGroupBox *outDirGroup = new QGroupBox(QStringLiteral("输出目录"), page);
    QVBoxLayout *outDirLayout = new QVBoxLayout(outDirGroup);
    outDirLayout->setSpacing(8);

    m_downloadOutputDir = new QLineEdit(outDirGroup);
    m_downloadOutputDir->setPlaceholderText(
        QStringLiteral("必填，请点击浏览选择输出目录"));
    m_downloadOutputBrowse = new QPushButton(QStringLiteral("浏览"), outDirGroup);
    m_downloadOutputBrowse->setFixedHeight(30);
    m_downloadOutputBrowse->setCursor(Qt::PointingHandCursor);
    connect(m_downloadOutputBrowse, &QPushButton::clicked, this, &MainWindow::onDownloadOutputBrowse);

    outDirLayout->addWidget(m_downloadOutputDir);
    outDirLayout->addWidget(m_downloadOutputBrowse);

    settingsRow->addWidget(settingsGroup);
    settingsRow->addWidget(outDirGroup);
    mainLayout->addLayout(settingsRow);

    // ---- Progress Table ----
    m_downloadProgressTable = new QTableWidget(0, 5, page);
    m_downloadProgressTable->setHorizontalHeaderLabels({
        QStringLiteral("文件名"), QStringLiteral("进度"), QStringLiteral("速度"),
        QStringLiteral("ETA"), QStringLiteral("状态")});
    m_downloadProgressTable->horizontalHeader()->setStretchLastSection(true);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_downloadProgressTable->horizontalHeader()->resizeSection(1, 160);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_downloadProgressTable->horizontalHeader()->resizeSection(2, 80);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_downloadProgressTable->horizontalHeader()->resizeSection(3, 70);
    m_downloadProgressTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_downloadProgressTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_downloadProgressTable->verticalHeader()->setVisible(false);
    m_downloadProgressTable->setMinimumHeight(100);
    mainLayout->addWidget(m_downloadProgressTable);

    // ---- Overall Progress ----
    m_downloadProgressBar = new QProgressBar(page);
    m_downloadProgressBar->setRange(0, 100);
    m_downloadProgressBar->setValue(0);
    m_downloadProgressBar->setTextVisible(true);
    m_downloadProgressBar->setFixedHeight(16);
    mainLayout->addWidget(m_downloadProgressBar);

    m_downloadStatusLabel = new QLabel(QStringLiteral("就绪"), page);
    mainLayout->addWidget(m_downloadStatusLabel);

    // ---- Action Buttons ----
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(16);
    m_downloadStartBtn = new QPushButton(QStringLiteral("开始下载"), page);
    m_downloadStartBtn->setFixedHeight(42);
    m_downloadStartBtn->setFixedWidth(160);
    m_downloadStartBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_downloadStartBtn->setCursor(Qt::PointingHandCursor);
    connect(m_downloadStartBtn, &QPushButton::clicked, this, &MainWindow::onDownloadStart);

    m_downloadCancelBtn = new QPushButton(QStringLiteral("取消"), page);
    m_downloadCancelBtn->setFixedHeight(42);
    m_downloadCancelBtn->setFixedWidth(160);
    m_downloadCancelBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_downloadCancelBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_downloadCancelBtn->setCursor(Qt::PointingHandCursor);
    m_downloadCancelBtn->setEnabled(false);
    connect(m_downloadCancelBtn, &QPushButton::clicked, this, &MainWindow::onDownloadCancel);

    actionLayout->addStretch();
    actionLayout->addWidget(m_downloadStartBtn);
    actionLayout->addSpacing(16);
    actionLayout->addWidget(m_downloadCancelBtn);
    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);

    return page;
}

// ==================== Download Slots ====================

void MainWindow::onDownloadAddFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择文本文件"),
        QString(), QStringLiteral("文本文件 (*.txt);;所有文件 (*)"));
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法打开文件: %1").arg(filePath));
        return;
    }

    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith('#'))
            lines.append(line);
    }
    file.close();

    if (!lines.isEmpty()) {
        QString existing = m_downloadUrlInput->toPlainText().trimmed();
        if (!existing.isEmpty())
            existing += '\n';
        m_downloadUrlInput->setPlainText(existing + lines.join('\n'));
    }
}

void MainWindow::onDownloadPaste()
{
    QString text = QApplication::clipboard()->text().trimmed();
    if (text.isEmpty()) return;

    QString existing = m_downloadUrlInput->toPlainText().trimmed();
    if (!existing.isEmpty())
        existing += '\n';
    m_downloadUrlInput->setPlainText(existing + text);
}

void MainWindow::onDownloadClear()
{
    m_downloadUrlInput->clear();
}

void MainWindow::onDownloadOutputBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
    if (!dir.isEmpty())
        m_downloadOutputDir->setText(QDir::toNativeSeparators(dir));
}

void MainWindow::setDownloadUiEnabled(bool enabled)
{
    m_downloadUrlInput->setReadOnly(!enabled);
    m_downloadAddFileBtn->setEnabled(enabled);
    m_downloadPasteBtn->setEnabled(enabled);
    m_downloadClearBtn->setEnabled(enabled);
    m_downloadMaxConcurrent->setEnabled(enabled);
    m_downloadMaxConnections->setEnabled(enabled);
    m_downloadSpeedLimit->setEnabled(enabled);
    m_downloadAllowOverwrite->setEnabled(enabled);
    m_downloadOutputDir->setEnabled(enabled);
    m_downloadOutputBrowse->setEnabled(enabled);
    m_downloadStartBtn->setEnabled(enabled);
    m_downloadCancelBtn->setEnabled(!enabled);
}

void MainWindow::onDownloadStart()
{
    QString text = m_downloadUrlInput->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请输入下载地址"));
        return;
    }

    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    m_downloadEntries.clear();
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        DownloadEntry entry;
        int sepIdx = trimmed.indexOf(QRegularExpression(QStringLiteral("[ \t]")));
        if (sepIdx > 0) {
            entry.url = trimmed.left(sepIdx).trimmed();
            entry.filename = trimmed.mid(sepIdx + 1).trimmed();
        } else {
            entry.url = trimmed;
        }
        entry.row = -1;
        entry.completed = false;
        entry.failed = false;

        if (entry.url.contains(QStringLiteral("://")))
            m_downloadEntries.append(entry);
    }

    if (m_downloadEntries.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("未检测到有效的下载地址"));
        return;
    }

    QString outputDir = m_downloadOutputDir->text().trimmed();
    if (outputDir.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请先选择输出目录"));
        return;
    }
    QDir().mkpath(outputDir);
    m_downloadOutputDirPath = outputDir;

    m_downloadTempFile = QDir::tempPath() + QStringLiteral("/zane_download_urls.txt");
    QFile tempFile(m_downloadTempFile);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法写入临时文件"));
        return;
    }

    QTextStream out(&tempFile);
    for (const auto &entry : m_downloadEntries) {
        out << entry.url << '\n';
        if (!entry.filename.isEmpty())
            out << "  out=" << entry.filename << '\n';
    }
    tempFile.close();

    m_downloadProgressTable->setRowCount(0);
    m_downloadBars.clear();
    for (int i = 0; i < m_downloadEntries.size(); i++) {
        DownloadEntry &entry = m_downloadEntries[i];
        int row = m_downloadProgressTable->rowCount();
        m_downloadProgressTable->insertRow(row);
        entry.row = row;

        QString displayName = entry.filename.isEmpty()
            ? entry.url.section('/', -1).section('?', 0, 0)
            : entry.filename;
        if (displayName.isEmpty()) displayName = entry.url;

        QTableWidgetItem *nameItem = new QTableWidgetItem(displayName);
        nameItem->setToolTip(entry.url);
        m_downloadProgressTable->setItem(row, 0, nameItem);

        QProgressBar *bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setTextVisible(true);
        bar->setFixedHeight(16);
        QWidget *barContainer = new QWidget();
        QVBoxLayout *barLayout = new QVBoxLayout(barContainer);
        barLayout->setContentsMargins(4, 0, 4, 0);
        barLayout->addWidget(bar);
        barLayout->setAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setCellWidget(row, 1, barContainer);
        m_downloadBars.append(bar);

        QTableWidgetItem *speedItem = new QTableWidgetItem(QStringLiteral("—"));
        speedItem->setTextAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setItem(row, 2, speedItem);

        QTableWidgetItem *etaItem = new QTableWidgetItem(QStringLiteral("—"));
        etaItem->setTextAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setItem(row, 3, etaItem);

        QTableWidgetItem *statusItem = new QTableWidgetItem(QStringLiteral("等待中"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setItem(row, 4, statusItem);
    }

    m_downloadCompleted = 0;
    m_downloadFailed = 0;
    m_downloadCancelling = false;
    m_downloadStdoutBuffer.clear();
    m_gidToIndex.clear();
    m_downloadPendingUrl.clear();
    m_downloadProgressBar->setValue(0);

    QStringList args;
    args << QStringLiteral("--input-file") << QDir::toNativeSeparators(m_downloadTempFile);
    args << QStringLiteral("--dir") << QDir::toNativeSeparators(outputDir);
    args << QStringLiteral("--max-concurrent-downloads")
         << QString::number(m_downloadMaxConcurrent->value());
    args << QStringLiteral("--max-connection-per-server")
         << QString::number(m_downloadMaxConnections->value());

    int speedLimit = m_downloadSpeedLimit->value();
    if (speedLimit > 0)
        args << QStringLiteral("--max-overall-download-limit") << (QString::number(speedLimit) + 'K');

    args << (m_downloadAllowOverwrite->isChecked()
        ? QStringLiteral("--allow-overwrite=true")
        : QStringLiteral("--allow-overwrite=false"));
    args << QStringLiteral("--console-log-level=notice");
    args << QStringLiteral("--summary-interval=1");
    args << QStringLiteral("--enable-color=false");
    args << QStringLiteral("--enable-rpc=false");

    if (m_downloadProcess) {
        m_downloadProcess->deleteLater();
        m_downloadProcess = nullptr;
    }

    m_downloadProcess = new QProcess(this);
    m_downloadProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_downloadProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus);
                if (m_downloadCancelling) return;

                if (!m_downloadStdoutBuffer.isEmpty())
                    onDownloadProcessOutput();

                for (auto &entry : m_downloadEntries) {
                    if (entry.completed || entry.failed || entry.row < 0)
                        continue;

                    QString displayName = entry.filename.isEmpty()
                        ? entry.url.section('/', -1).section('?', 0, 0)
                        : entry.filename;

                    // Try to find the file in the output directory
                    bool fileExists = false;
                    if (!displayName.isEmpty()) {
                        QString filePath = m_downloadOutputDirPath
                            + QStringLiteral("/") + displayName;
                        fileExists = QFileInfo::exists(filePath)
                                     && QFileInfo(filePath).size() > 0;
                    }

                    if (fileExists || exitCode == 0) {
                        entry.completed = true;
                        m_downloadCompleted++;
                        QTableWidgetItem *s = m_downloadProgressTable->item(entry.row, 4);
                        if (s) s->setText(QStringLiteral("已完成"));
                        if (entry.row < m_downloadBars.size())
                            m_downloadBars[entry.row]->setValue(100);
                    } else {
                        entry.failed = true;
                        m_downloadFailed++;
                        QTableWidgetItem *s = m_downloadProgressTable->item(entry.row, 4);
                        if (s) s->setText(QStringLiteral("失败"));
                    }
                }

                m_downloadProgressBar->setValue(100);

                QString statusText = QStringLiteral("下载结束 — 成功: %1").arg(m_downloadCompleted);
                if (m_downloadFailed > 0)
                    statusText += QStringLiteral(" — 失败: %1").arg(m_downloadFailed);
                m_downloadStatusLabel->setText(statusText);

                Utils::logToFile(QStringLiteral("[DOWNLOAD] Finished: ok=%1 fail=%2 code=%3")
                    .arg(m_downloadCompleted).arg(m_downloadFailed).arg(exitCode));

                if (!m_downloadTempFile.isEmpty()) {
                    QFile::remove(m_downloadTempFile);
                    m_downloadTempFile.clear();
                }

                setDownloadUiEnabled(true);
            });

    connect(m_downloadProcess, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
                Q_UNUSED(error);
                if (!m_downloadCancelling)
                    m_downloadStatusLabel->setText(
                        QStringLiteral("进程错误: ") + m_downloadProcess->errorString());
            });

    connect(m_downloadProcess, &QProcess::readyReadStandardOutput,
            this, &MainWindow::onDownloadProcessOutput);

    m_downloadProcess->start(m_aria2Path, args);
    m_downloadStatusLabel->setText(QStringLiteral("正在启动下载..."));

    Utils::logToFile(QStringLiteral("[DOWNLOAD] Start: %1 URLs, dir=%2, conc=%3")
        .arg(m_downloadEntries.size()).arg(outputDir)
        .arg(m_downloadMaxConcurrent->value()));

    setDownloadUiEnabled(false);
}

void MainWindow::onDownloadCancel()
{
    m_downloadCancelling = true;
    m_downloadStatusLabel->setText(QStringLiteral("正在取消..."));
    m_downloadCancelBtn->setEnabled(false);

    if (m_downloadProcess && m_downloadProcess->state() != QProcess::NotRunning) {
        m_downloadProcess->kill();
        m_downloadProcess->waitForFinished(3000);
    }

    if (!m_downloadTempFile.isEmpty()) {
        QFile::remove(m_downloadTempFile);
        m_downloadTempFile.clear();
    }

    Utils::logToFile(QStringLiteral("[DOWNLOAD] Cancelled"));
    setDownloadUiEnabled(true);
    m_downloadStatusLabel->setText(QStringLiteral("已取消"));
}

void MainWindow::onDownloadProcessOutput()
{
    if (!m_downloadProcess) return;

    m_downloadStdoutBuffer += QString::fromUtf8(
        m_downloadProcess->readAllStandardOutput());

    static const QRegularExpression progressRe(
        QStringLiteral("\\[#(\\S+)\\s+\\S+/(\\S+)\\((\\d+)%\\).*SPD:(\\S+)(?:\\s*ETA:(\\S*))?\\]"));

    while (true) {
        int idx = m_downloadStdoutBuffer.indexOf('\n');
        if (idx < 0) {
            idx = m_downloadStdoutBuffer.indexOf('\r');
            if (idx < 0) break;
            if (idx + 1 < m_downloadStdoutBuffer.size()
                && m_downloadStdoutBuffer[idx + 1] == '\n')
                idx++;
        }

        QString line = m_downloadStdoutBuffer.left(idx).trimmed();
        m_downloadStdoutBuffer = m_downloadStdoutBuffer.mid(idx + 1);
        if (line.isEmpty()) continue;

        int dlPos = line.indexOf(QStringLiteral("Downloading:"));
        if (dlPos >= 0) {
            QString url = line.mid(dlPos + 12).trimmed();
            m_downloadPendingUrl = url;
            for (auto &entry : m_downloadEntries) {
                if (entry.row >= 0 && entry.url == url) {
                    QTableWidgetItem *s = m_downloadProgressTable->item(entry.row, 4);
                    if (s && s->text() == QStringLiteral("等待中"))
                        s->setText(QStringLiteral("下载中"));
                    break;
                }
            }
            continue;
        }

        QRegularExpressionMatch m = progressRe.match(line);
        if (m.hasMatch()) {
            QString gid = m.captured(1);
            int percent = m.captured(3).toInt();
            QString speed = m.captured(4);
            QString eta = m.captured(5);

            int entryIdx = -1;
            if (m_gidToIndex.contains(gid)) {
                entryIdx = m_gidToIndex[gid];
            } else if (!m_downloadPendingUrl.isEmpty()) {
                for (int i = 0; i < m_downloadEntries.size(); i++) {
                    if (m_downloadEntries[i].row >= 0
                        && m_downloadEntries[i].url == m_downloadPendingUrl) {
                        m_gidToIndex[gid] = i;
                        entryIdx = i;
                        m_downloadPendingUrl.clear();
                        break;
                    }
                }
            }

            if (entryIdx < 0 && !m_gidToIndex.isEmpty()) {
                // Fallback: if all known GIDs are done, assign new GID to first non-completed entry
                for (int i = 0; i < m_downloadEntries.size(); i++) {
                    if (m_downloadEntries[i].row >= 0 && !m_downloadEntries[i].completed) {
                        bool alreadyMapped = false;
                        for (auto it = m_gidToIndex.cbegin(); it != m_gidToIndex.cend(); ++it) {
                            if (it.value() == i) { alreadyMapped = true; break; }
                        }
                        if (!alreadyMapped) {
                            m_gidToIndex[gid] = i;
                            entryIdx = i;
                            break;
                        }
                    }
                }
            }

            if (entryIdx < 0 || entryIdx >= m_downloadEntries.size()) continue;

            DownloadEntry &entry = m_downloadEntries[entryIdx];
            if (entry.row < 0 || entry.completed) continue;

            QTableWidgetItem *statusItem = m_downloadProgressTable->item(entry.row, 4);
            if (statusItem && statusItem->text() == QStringLiteral("等待中"))
                statusItem->setText(QStringLiteral("下载中"));

            QProgressBar *bar = entry.row < m_downloadBars.size()
                ? m_downloadBars[entry.row] : nullptr;
            if (bar && bar->value() < 100)
                bar->setValue(percent);

            QTableWidgetItem *speedItem = m_downloadProgressTable->item(entry.row, 2);
            if (speedItem) speedItem->setText(speed);

            QTableWidgetItem *etaItem = m_downloadProgressTable->item(entry.row, 3);
            if (etaItem) {
                etaItem->setText(
                    eta.isEmpty() || eta == QStringLiteral("-") ? QStringLiteral("—") : eta);
            }

            if (percent >= 100 && !entry.completed) {
                entry.completed = true;
                m_downloadCompleted++;
                if (statusItem) statusItem->setText(QStringLiteral("已完成"));
                Utils::logToFile(QStringLiteral("[DOWNLOAD] OK: %1").arg(entry.url));
            }

            int totalPercent = 0;
            for (const auto &e : m_downloadEntries) {
                if (e.row >= 0 && e.row < m_downloadBars.size()) {
                    totalPercent += m_downloadBars[e.row]->value();
                }
            }
            int count = m_downloadEntries.size();
            m_downloadProgressBar->setValue(count > 0 ? totalPercent / count : 0);

            QString statusText = QStringLiteral("下载中 — 完成: %1/%2")
                .arg(m_downloadCompleted).arg(m_downloadEntries.size());
            if (m_downloadFailed > 0)
                statusText += QStringLiteral(" — 失败: %1").arg(m_downloadFailed);
            m_downloadStatusLabel->setText(statusText);
            continue;
        }

        if (line.contains(QStringLiteral("[ERROR]"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("error:"), Qt::CaseInsensitive)) {
            Utils::logToFile(QStringLiteral("[DOWNLOAD] ") + line);
        }
    }
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
