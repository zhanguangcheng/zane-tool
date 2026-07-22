#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QHeaderView>
#include <QDateTimeEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QRandomGenerator>
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

#include "third_party/qrcodegen.hpp"
#include "third_party/quirc.h"

MainWindow::MainWindow(const QString &ffmpegPath, const QString &aria2Path, const QString &mkcertPath, QWidget *parent)
    : QMainWindow(parent)
    , m_ffmpegPath(ffmpegPath)
    , m_aria2Path(aria2Path)
    , m_mkcertPath(mkcertPath)
    , m_downloadProcess(nullptr)
    , m_certProcess(nullptr)
    , m_certRunning(false)
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

    connect(m_screenshotTool, &ScreenshotTool::qrScreenshotCaptured,
            this, [this](const QImage &image) {
        processQrDecodeImage(image, QStringLiteral("屏幕截图"));
    });
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
    m_stackedWidget->addWidget(createRandomStringPage());
    m_stackedWidget->addWidget(createQrCodePage());
    m_stackedWidget->addWidget(createCertPage());
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
    if (m_certProcess && m_certProcess->state() != QProcess::NotRunning) {
        m_certProcess->kill();
        m_certProcess->waitForFinished(3000);
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
    if (obj == m_qrDecDropZone) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *de = static_cast<QDragEnterEvent *>(event);
            if (de->mimeData()->hasUrls())
                de->acceptProposedAction();
            return true;
        }
        if (event->type() == QEvent::Drop) {
            QDropEvent *de = static_cast<QDropEvent *>(event);
            const QMimeData *mimeData = de->mimeData();
            if (mimeData->hasUrls()) {
                for (const QUrl &url : mimeData->urls()) {
                    if (url.isLocalFile()) {
                        QString filePath = url.toLocalFile();
                        QImage image(filePath);
                        if (image.isNull()) {
                            QMessageBox::warning(this, QStringLiteral("错误"),
                                QStringLiteral("无法加载图片：%1").arg(filePath));
                            return true;
                        }
                        m_qrDecFilePath->setText(QDir::toNativeSeparators(filePath));
                        processQrDecodeImage(image, QFileInfo(filePath).fileName());
                        return true;
                    }
                }
            }
        }
    }
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

QWidget *MainWindow::createRandomStringPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *charGroup = new QGroupBox(QStringLiteral("字符集合"), page);
    QVBoxLayout *charLayout = new QVBoxLayout(charGroup);
    charLayout->setSpacing(10);

    QGridLayout *checkGrid = new QGridLayout();
    checkGrid->setSpacing(8);
    checkGrid->setColumnStretch(1, 1);
    checkGrid->setColumnStretch(3, 1);

    m_randomUpperCheck = new QCheckBox(QStringLiteral("A-Z 大写字母"), charGroup);
    m_randomUpperCheck->setChecked(true);
    m_randomLowerCheck = new QCheckBox(QStringLiteral("a-z 小写字母"), charGroup);
    m_randomLowerCheck->setChecked(true);
    m_randomDigitCheck = new QCheckBox(QStringLiteral("0-9 数字"), charGroup);
    m_randomDigitCheck->setChecked(true);
    m_randomSymbolCheck = new QCheckBox(QStringLiteral("!@#$ 特殊符号"), charGroup);

    checkGrid->addWidget(m_randomUpperCheck, 0, 0);
    checkGrid->addWidget(m_randomLowerCheck, 0, 2);
    checkGrid->addWidget(m_randomDigitCheck, 1, 0);
    checkGrid->addWidget(m_randomSymbolCheck, 1, 2);

    QHBoxLayout *excludeRow = new QHBoxLayout();
    excludeRow->setSpacing(8);
    QLabel *excludeLabel = new QLabel(QStringLiteral("排除字符:"), charGroup);
    m_randomExcludeEdit = new QLineEdit(charGroup);
    m_randomExcludeEdit->setPlaceholderText(QStringLiteral("例如: 0O1lI  (这些字符不会出现在结果中)"));
    m_randomExcludeEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    excludeRow->addWidget(excludeLabel);
    excludeRow->addWidget(m_randomExcludeEdit, 1);

    charLayout->addLayout(checkGrid);
    charLayout->addLayout(excludeRow);

    QGroupBox *configGroup = new QGroupBox(QStringLiteral("生成设置"), page);
    QHBoxLayout *configLayout = new QHBoxLayout(configGroup);
    configLayout->setSpacing(20);

    QHBoxLayout *lengthRow = new QHBoxLayout();
    lengthRow->setSpacing(6);
    QLabel *lengthLabel = new QLabel(QStringLiteral("字符串长度:"), configGroup);
    m_randomLengthSpin = new QSpinBox(configGroup);
    m_randomLengthSpin->setRange(1, 256);
    m_randomLengthSpin->setValue(16);
    m_randomLengthSpin->setFixedWidth(80);
    lengthRow->addWidget(lengthLabel);
    lengthRow->addWidget(m_randomLengthSpin);

    QHBoxLayout *countRow = new QHBoxLayout();
    countRow->setSpacing(6);
    QLabel *countLabel = new QLabel(QStringLiteral("生成数量:"), configGroup);
    m_randomCountSpin = new QSpinBox(configGroup);
    m_randomCountSpin->setRange(1, 1000);
    m_randomCountSpin->setValue(10);
    m_randomCountSpin->setFixedWidth(80);
    countRow->addWidget(countLabel);
    countRow->addWidget(m_randomCountSpin);

    m_randomGenerateBtn = new QPushButton(QStringLiteral("生成随机字符串"), configGroup);
    m_randomGenerateBtn->setCursor(Qt::PointingHandCursor);
    m_randomGenerateBtn->setFixedHeight(32);
    connect(m_randomGenerateBtn, &QPushButton::clicked, this, &MainWindow::onRandomGenerate);

    m_randomCopyBtn = new QPushButton(QStringLiteral("复制全部"), configGroup);
    m_randomCopyBtn->setCursor(Qt::PointingHandCursor);
    m_randomCopyBtn->setFixedHeight(32);
    m_randomCopyBtn->setFixedWidth(100);
    m_randomCopyBtn->setEnabled(false);
    connect(m_randomCopyBtn, &QPushButton::clicked, this, &MainWindow::onRandomCopy);

    configLayout->addLayout(lengthRow);
    configLayout->addLayout(countRow);
    configLayout->addStretch();
    configLayout->addWidget(m_randomGenerateBtn);
    configLayout->addWidget(m_randomCopyBtn);

    QGroupBox *resultGroup = new QGroupBox(QStringLiteral("结果"), page);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setSpacing(0);

    m_randomOutput = new QTextEdit(resultGroup);
    m_randomOutput->setReadOnly(true);
    m_randomOutput->setStyleSheet(QStringLiteral(
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
    m_randomOutput->setMinimumHeight(200);
    m_randomOutput->setPlaceholderText(QStringLiteral("点击「生成随机字符串」按钮生成结果"));

    resultLayout->addWidget(m_randomOutput);

    mainLayout->addWidget(charGroup);
    mainLayout->addWidget(configGroup);
    mainLayout->addWidget(resultGroup, 1);

    return page;
}

void MainWindow::onRandomGenerate()
{
    QString chars;
    if (m_randomUpperCheck->isChecked())
        chars += QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (m_randomLowerCheck->isChecked())
        chars += QStringLiteral("abcdefghijklmnopqrstuvwxyz");
    if (m_randomDigitCheck->isChecked())
        chars += QStringLiteral("0123456789");
    if (m_randomSymbolCheck->isChecked())
        chars += QStringLiteral("!@#$%^&*()-_=+[]{};:'\",.<>?/\\|`~");

    QString excludes = m_randomExcludeEdit->text();
    for (const QChar &ch : excludes)
        chars.remove(ch);

    if (chars.isEmpty()) {
        m_randomOutput->setPlainText(QStringLiteral("请至少选择一种字符类型"));
        m_randomCopyBtn->setEnabled(false);
        return;
    }

    int length = m_randomLengthSpin->value();
    int count = m_randomCountSpin->value();
    QRandomGenerator *rng = QRandomGenerator::global();

    QStringList results;
    results.reserve(count);
    int charCount = chars.size();
    for (int i = 0; i < count; ++i) {
        QString str;
        str.reserve(length);
        for (int j = 0; j < length; ++j)
            str.append(chars.at(rng->bounded(charCount)));
        results.append(str);
    }

    m_randomOutput->setPlainText(results.join(QStringLiteral("\n")));
    m_randomCopyBtn->setEnabled(true);
}

void MainWindow::onRandomCopy()
{
    QString text = m_randomOutput->toPlainText();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_randomCopyBtn->text();
    m_randomCopyBtn->setText(QStringLiteral("已复制"));
    m_randomCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_randomCopyBtn->setText(original);
        m_randomCopyBtn->setEnabled(true);
    });
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

    if (pageIndex == 13) {
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                QImage image(filePath);
                if (!image.isNull()) {
                    m_qrDecFilePath->setText(QDir::toNativeSeparators(filePath));
                    processQrDecodeImage(image, QFileInfo(filePath).fileName());
                }
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

// ==================== QR Code Page ====================

QWidget *MainWindow::createQrCodePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QTabWidget *tabs = new QTabWidget(page);
    tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane {"
        "  border: 1px solid #ced4da;"
        "  border-radius: 4px;"
        "  background-color: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  padding: 8px 20px;"
        "  border: 1px solid #ced4da;"
        "  border-bottom: none;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "  background-color: #f1f3f5;"
        "  color: #495057;"
        "  font-size: 13px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #ffffff;"
        "  color: #0d6efd;"
        "  font-weight: bold;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background-color: #e9ecef;"
        "}"));

    // ---- Generate tab ----
    QWidget *genTab = new QWidget(tabs);
    QVBoxLayout *genLayout = new QVBoxLayout(genTab);
    genLayout->setSpacing(12);
    genLayout->setContentsMargins(0, 12, 0, 12);

    QGroupBox *inputGroup = new QGroupBox(QStringLiteral("内容"), genTab);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(10);

    m_qrGenInput = new QTextEdit(inputGroup);
    m_qrGenInput->setPlaceholderText(QStringLiteral("输入要生成二维码的文本或链接..."));
    m_qrGenInput->setMaximumHeight(100);
    m_qrGenInput->setAcceptRichText(false);
    m_qrGenInput->setStyleSheet(QStringLiteral(
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
    connect(m_qrGenInput, &QTextEdit::textChanged, this, &MainWindow::onQrGenerate);

    QHBoxLayout *optionRow = new QHBoxLayout();
    optionRow->setSpacing(8);

    QLabel *eccLabel = new QLabel(QStringLiteral("纠错等级:"), inputGroup);
    m_qrGenEccCombo = new QComboBox(inputGroup);
    m_qrGenEccCombo->addItem(QStringLiteral("L (7%)"), 0);
    m_qrGenEccCombo->addItem(QStringLiteral("M (15%)"), 1);
    m_qrGenEccCombo->addItem(QStringLiteral("Q (25%)"), 2);
    m_qrGenEccCombo->addItem(QStringLiteral("H (30%)"), 3);
    m_qrGenEccCombo->setCurrentIndex(1);
    connect(m_qrGenEccCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onQrGenerate);

    QLabel *scaleLabel = new QLabel(QStringLiteral("尺寸:"), inputGroup);
    m_qrGenScaleSpin = new QSpinBox(inputGroup);
    m_qrGenScaleSpin->setRange(2, 10);
    m_qrGenScaleSpin->setValue(4);
    m_qrGenScaleSpin->setSuffix(QStringLiteral(" px/格"));
    connect(m_qrGenScaleSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onQrGenerate);

    optionRow->addWidget(eccLabel);
    optionRow->addWidget(m_qrGenEccCombo);
    optionRow->addSpacing(16);
    optionRow->addWidget(scaleLabel);
    optionRow->addWidget(m_qrGenScaleSpin);
    optionRow->addStretch(1);

    inputLayout->addWidget(m_qrGenInput);
    inputLayout->addLayout(optionRow);

    QGroupBox *previewGroup = new QGroupBox(QStringLiteral("预览"), genTab);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setSpacing(10);

    m_qrGenPreview = new QLabel(previewGroup);
    m_qrGenPreview->setAlignment(Qt::AlignCenter);
    m_qrGenPreview->setMinimumHeight(260);
    m_qrGenPreview->setStyleSheet(QStringLiteral(
        "QLabel { border: 1px solid #ced4da; border-radius: 6px; background-color: #ffffff; }"));
    m_qrGenPreview->setText(QStringLiteral("输入内容后自动生成二维码"));

    m_qrGenStatusLabel = new QLabel(previewGroup);
    m_qrGenStatusLabel->setAlignment(Qt::AlignCenter);
    m_qrGenStatusLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));

    QHBoxLayout *genBtnRow = new QHBoxLayout();
    genBtnRow->setSpacing(8);

    m_qrGenSaveBtn = new QPushButton(QStringLiteral("保存为 PNG"), previewGroup);
    m_qrGenSaveBtn->setFixedHeight(34);
    m_qrGenSaveBtn->setCursor(Qt::PointingHandCursor);
    m_qrGenSaveBtn->setEnabled(false);
    connect(m_qrGenSaveBtn, &QPushButton::clicked, this, &MainWindow::onQrSaveImage);

    m_qrGenCopyBtn = new QPushButton(QStringLiteral("复制到剪贴板"), previewGroup);
    m_qrGenCopyBtn->setFixedHeight(34);
    m_qrGenCopyBtn->setCursor(Qt::PointingHandCursor);
    m_qrGenCopyBtn->setEnabled(false);
    connect(m_qrGenCopyBtn, &QPushButton::clicked, this, &MainWindow::onQrCopyImage);

    genBtnRow->addStretch(1);
    genBtnRow->addWidget(m_qrGenSaveBtn);
    genBtnRow->addWidget(m_qrGenCopyBtn);

    previewLayout->addWidget(m_qrGenPreview, 1);
    previewLayout->addWidget(m_qrGenStatusLabel);
    previewLayout->addLayout(genBtnRow);

    genLayout->addWidget(inputGroup);
    genLayout->addWidget(previewGroup, 1);

    // ---- Decode tab ----
    QWidget *decTab = new QWidget(tabs);
    QVBoxLayout *decLayout = new QVBoxLayout(decTab);
    decLayout->setSpacing(12);
    decLayout->setContentsMargins(0, 12, 0, 12);

    QGroupBox *sourceGroup = new QGroupBox(QStringLiteral("选择图片来源"), decTab);
    QVBoxLayout *sourceLayout = new QVBoxLayout(sourceGroup);
    sourceLayout->setSpacing(10);

    QHBoxLayout *fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);

    m_qrDecFilePath = new QLineEdit(sourceGroup);
    m_qrDecFilePath->setReadOnly(true);
    m_qrDecFilePath->setPlaceholderText(QStringLiteral("请选择图片文件，或使用屏幕识别 / 拖放图片..."));

    m_qrDecSelectBtn = new QPushButton(QStringLiteral("选择图片"), sourceGroup);
    m_qrDecSelectBtn->setFixedHeight(34);
    m_qrDecSelectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_qrDecSelectBtn, &QPushButton::clicked, this, &MainWindow::onQrSelectImage);

    m_qrDecScreenBtn = new QPushButton(QStringLiteral("屏幕识别"), sourceGroup);
    m_qrDecScreenBtn->setFixedHeight(34);
    m_qrDecScreenBtn->setCursor(Qt::PointingHandCursor);
    connect(m_qrDecScreenBtn, &QPushButton::clicked, this, &MainWindow::onQrScreenCapture);

    fileRow->addWidget(m_qrDecFilePath, 1);
    fileRow->addWidget(m_qrDecSelectBtn);
    fileRow->addWidget(m_qrDecScreenBtn);

    m_qrDecDropZone = new QLabel(sourceGroup);
    m_qrDecDropZone->setFixedHeight(70);
    m_qrDecDropZone->setAlignment(Qt::AlignCenter);
    m_qrDecDropZone->setAcceptDrops(true);
    m_qrDecDropZone->setCursor(Qt::PointingHandCursor);
    m_qrDecDropZone->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  border: 2px dashed #ced4da;"
        "  border-radius: 8px;"
        "  background-color: #f8f9fa;"
        "  color: #6c757d;"
        "  font-size: 14px;"
        "}"
        "QLabel:hover {"
        "  border-color: #0d6efd;"
        "  color: #0d6efd;"
        "  background-color: #e7f1ff;"
        "}"));
    m_qrDecDropZone->setText(QStringLiteral("将图片拖放到此处进行识别"));
    m_qrDecDropZone->installEventFilter(this);

    sourceLayout->addLayout(fileRow);
    sourceLayout->addWidget(m_qrDecDropZone);

    QGroupBox *resultGroup = new QGroupBox(QStringLiteral("识别结果"), decTab);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setSpacing(10);

    QHBoxLayout *resultBody = new QHBoxLayout();
    resultBody->setSpacing(12);

    m_qrDecPreview = new QLabel(resultGroup);
    m_qrDecPreview->setFixedSize(200, 200);
    m_qrDecPreview->setAlignment(Qt::AlignCenter);
    m_qrDecPreview->setStyleSheet(QStringLiteral(
        "QLabel { border: 1px solid #ced4da; border-radius: 6px; background-color: #f8f9fa; color: #6c757d; }"));
    m_qrDecPreview->setText(QStringLiteral("图片预览"));

    m_qrDecOutput = new QTextEdit(resultGroup);
    m_qrDecOutput->setReadOnly(true);
    m_qrDecOutput->setPlaceholderText(QStringLiteral("识别结果将在此显示..."));
    m_qrDecOutput->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  border: 1px solid #ced4da;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  background-color: #ffffff;"
        "  color: #212529;"
        "}"
        "QTextEdit:focus {"
        "  border-color: #86b7fe;"
        "}"));

    resultBody->addWidget(m_qrDecPreview);
    resultBody->addWidget(m_qrDecOutput, 1);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(8);

    m_qrDecInfoLabel = new QLabel(resultGroup);
    m_qrDecInfoLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));

    m_qrDecCopyBtn = new QPushButton(QStringLiteral("复制到剪贴板"), resultGroup);
    m_qrDecCopyBtn->setFixedHeight(34);
    m_qrDecCopyBtn->setCursor(Qt::PointingHandCursor);
    m_qrDecCopyBtn->setEnabled(false);
    connect(m_qrDecCopyBtn, &QPushButton::clicked, this, &MainWindow::onQrCopyResult);

    m_qrDecOpenBtn = new QPushButton(QStringLiteral("打开链接"), resultGroup);
    m_qrDecOpenBtn->setFixedHeight(34);
    m_qrDecOpenBtn->setCursor(Qt::PointingHandCursor);
    m_qrDecOpenBtn->setEnabled(false);
    connect(m_qrDecOpenBtn, &QPushButton::clicked, this, &MainWindow::onQrOpenLink);

    bottomRow->addWidget(m_qrDecInfoLabel, 1);
    bottomRow->addWidget(m_qrDecCopyBtn);
    bottomRow->addWidget(m_qrDecOpenBtn);

    resultLayout->addLayout(resultBody, 1);
    resultLayout->addLayout(bottomRow);

    decLayout->addWidget(sourceGroup);
    decLayout->addWidget(resultGroup, 1);

    tabs->addTab(genTab, QStringLiteral("生成"));
    tabs->addTab(decTab, QStringLiteral("识别"));

    mainLayout->addWidget(tabs, 1);

    return page;
}

void MainWindow::onQrGenerate()
{
    QString text = m_qrGenInput->toPlainText();
    if (text.isEmpty()) {
        m_qrGenPixmap = QPixmap();
        m_qrGenPreview->setPixmap(QPixmap());
        m_qrGenPreview->setText(QStringLiteral("输入内容后自动生成二维码"));
        m_qrGenStatusLabel->clear();
        m_qrGenSaveBtn->setEnabled(false);
        m_qrGenCopyBtn->setEnabled(false);
        return;
    }

    static const qrcodegen::QrCode::Ecc eccTable[] = {
        qrcodegen::QrCode::Ecc::LOW,
        qrcodegen::QrCode::Ecc::MEDIUM,
        qrcodegen::QrCode::Ecc::QUARTILE,
        qrcodegen::QrCode::Ecc::HIGH,
    };
    int eccIndex = m_qrGenEccCombo->currentData().toInt();
    qrcodegen::QrCode::Ecc ecc = eccTable[qBound(0, eccIndex, 3)];

    try {
        QByteArray utf8 = text.toUtf8();
        qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(utf8.constData(), ecc);

        int scale = m_qrGenScaleSpin->value();
        int border = 4;
        int modules = qr.getSize();
        int imgSize = (modules + border * 2) * scale;

        QImage img(imgSize, imgSize, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.fillRect(0, 0, imgSize, imgSize, Qt::white);
        for (int y = 0; y < modules; ++y) {
            for (int x = 0; x < modules; ++x) {
                if (qr.getModule(x, y))
                    painter.fillRect((x + border) * scale, (y + border) * scale, scale, scale, Qt::black);
            }
        }
        painter.end();

        m_qrGenPixmap = QPixmap::fromImage(img);
        m_qrGenPreview->setPixmap(m_qrGenPixmap.scaled(m_qrGenPreview->size(),
            Qt::KeepAspectRatio, Qt::FastTransformation));
        m_qrGenStatusLabel->setText(QStringLiteral("版本 %1 | %2×%3 模块 | 输出 %4×%4 px")
            .arg(qr.getVersion()).arg(modules).arg(modules).arg(imgSize));
        m_qrGenSaveBtn->setEnabled(true);
        m_qrGenCopyBtn->setEnabled(true);
    } catch (const std::length_error &) {
        m_qrGenPixmap = QPixmap();
        m_qrGenPreview->setPixmap(QPixmap());
        m_qrGenPreview->setText(QStringLiteral("内容过长，无法生成"));
        m_qrGenStatusLabel->setText(QStringLiteral("请缩短内容或降低纠错等级"));
        m_qrGenSaveBtn->setEnabled(false);
        m_qrGenCopyBtn->setEnabled(false);
    }
}

void MainWindow::onQrSaveImage()
{
    if (m_qrGenPixmap.isNull())
        return;
    QString filePath = QFileDialog::getSaveFileName(this,
        QStringLiteral("保存二维码"), QStringLiteral("qrcode.png"),
        QStringLiteral("PNG 图片 (*.png)"));
    if (filePath.isEmpty())
        return;
    if (!m_qrGenPixmap.save(filePath, "PNG")) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("保存失败：%1").arg(filePath));
    }
}

void MainWindow::onQrCopyImage()
{
    if (m_qrGenPixmap.isNull())
        return;
    QApplication::clipboard()->setPixmap(m_qrGenPixmap);
    QString original = m_qrGenCopyBtn->text();
    m_qrGenCopyBtn->setText(QStringLiteral("已复制"));
    m_qrGenCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_qrGenCopyBtn->setText(original);
        m_qrGenCopyBtn->setEnabled(true);
    });
}

void MainWindow::onQrSelectImage()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        QStringLiteral("选择图片文件"), QString(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp *.gif *.ico *.tiff *.tif)"));
    if (filePath.isEmpty())
        return;
    QImage image(filePath);
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法加载图片：%1").arg(filePath));
        return;
    }
    m_qrDecFilePath->setText(QDir::toNativeSeparators(filePath));
    processQrDecodeImage(image, QFileInfo(filePath).fileName());
}

void MainWindow::onQrScreenCapture()
{
    m_screenshotTool->startScreenshotForQr();
}

void MainWindow::processQrDecodeImage(const QImage &image, const QString &sourceDesc)
{
    m_qrDecPreview->setPixmap(QPixmap::fromImage(image).scaled(200, 200,
        Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    int w = gray.width();
    int h = gray.height();

    QStringList results;
    quirc *q = quirc_new();
    if (q && quirc_resize(q, w, h) == 0) {
        int qw, qh;
        uint8_t *buf = quirc_begin(q, &qw, &qh);
        for (int y = 0; y < h; ++y)
            memcpy(buf + y * qw, gray.constScanLine(y), w);
        quirc_end(q);

        int count = quirc_count(q);
        for (int i = 0; i < count; ++i) {
            quirc_code code;
            quirc_data data;
            quirc_extract(q, i, &code);
            if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
                results << QString::fromUtf8(
                    reinterpret_cast<const char *>(data.payload), data.payload_len);
            }
        }
    }
    if (q)
        quirc_destroy(q);

    if (results.isEmpty()) {
        m_qrDecOutput->setPlainText(QStringLiteral("未识别到二维码"));
        m_qrDecInfoLabel->setText(QStringLiteral("来源: %1").arg(sourceDesc));
        m_qrDecCopyBtn->setEnabled(false);
        m_qrDecOpenBtn->setEnabled(false);
        return;
    }

    QStringList numbered;
    for (int i = 0; i < results.size(); ++i) {
        if (results.size() > 1)
            numbered << QStringLiteral("[%1] %2").arg(i + 1).arg(results[i]);
        else
            numbered << results[i];
    }
    m_qrDecOutput->setPlainText(numbered.join(QStringLiteral("\n\n")));
    m_qrDecInfoLabel->setText(QStringLiteral("来源: %1 | 识别到 %2 个二维码")
        .arg(sourceDesc).arg(results.size()));
    m_qrDecCopyBtn->setEnabled(true);

    bool isUrl = results.first().startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
              || results.first().startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    m_qrDecOpenBtn->setEnabled(isUrl);
}

void MainWindow::onQrCopyResult()
{
    QString text = m_qrDecOutput->toPlainText();
    if (text.isEmpty() || text == QStringLiteral("未识别到二维码"))
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_qrDecCopyBtn->text();
    m_qrDecCopyBtn->setText(QStringLiteral("已复制"));
    m_qrDecCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_qrDecCopyBtn->setText(original);
        m_qrDecCopyBtn->setEnabled(true);
    });
}

void MainWindow::onQrOpenLink()
{
    QString text = m_qrDecOutput->toPlainText();
    if (text.startsWith(QStringLiteral("[1] ")))
        text = text.mid(4).section(QStringLiteral("\n"), 0, 0);
    QUrl url(text.trimmed());
    if (url.isValid())
        QDesktopServices::openUrl(url);
}

// ==================== Cert (HTTPS证书) Page ====================

QWidget *MainWindow::createCertPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(10);

    QGroupBox *caGroup = new QGroupBox(QStringLiteral("本地 CA（根证书）"), page);
    QVBoxLayout *caLayout = new QVBoxLayout(caGroup);

    m_certCaStatusLabel = new QLabel(QStringLiteral("状态: 检测中…"), caGroup);
    caLayout->addWidget(m_certCaStatusLabel);

    QHBoxLayout *carootLayout = new QHBoxLayout();
    carootLayout->addWidget(new QLabel(QStringLiteral("CA 目录:"), caGroup));
    m_certCarootLabel = new QLabel(QStringLiteral("-"), caGroup);
    m_certCarootLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_certCarootLabel->setStyleSheet(QStringLiteral("color:#6c757d;"));
    carootLayout->addWidget(m_certCarootLabel, 1);
    caLayout->addLayout(carootLayout);

    QHBoxLayout *caBtnLayout = new QHBoxLayout();
    m_certInstallCaBtn = new QPushButton(QStringLiteral("安装到系统信任"), caGroup);
    m_certUninstallCaBtn = new QPushButton(QStringLiteral("卸载根证书"), caGroup);
    m_certUninstallCaBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_certOpenCarootBtn = new QPushButton(QStringLiteral("打开 CA 目录"), caGroup);
    connect(m_certInstallCaBtn, &QPushButton::clicked, this, &MainWindow::onCertInstallCa);
    connect(m_certUninstallCaBtn, &QPushButton::clicked, this, &MainWindow::onCertUninstallCa);
    connect(m_certOpenCarootBtn, &QPushButton::clicked, this, &MainWindow::onCertOpenCaroot);
    caBtnLayout->addWidget(m_certInstallCaBtn);
    caBtnLayout->addWidget(m_certUninstallCaBtn);
    caBtnLayout->addWidget(m_certOpenCarootBtn);
    caBtnLayout->addStretch();
    caLayout->addLayout(caBtnLayout);

    QLabel *caHint = new QLabel(QStringLiteral("安装/卸载需要管理员权限，将弹出 UAC 授权窗口。"), caGroup);
    caHint->setStyleSheet(QStringLiteral("color:#6c757d;"));
    caLayout->addWidget(caHint);

    QGroupBox *genGroup = new QGroupBox(QStringLiteral("生成证书"), page);
    QVBoxLayout *genLayout = new QVBoxLayout(genGroup);

    genLayout->addWidget(new QLabel(QStringLiteral("域名 / IP（每行一个，支持通配符 *.example.com）:"), genGroup));
    m_certDomainsInput = new QTextEdit(genGroup);
    m_certDomainsInput->setPlaceholderText(QStringLiteral("localhost\n127.0.0.1\n::1\n*.example.com"));
    m_certDomainsInput->setAcceptRichText(false);
    m_certDomainsInput->setMaximumHeight(90);
    m_certDomainsInput->setStyleSheet(QStringLiteral(
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
    genLayout->addWidget(m_certDomainsInput);

    QHBoxLayout *quickLayout = new QHBoxLayout();
    quickLayout->addWidget(new QLabel(QStringLiteral("快速添加:"), genGroup));
    const QStringList quickNames = {
        QStringLiteral("localhost"), QStringLiteral("127.0.0.1"),
        QStringLiteral("::1"), QStringLiteral("*.localhost")
    };
    for (const QString &name : quickNames) {
        QPushButton *btn = new QPushButton(name, genGroup);
        connect(btn, &QPushButton::clicked, this, [this, name]() {
            const QStringList lines = m_certDomainsInput->toPlainText().split('\n', Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                if (line.trimmed() == name)
                    return;
            }
            m_certDomainsInput->append(name);
        });
        quickLayout->addWidget(btn);
    }
    quickLayout->addStretch();
    genLayout->addLayout(quickLayout);

    QHBoxLayout *nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(QStringLiteral("文件名:"), genGroup));
    m_certNameEdit = new QLineEdit(QStringLiteral("dev"), genGroup);
    m_certNameEdit->setPlaceholderText(QStringLiteral("dev → dev.pem / dev-key.pem"));
    nameLayout->addWidget(m_certNameEdit, 1);
    genLayout->addLayout(nameLayout);

    QHBoxLayout *dirLayout = new QHBoxLayout();
    dirLayout->addWidget(new QLabel(QStringLiteral("输出目录:"), genGroup));
    m_certOutputDir = new QLineEdit(genGroup);
    m_certOutputDir->setPlaceholderText(QStringLiteral("必填，点击「浏览」选择证书输出目录"));
    m_certOutputBrowseBtn = new QPushButton(QStringLiteral("浏览"), genGroup);
    connect(m_certOutputBrowseBtn, &QPushButton::clicked, this, &MainWindow::onCertOutputBrowse);
    dirLayout->addWidget(m_certOutputDir, 1);
    dirLayout->addWidget(m_certOutputBrowseBtn);
    genLayout->addLayout(dirLayout);

    QHBoxLayout *genBtnLayout = new QHBoxLayout();
    m_certGenerateBtn = new QPushButton(QStringLiteral("生成证书"), genGroup);
    m_certOpenOutputBtn = new QPushButton(QStringLiteral("打开输出目录"), genGroup);
    connect(m_certGenerateBtn, &QPushButton::clicked, this, &MainWindow::onCertGenerate);
    connect(m_certOpenOutputBtn, &QPushButton::clicked, this, &MainWindow::onCertOpenOutputDir);
    genBtnLayout->addWidget(m_certGenerateBtn);
    genBtnLayout->addWidget(m_certOpenOutputBtn);
    genBtnLayout->addStretch();
    genLayout->addLayout(genBtnLayout);

    QGroupBox *logGroup = new QGroupBox(QStringLiteral("日志"), page);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_certLogOutput = new QTextEdit(logGroup);
    m_certLogOutput->setReadOnly(true);
    m_certLogOutput->setStyleSheet(QStringLiteral(
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
    logLayout->addWidget(m_certLogOutput);

    mainLayout->addWidget(caGroup);
    mainLayout->addWidget(genGroup);
    mainLayout->addWidget(logGroup, 1);

    refreshCertCaStatus();

    return page;
}

void MainWindow::refreshCertCaStatus()
{
    QProcess proc;
    proc.start(m_mkcertPath, {QStringLiteral("-CAROOT")});
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        proc.waitForFinished(1000);
        m_certCaStatusLabel->setText(QStringLiteral("状态: 检测失败"));
        return;
    }

    m_certCarootPath = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    m_certCarootLabel->setText(m_certCarootPath.isEmpty() ? QStringLiteral("-") : m_certCarootPath);
    m_certCarootLabel->setToolTip(m_certCarootPath);
    m_certOpenCarootBtn->setEnabled(!m_certCarootPath.isEmpty());

    bool caExists = !m_certCarootPath.isEmpty()
        && QFileInfo::exists(m_certCarootPath + QStringLiteral("/rootCA.pem"));
    if (caExists) {
        m_certCaStatusLabel->setText(QStringLiteral(
            "状态: <span style='color:#198754;font-weight:bold;'>CA 已生成</span>"));
    } else {
        m_certCaStatusLabel->setText(QStringLiteral(
            "状态: <span style='color:#dc3545;font-weight:bold;'>CA 未生成</span>"
            "（点击「安装到系统信任」创建并信任）"));
    }
}

void MainWindow::onCertInstallCa()
{
    QString path = QDir::toNativeSeparators(m_mkcertPath);
    QString args = QStringLiteral("-install");

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = reinterpret_cast<HWND>(winId());
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(path.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(args.utf16());
    sei.nShow = SW_SHOW;

    if (!ShellExecuteExW(&sei)) {
        if (GetLastError() != ERROR_CANCELLED) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                QStringLiteral("无法以管理员身份启动 mkcert"));
        }
        return;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 60000);
        CloseHandle(sei.hProcess);
    }

    refreshCertCaStatus();
    m_certLogOutput->append(QStringLiteral("> mkcert -install（已请求管理员权限）"));
    Utils::logToFile(QStringLiteral("[CERT] mkcert -install requested"));
}

void MainWindow::onCertUninstallCa()
{
    auto ret = QMessageBox::question(this, QStringLiteral("确认"),
        QStringLiteral("确定要从系统信任库卸载本地 CA 根证书吗？"));
    if (ret != QMessageBox::Yes)
        return;

    QString path = QDir::toNativeSeparators(m_mkcertPath);
    QString args = QStringLiteral("-uninstall");

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = reinterpret_cast<HWND>(winId());
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(path.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(args.utf16());
    sei.nShow = SW_SHOW;

    if (!ShellExecuteExW(&sei)) {
        if (GetLastError() != ERROR_CANCELLED) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                QStringLiteral("无法以管理员身份启动 mkcert"));
        }
        return;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 60000);
        CloseHandle(sei.hProcess);
    }

    refreshCertCaStatus();
    m_certLogOutput->append(QStringLiteral("> mkcert -uninstall（已请求管理员权限）"));
    Utils::logToFile(QStringLiteral("[CERT] mkcert -uninstall requested"));
}

void MainWindow::onCertOutputBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"),
                                                    m_certOutputDir->text());
    if (!dir.isEmpty())
        m_certOutputDir->setText(dir);
}

void MainWindow::onCertOpenOutputDir()
{
    QString dir = m_certOutputDir->text().trimmed();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::onCertOpenCaroot()
{
    if (m_certCarootPath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_certCarootPath));
}

void MainWindow::onCertGenerate()
{
    if (m_certRunning)
        return;

    QStringList domains;
    const QStringList lines = m_certDomainsInput->toPlainText().split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                             Qt::SkipEmptyParts);
        for (const QString &part : parts)
            domains << part.trimmed();
    }

    if (domains.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请输入至少一个域名或 IP"));
        return;
    }

    QRegularExpression validRe(QStringLiteral("^[A-Za-z0-9*_.\\-:]+$"));
    for (const QString &d : domains) {
        if (!validRe.match(d).hasMatch()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("包含非法字符的条目: %1").arg(d));
            return;
        }
    }

    QString name = m_certNameEdit->text().trimmed();
    name.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
    if (name.isEmpty())
        name = QStringLiteral("dev");

    QString outDir = m_certOutputDir->text().trimmed();
    if (outDir.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请先选择输出目录"));
        return;
    }
    if (!QDir().mkpath(outDir)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法创建输出目录: %1").arg(outDir));
        return;
    }

    QString certFile = outDir + QStringLiteral("/") + name + QStringLiteral(".pem");
    QString keyFile = outDir + QStringLiteral("/") + name + QStringLiteral("-key.pem");

    QStringList args;
    args << QStringLiteral("-cert-file") << certFile
         << QStringLiteral("-key-file") << keyFile;
    args << domains;

    if (m_certProcess) {
        m_certProcess->deleteLater();
        m_certProcess = nullptr;
    }
    m_certProcess = new QProcess(this);
    m_certProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_certProcess, &QProcess::readyReadStandardOutput,
            this, &MainWindow::onCertProcessOutput);

    connect(m_certProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, certFile, keyFile](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus);
                m_certRunning = false;
                m_certGenerateBtn->setEnabled(true);
                m_certDomainsInput->setEnabled(true);
                m_certNameEdit->setEnabled(true);
                m_certOutputBrowseBtn->setEnabled(true);

                bool ok = exitCode == 0
                    && QFileInfo::exists(certFile) && QFileInfo(certFile).size() > 0
                    && QFileInfo::exists(keyFile) && QFileInfo(keyFile).size() > 0;

                if (ok) {
                    m_certLogOutput->append(
                        QStringLiteral("证书生成成功:\n  %1\n  %2").arg(certFile, keyFile));
                    Utils::logToFile(QStringLiteral("[CERT] Generated: %1").arg(certFile));
                    QMessageBox::information(this, QStringLiteral("成功"),
                        QStringLiteral("证书生成成功：\n%1\n%2").arg(certFile, keyFile));
                } else {
                    m_certLogOutput->append(
                        QStringLiteral("生成失败，退出码: %1").arg(exitCode));
                    Utils::logToFile(QStringLiteral("[CERT] Generate failed, code=%1").arg(exitCode));
                }
            });

    connect(m_certProcess, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
                Q_UNUSED(error);
                m_certRunning = false;
                m_certGenerateBtn->setEnabled(true);
                m_certDomainsInput->setEnabled(true);
                m_certNameEdit->setEnabled(true);
                m_certOutputBrowseBtn->setEnabled(true);
                m_certLogOutput->append(
                    QStringLiteral("进程错误: ") + m_certProcess->errorString());
            });

    m_certRunning = true;
    m_certGenerateBtn->setEnabled(false);
    m_certDomainsInput->setEnabled(false);
    m_certNameEdit->setEnabled(false);
    m_certOutputBrowseBtn->setEnabled(false);

    m_certLogOutput->append(QStringLiteral("> mkcert %1").arg(args.join(' ')));
    m_certProcess->start(m_mkcertPath, args);
}

void MainWindow::onCertProcessOutput()
{
    if (!m_certProcess)
        return;
    QString output = QString::fromUtf8(m_certProcess->readAllStandardOutput());
    m_certLogOutput->append(output.trimmed());
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
