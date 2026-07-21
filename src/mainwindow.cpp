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

#include <set>
#include <algorithm>

#include <windows.h>

#include "mainwindow.h"
#include "ffmpegprocess.h"
#include "utils.h"
#include "colorpicker.h"
#include "windowpicker.h"
#include "screenshotpicker.h"
#include "pinwindow.h"

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
#include <QSettings>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QImage>
#include <QPainter>

#include "third_party/qrcodegen.hpp"
#include "third_party/quirc.h"

struct EnumWindowsContext {
    QList<HWND> *hwnds;
    HWND exclude;
};

static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    EnumWindowsContext *ctx = reinterpret_cast<EnumWindowsContext *>(lParam);
    if (hwnd == ctx->exclude)
        return TRUE;
    if (!IsWindowVisible(hwnd))
        return TRUE;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW)
        return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr)
        return TRUE;

    int len = GetWindowTextLengthW(hwnd);
    if (len == 0)
        return TRUE;

    wchar_t *buf = new wchar_t[len + 1];
    GetWindowTextW(hwnd, buf, len + 1);
    QString title = QString::fromWCharArray(buf);
    delete[] buf;
    if (title.trimmed().isEmpty())
        return TRUE;

    ctx->hwnds->append(hwnd);
    return TRUE;
}

class HotkeyDialog : public QDialog
{
public:
    UINT vk = 0;
    UINT modifiers = 0;

    explicit HotkeyDialog(QWidget *parent) : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("\u8BBE\u7F6E\u5FEB\u6377\u952E"));
        setFixedSize(340, 150);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(12);
        layout->setAlignment(Qt::AlignCenter);

        auto *label = new QLabel(QStringLiteral("\u8BF7\u6309\u4E0B\u65B0\u7684\u5FEB\u6377\u952E..."), this);
        QFont f = label->font();
        f.setPointSize(13);
        label->setFont(f);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);

        auto *hint = new QLabel(QStringLiteral("ESC \u53D6\u6D88  |  \u652F\u6301 F1-F12 \u53CA Ctrl/Alt/Shift \u7EC4\u5408\u952E"), this);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 11px;"));
        layout->addWidget(hint);
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QDialog::showEvent(event);
        grabKeyboard();
    }

    void hideEvent(QHideEvent *event) override
    {
        releaseKeyboard();
        QDialog::hideEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            reject();
            return;
        }

        vk = 0;
        modifiers = 0;

        if (event->modifiers() & Qt::ControlModifier) modifiers |= MOD_CONTROL;
        if (event->modifiers() & Qt::AltModifier)     modifiers |= MOD_ALT;
        if (event->modifiers() & Qt::ShiftModifier)   modifiers |= MOD_SHIFT;
        if (event->modifiers() & Qt::MetaModifier)     modifiers |= MOD_WIN;

        int qtKey = event->key();
        switch (qtKey) {
            case Qt::Key_F1:  vk = VK_F1;  break;
            case Qt::Key_F2:  vk = VK_F2;  break;
            case Qt::Key_F3:  vk = VK_F3;  break;
            case Qt::Key_F4:  vk = VK_F4;  break;
            case Qt::Key_F5:  vk = VK_F5;  break;
            case Qt::Key_F6:  vk = VK_F6;  break;
            case Qt::Key_F7:  vk = VK_F7;  break;
            case Qt::Key_F8:  vk = VK_F8;  break;
            case Qt::Key_F9:  vk = VK_F9;  break;
            case Qt::Key_F10: vk = VK_F10; break;
            case Qt::Key_F11: vk = VK_F11; break;
            case Qt::Key_F12: vk = VK_F12; break;
            default: {
                if (event->nativeScanCode() != 0) {
                    vk = MapVirtualKeyW(event->nativeScanCode(), MAPVK_VSC_TO_VK);
                } else if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
                    vk = 'A' + (qtKey - Qt::Key_A);
                } else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
                    vk = '0' + (qtKey - Qt::Key_0);
                } else {
                    vk = qtKey;
                }
                break;
            }
        }

        if (vk != 0) {
            accept();
        }
    }
};

MainWindow::MainWindow(const QString &ffmpegPath, const QString &aria2Path, QWidget *parent)
    : QMainWindow(parent)
    , m_ffmpegPath(ffmpegPath)
    , m_aria2Path(aria2Path)
    , m_imageCurrentIndex(0)
    , m_videoCurrentIndex(0)
    , m_audioCurrentIndex(0)
    , m_imageCancelling(false)
    , m_videoCancelling(false)
    , m_audioCancelling(false)
    , m_imageSizeBefore(0)
    , m_imageSizeAfter(0)
    , m_imageSuccessCount(0)
    , m_imageFailedCount(0)
    , m_videoSizeBefore(0)
    , m_videoSizeAfter(0)
    , m_videoSuccessCount(0)
    , m_videoFailedCount(0)
    , m_audioSizeBefore(0)
    , m_audioSizeAfter(0)
    , m_audioSuccessCount(0)
    , m_audioFailedCount(0)
    , m_ffmpegImage(new FFmpegProcess(this))
    , m_ffmpegVideo(new FFmpegProcess(this))
    , m_ffmpegAudio(new FFmpegProcess(this))
    , m_downloadProcess(nullptr)
    , m_downloadCompleted(0)
    , m_downloadFailed(0)
    , m_downloadCancelling(false)
    , m_pickedColor(Qt::white)
    , m_colorPicker(new ColorPicker(nullptr))
    , m_windowPicker(new WindowPicker(nullptr))
    , m_stopwatch(new StopwatchTimer(this))
    , m_screenshotPicker(new ScreenshotPicker(nullptr))
    , m_screenshotHotkeyLabel(nullptr)
    , m_transparencyTargetHwnd(nullptr)
    , m_transparencyOriginalExStyle(0)
{
    setWindowTitle(QStringLiteral("Zane Tool"));
    resize(820, 560);
    setAcceptDrops(true);
    setupUi();

    connect(m_ffmpegImage, &FFmpegProcess::finished, this, [this](bool success, int) {
        if (m_imageCancelling) return;
        QFileInfo fi(m_imageTaskQueue[m_imageCurrentIndex].inputPath);
        qint64 inputSize = fi.size();
        QFileInfo fo(ImageProcessor::buildOutputPath(m_imageTaskQueue[m_imageCurrentIndex]));
        qint64 outputSize = fo.exists() ? fo.size() : 0;

        if (success) {
            m_imageSuccessCount++;
            m_imageSizeBefore += inputSize;
            m_imageSizeAfter += outputSize;
            Utils::logToFile(QStringLiteral("[IMAGE] OK: %1 -> %2 (%3 -> %4)")
                .arg(fi.fileName(), fo.fileName())
                .arg(Utils::formatFileSize(inputSize), Utils::formatFileSize(outputSize)));
        } else {
            m_imageFailedCount++;
            m_imageFailedFiles.append(fi.fileName());
            Utils::logToFile(QStringLiteral("[IMAGE] FAIL: %1").arg(fi.fileName()));
        }
        m_imageCurrentIndex++;
        processNextImage();
    });

    connect(m_ffmpegImage, &FFmpegProcess::errorOccurred, this, [this](const QString &msg) {
        m_imageStatusLabel->setText(QStringLiteral("错误: ") + msg);
    });

    connect(m_ffmpegVideo, &FFmpegProcess::finished, this, [this](bool success, int) {
        if (m_videoCancelling) return;
        QFileInfo fi(m_videoTaskQueue[m_videoCurrentIndex].inputPath);
        qint64 inputSize = fi.size();
        QFileInfo fo(VideoProcessor::buildOutputPath(m_videoTaskQueue[m_videoCurrentIndex]));
        qint64 outputSize = fo.exists() ? fo.size() : 0;

        if (success) {
            m_videoSuccessCount++;
            m_videoSizeBefore += inputSize;
            m_videoSizeAfter += outputSize;
            Utils::logToFile(QStringLiteral("[VIDEO] OK: %1 -> %2 (%3 -> %4)")
                .arg(fi.fileName(), fo.fileName())
                .arg(Utils::formatFileSize(inputSize), Utils::formatFileSize(outputSize)));
        } else {
            m_videoFailedCount++;
            m_videoFailedFiles.append(fi.fileName());
            Utils::logToFile(QStringLiteral("[VIDEO] FAIL: %1").arg(fi.fileName()));
        }
        m_videoCurrentIndex++;
        processNextVideo();
    });

    connect(m_ffmpegVideo, &FFmpegProcess::errorOccurred, this, [this](const QString &msg) {
        m_videoStatusLabel->setText(QStringLiteral("错误: ") + msg);
    });

    connect(m_ffmpegAudio, &FFmpegProcess::finished, this, [this](bool success, int) {
        if (m_audioCancelling) return;
        QFileInfo fi(m_audioTaskQueue[m_audioCurrentIndex].inputPath);
        qint64 inputSize = fi.size();
        QFileInfo fo(AudioProcessor::buildOutputPath(m_audioTaskQueue[m_audioCurrentIndex]));
        qint64 outputSize = fo.exists() ? fo.size() : 0;

        if (success) {
            m_audioSuccessCount++;
            m_audioSizeBefore += inputSize;
            m_audioSizeAfter += outputSize;
            Utils::logToFile(QStringLiteral("[AUDIO] OK: %1 -> %2 (%3 -> %4)")
                .arg(fi.fileName(), fo.fileName())
                .arg(Utils::formatFileSize(inputSize), Utils::formatFileSize(outputSize)));
        } else {
            m_audioFailedCount++;
            m_audioFailedFiles.append(fi.fileName());
            Utils::logToFile(QStringLiteral("[AUDIO] FAIL: %1").arg(fi.fileName()));
        }
        m_audioCurrentIndex++;
        processNextAudio();
    });

    connect(m_ffmpegAudio, &FFmpegProcess::errorOccurred, this, [this](const QString &msg) {
        m_audioStatusLabel->setText(QStringLiteral("错误: ") + msg);
    });

    connect(m_colorPicker, &ColorPicker::colorPicked, this, &MainWindow::onColorPicked);
    connect(m_colorPicker, &ColorPicker::cancelled, this, &MainWindow::onPickCancelled);

    connect(m_windowPicker, &WindowPicker::windowPicked, this, &MainWindow::onWindowPicked);
    connect(m_windowPicker, &WindowPicker::cancelled, this, &MainWindow::onPickCancelled);

    connect(m_stopwatch, &StopwatchTimer::timeUpdated, this, &MainWindow::onTimerTick);
    connect(m_stopwatch, &StopwatchTimer::stateChanged, this, &MainWindow::onTimerStateChanged);
    connect(m_stopwatch, &StopwatchTimer::lapRecorded, this, &MainWindow::onTimerLapRecorded);

    connect(m_screenshotPicker, &ScreenshotPicker::screenshotCaptured,
            this, &MainWindow::onScreenshotCaptured);
    connect(m_screenshotPicker, &ScreenshotPicker::cancelled,
            this, &MainWindow::onPickCancelled);

    QString configPath = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings settings(configPath, QSettings::IniFormat);
    m_hotkeyVk = settings.value(QStringLiteral("screenshot/hotkeyVk"), VK_F4).toUInt();
    m_hotkeyModifiers = settings.value(QStringLiteral("screenshot/hotkeyModifiers"), 0).toUInt();
    registerGlobalHotkey();

    if (m_screenshotHotkeyLabel) {
        m_screenshotHotkeyLabel->setText(
            QStringLiteral("\u5F53\u524D\u5FEB\u6377\u952E\uFF1A%1").arg(hotkeyDisplayText()));
    }
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

    m_stackedWidget->addWidget(createImageTab());
    m_stackedWidget->addWidget(createVideoTab());
    m_stackedWidget->addWidget(createAudioTab());
    m_stackedWidget->addWidget(createColorPickerPage());
    m_stackedWidget->addWidget(createStickyNotePage());
    m_stackedWidget->addWidget(createTransparencyPage());
    m_stackedWidget->addWidget(createTimerPage());
    m_stackedWidget->addWidget(createBase64Page());
    m_stackedWidget->addWidget(createTimestampPage());
    m_stackedWidget->addWidget(createCronPage());
    m_stackedWidget->addWidget(createJwtPage());
    m_stackedWidget->addWidget(createDownloadPage());
    m_stackedWidget->addWidget(createRandomStringPage());
    m_stackedWidget->addWidget(createQrCodePage());

    m_stackedWidget->setCurrentIndex(0);

    m_aboutLabel = new QLabel(QStringLiteral("<a href='about' style='color:#6c757d;text-decoration:none;'>v0.1.0</a>"), this);
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

    addCategory(QStringLiteral("\U0001F527 开发工具"));
    addTool(QStringLiteral("图片转Base64"), 7);
    addTool(QStringLiteral("时间戳转换"), 8);
    addTool(QStringLiteral("Cron 解析"), 9);
    addTool(QStringLiteral("JWT 解析"), 10);
    addTool(QStringLiteral("随机字符串"), 12);
    addTool(QStringLiteral("二维码工具"), 13);

    addCategory(QStringLiteral("\U0001F310 网络工具"));
    addTool(QStringLiteral("文件批量下载"), 11);

    connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        QListWidgetItem *item = m_sidebar->item(row);
        int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0)
            m_stackedWidget->setCurrentIndex(idx);
    });

    m_sidebar->setCurrentRow(1);
}

// ==================== Image Tab ====================

QWidget *MainWindow::createImageTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(tab);
    mainLayout->setSpacing(10);

    // --- File list section ---
    m_imageFileList = new QListWidget(tab);
    m_imageFileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_imageFileList->setMinimumHeight(60);
    m_imageFileList->setAcceptDrops(true);
    m_imageFileList->setDragDropMode(QAbstractItemView::DropOnly);

    connect(m_imageFileList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onImageSelectionChanged);

    QHBoxLayout *fileBtnLayout = new QHBoxLayout();
    m_imageAddBtn = new QPushButton(QStringLiteral("添加文件"), tab);
    m_imageRemoveBtn = new QPushButton(QStringLiteral("移除选中"), tab);
    m_imageRemoveBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_imageClearBtn = new QPushButton(QStringLiteral("清空"), tab);
    m_imageClearBtn->setObjectName(QStringLiteral("dangerBtn"));
    fileBtnLayout->addWidget(m_imageAddBtn);
    fileBtnLayout->addWidget(m_imageRemoveBtn);
    fileBtnLayout->addWidget(m_imageClearBtn);
    fileBtnLayout->addStretch();

    connect(m_imageAddBtn, &QPushButton::clicked, this, &MainWindow::onImageAddFiles);
    connect(m_imageRemoveBtn, &QPushButton::clicked, this, &MainWindow::onImageRemoveSelected);
    connect(m_imageClearBtn, &QPushButton::clicked, this, &MainWindow::onImageClearFiles);

    // --- Settings row ---
    QHBoxLayout *settingsLayout = new QHBoxLayout();

    // Compression settings
    QGroupBox *compGroup = new QGroupBox(QStringLiteral("压缩设置"), tab);
    QVBoxLayout *compLayout = new QVBoxLayout(compGroup);
    QHBoxLayout *qualityLayout = new QHBoxLayout();
    QLabel *qualityLabel = new QLabel(QStringLiteral("质量:"), tab);
    m_imageQualitySlider = new QSlider(Qt::Horizontal, tab);
    m_imageQualitySlider->setRange(0, 100);
    m_imageQualitySlider->setValue(75);
    m_imageQualityLabel = new QLabel(QStringLiteral("75"), tab);
    m_imageQualityLabel->setFixedWidth(30);
    qualityLayout->addWidget(qualityLabel);
    qualityLayout->addWidget(m_imageQualitySlider);
    qualityLayout->addWidget(m_imageQualityLabel);
    compLayout->addLayout(qualityLayout);

    connect(m_imageQualitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_imageQualityLabel->setText(QString::number(v));
    });

    // Scale settings
    QGroupBox *scaleGroup = new QGroupBox(QStringLiteral("缩放设置"), tab);
    QVBoxLayout *scaleLayout = new QVBoxLayout(scaleGroup);
    m_imageScaleCheck = new QCheckBox(QStringLiteral("启用缩放"), tab);
    QHBoxLayout *scaleWidthLayout = new QHBoxLayout();
    QLabel *widthLabel = new QLabel(QStringLiteral("目标宽度:"), tab);
    m_imageScaleWidth = new QSpinBox(tab);
    m_imageScaleWidth->setRange(1, 10000);
    m_imageScaleWidth->setValue(1920);
    m_imageScaleWidth->setSuffix(QStringLiteral(" px"));
    m_imageScaleWidth->setEnabled(false);
    scaleWidthLayout->addWidget(widthLabel);
    scaleWidthLayout->addWidget(m_imageScaleWidth);
    scaleLayout->addWidget(m_imageScaleCheck);
    scaleLayout->addLayout(scaleWidthLayout);

    connect(m_imageScaleCheck, &QCheckBox::toggled, m_imageScaleWidth, &QSpinBox::setEnabled);

    settingsLayout->addWidget(compGroup);
    settingsLayout->addWidget(scaleGroup);
    settingsLayout->setStretch(0, 1);
    settingsLayout->setStretch(1, 1);

    // --- Format + Output row ---
    QHBoxLayout *outputRow = new QHBoxLayout();

    QGroupBox *formatGroup = new QGroupBox(QStringLiteral("输出格式"), tab);
    QHBoxLayout *formatLayout = new QHBoxLayout(formatGroup);
    m_imageFormatCombo = new QComboBox(tab);
    m_imageFormatCombo->addItem(QStringLiteral("保持原格式"), QString());
    m_imageFormatCombo->addItem(QStringLiteral("JPG"), QStringLiteral("jpg"));
    m_imageFormatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    m_imageFormatCombo->addItem(QStringLiteral("WebP"), QStringLiteral("webp"));
    m_imageFormatCombo->addItem(QStringLiteral("BMP"), QStringLiteral("bmp"));
    formatLayout->addWidget(m_imageFormatCombo);

    QGroupBox *outDirGroup = new QGroupBox(QStringLiteral("输出目录"), tab);
    QHBoxLayout *outDirLayout = new QHBoxLayout(outDirGroup);
    m_imageOutputDir = new QLineEdit(tab);
    m_imageOutputBrowse = new QPushButton(QStringLiteral("浏览"), tab);
    outDirLayout->addWidget(m_imageOutputDir);
    outDirLayout->addWidget(m_imageOutputBrowse);

    connect(m_imageOutputBrowse, &QPushButton::clicked, this, &MainWindow::onImageOutputBrowse);

    outputRow->addWidget(formatGroup);
    outputRow->addWidget(outDirGroup);

    // --- Resolution preview ---
    m_imageResPreview = new QLabel(tab);
    m_imageResPreview->setVisible(false);

    // --- Progress / Status ---
    m_imageProgressBar = new QProgressBar(tab);
    m_imageProgressBar->setRange(0, 100);
    m_imageProgressBar->setValue(0);
    m_imageProgressBar->setTextVisible(true);
    m_imageProgressBar->setFixedHeight(12);

    m_imageStatusLabel = new QLabel(QStringLiteral("就绪"), tab);

    // --- Start/Cancel ---
    QHBoxLayout *actionLayout = new QHBoxLayout();
    m_imageStartBtn = new QPushButton(QStringLiteral("开始处理"), tab);
    m_imageStartBtn->setFixedHeight(42);
    m_imageStartBtn->setFixedWidth(160);
    m_imageStartBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_imageCancelBtn = new QPushButton(QStringLiteral("取消"), tab);
    m_imageCancelBtn->setEnabled(false);
    m_imageCancelBtn->setFixedHeight(42);
    m_imageCancelBtn->setFixedWidth(160);
    m_imageCancelBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_imageCancelBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    actionLayout->addStretch();
    actionLayout->addWidget(m_imageStartBtn);
    actionLayout->addSpacing(16);
    actionLayout->addWidget(m_imageCancelBtn);
    actionLayout->addStretch();

    connect(m_imageStartBtn, &QPushButton::clicked, this, &MainWindow::onImageStart);
    connect(m_imageCancelBtn, &QPushButton::clicked, this, &MainWindow::onImageCancel);

    // --- Assemble ---
    mainLayout->addWidget(m_imageFileList);
    mainLayout->addLayout(fileBtnLayout);
    mainLayout->addLayout(settingsLayout);
    mainLayout->addLayout(outputRow);
    mainLayout->addWidget(m_imageResPreview);
    mainLayout->addWidget(m_imageProgressBar);
    mainLayout->addWidget(m_imageStatusLabel);
    mainLayout->addLayout(actionLayout);

    return tab;
}

// ==================== Video Tab ====================

QWidget *MainWindow::createVideoTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(tab);
    mainLayout->setSpacing(10);

    // --- File list section ---
    m_videoFileList = new QListWidget(tab);
    m_videoFileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_videoFileList->setMinimumHeight(60);
    m_videoFileList->setAcceptDrops(true);
    m_videoFileList->setDragDropMode(QAbstractItemView::DropOnly);

    connect(m_videoFileList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onVideoSelectionChanged);

    QHBoxLayout *fileBtnLayout = new QHBoxLayout();
    m_videoAddBtn = new QPushButton(QStringLiteral("添加文件"), tab);
    m_videoRemoveBtn = new QPushButton(QStringLiteral("移除选中"), tab);
    m_videoRemoveBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_videoClearBtn = new QPushButton(QStringLiteral("清空"), tab);
    m_videoClearBtn->setObjectName(QStringLiteral("dangerBtn"));
    fileBtnLayout->addWidget(m_videoAddBtn);
    fileBtnLayout->addWidget(m_videoRemoveBtn);
    fileBtnLayout->addWidget(m_videoClearBtn);
    fileBtnLayout->addStretch();

    connect(m_videoAddBtn, &QPushButton::clicked, this, &MainWindow::onVideoAddFiles);
    connect(m_videoRemoveBtn, &QPushButton::clicked, this, &MainWindow::onVideoRemoveSelected);
    connect(m_videoClearBtn, &QPushButton::clicked, this, &MainWindow::onVideoClearFiles);

    // --- Settings row ---
    QHBoxLayout *settingsLayout = new QHBoxLayout();

    // Encoding settings
    QGroupBox *encGroup = new QGroupBox(QStringLiteral("编码设置"), tab);
    QVBoxLayout *encLayout = new QVBoxLayout(encGroup);

    QHBoxLayout *fmtLayout = new QHBoxLayout();
    QLabel *fmtLabel = new QLabel(QStringLiteral("格式:"), tab);
    m_videoFormatCombo = new QComboBox(tab);
    m_videoFormatCombo->addItem(QStringLiteral("保持原格式"), QString());
    m_videoFormatCombo->addItem(QStringLiteral("MP4 (H.264)"), QStringLiteral("mp4"));
    m_videoFormatCombo->addItem(QStringLiteral("MP4 (H.265)"), QStringLiteral("mp4_hevc"));
    m_videoFormatCombo->addItem(QStringLiteral("WebM (VP9)"), QStringLiteral("webm"));
    m_videoFormatCombo->addItem(QStringLiteral("AVI"), QStringLiteral("avi"));
    m_videoFormatCombo->addItem(QStringLiteral("MOV"), QStringLiteral("mov"));
    fmtLayout->addWidget(fmtLabel);
    fmtLayout->addWidget(m_videoFormatCombo);

    QHBoxLayout *crfLayout = new QHBoxLayout();
    QLabel *crfLabel = new QLabel(QStringLiteral("CRF:"), tab);
    m_videoCrfSlider = new QSlider(Qt::Horizontal, tab);
    m_videoCrfSlider->setRange(0, 51);
    m_videoCrfSlider->setValue(23);
    m_videoCrfLabel = new QLabel(QStringLiteral("23"), tab);
    m_videoCrfLabel->setFixedWidth(30);
    crfLayout->addWidget(crfLabel);
    crfLayout->addWidget(m_videoCrfSlider);
    crfLayout->addWidget(m_videoCrfLabel);

    encLayout->addLayout(fmtLayout);
    encLayout->addLayout(crfLayout);

    connect(m_videoCrfSlider, &QSlider::valueChanged, this, [this](int v) {
        m_videoCrfLabel->setText(QString::number(v));
    });

    // Scale settings
    QGroupBox *scaleGroup = new QGroupBox(QStringLiteral("缩放设置"), tab);
    QVBoxLayout *scaleLayout = new QVBoxLayout(scaleGroup);
    m_videoScaleCheck = new QCheckBox(QStringLiteral("启用缩放"), tab);
    QHBoxLayout *presetLayout = new QHBoxLayout();
    QLabel *presetLabel = new QLabel(QStringLiteral("预设分辨率:"), tab);
    m_videoPresetRes = new QComboBox(tab);
    m_videoPresetRes->addItem(QStringLiteral("原始"), QStringLiteral("original"));
    m_videoPresetRes->addItem(QStringLiteral("1080p"), QStringLiteral("1080"));
    m_videoPresetRes->addItem(QStringLiteral("720p"), QStringLiteral("720"));
    m_videoPresetRes->addItem(QStringLiteral("480p"), QStringLiteral("480"));
    m_videoPresetRes->setEnabled(false);
    presetLayout->addWidget(presetLabel);
    presetLayout->addWidget(m_videoPresetRes);
    scaleLayout->addWidget(m_videoScaleCheck);
    scaleLayout->addLayout(presetLayout);

    connect(m_videoScaleCheck, &QCheckBox::toggled, m_videoPresetRes, &QComboBox::setEnabled);

    settingsLayout->addWidget(encGroup);
    settingsLayout->addWidget(scaleGroup);
    settingsLayout->setStretch(0, 1);
    settingsLayout->setStretch(1, 1);

    // --- Output directory ---
    QGroupBox *outDirGroup = new QGroupBox(QStringLiteral("输出目录"), tab);
    QHBoxLayout *outDirLayout = new QHBoxLayout(outDirGroup);
    m_videoOutputDir = new QLineEdit(tab);
    m_videoOutputBrowse = new QPushButton(QStringLiteral("浏览"), tab);
    outDirLayout->addWidget(m_videoOutputDir);
    outDirLayout->addWidget(m_videoOutputBrowse);

    connect(m_videoOutputBrowse, &QPushButton::clicked, this, &MainWindow::onVideoOutputBrowse);

    // --- Info preview ---
    m_videoInfoPreview = new QLabel(tab);
    m_videoInfoPreview->setVisible(false);

    // --- Progress / Status ---
    m_videoProgressBar = new QProgressBar(tab);
    m_videoProgressBar->setRange(0, 100);
    m_videoProgressBar->setValue(0);
    m_videoProgressBar->setTextVisible(true);
    m_videoProgressBar->setFixedHeight(12);

    m_videoStatusLabel = new QLabel(QStringLiteral("就绪"), tab);

    // --- Start/Cancel ---
    QHBoxLayout *actionLayout = new QHBoxLayout();
    m_videoStartBtn = new QPushButton(QStringLiteral("开始处理"), tab);
    m_videoStartBtn->setFixedHeight(42);
    m_videoStartBtn->setFixedWidth(160);
    m_videoStartBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_videoCancelBtn = new QPushButton(QStringLiteral("取消"), tab);
    m_videoCancelBtn->setEnabled(false);
    m_videoCancelBtn->setFixedHeight(42);
    m_videoCancelBtn->setFixedWidth(160);
    m_videoCancelBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_videoCancelBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    actionLayout->addStretch();
    actionLayout->addWidget(m_videoStartBtn);
    actionLayout->addSpacing(16);
    actionLayout->addWidget(m_videoCancelBtn);
    actionLayout->addStretch();

    connect(m_videoStartBtn, &QPushButton::clicked, this, &MainWindow::onVideoStart);
    connect(m_videoCancelBtn, &QPushButton::clicked, this, &MainWindow::onVideoCancel);

    // --- Assemble ---
    mainLayout->addWidget(m_videoFileList);
    mainLayout->addLayout(fileBtnLayout);
    mainLayout->addLayout(settingsLayout);
    mainLayout->addWidget(outDirGroup);
    mainLayout->addWidget(m_videoInfoPreview);
    mainLayout->addWidget(m_videoProgressBar);
    mainLayout->addWidget(m_videoStatusLabel);
    mainLayout->addLayout(actionLayout);

    return tab;
}

// ==================== Audio Tab ====================

QWidget *MainWindow::createAudioTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(tab);
    mainLayout->setSpacing(10);

    // --- File list section ---
    m_audioFileList = new QListWidget(tab);
    m_audioFileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_audioFileList->setMinimumHeight(60);
    m_audioFileList->setAcceptDrops(true);
    m_audioFileList->setDragDropMode(QAbstractItemView::DropOnly);

    connect(m_audioFileList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onAudioSelectionChanged);

    QHBoxLayout *fileBtnLayout = new QHBoxLayout();
    m_audioAddBtn = new QPushButton(QStringLiteral("添加文件"), tab);
    m_audioRemoveBtn = new QPushButton(QStringLiteral("移除选中"), tab);
    m_audioRemoveBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_audioClearBtn = new QPushButton(QStringLiteral("清空"), tab);
    m_audioClearBtn->setObjectName(QStringLiteral("dangerBtn"));
    fileBtnLayout->addWidget(m_audioAddBtn);
    fileBtnLayout->addWidget(m_audioRemoveBtn);
    fileBtnLayout->addWidget(m_audioClearBtn);
    fileBtnLayout->addStretch();

    connect(m_audioAddBtn, &QPushButton::clicked, this, &MainWindow::onAudioAddFiles);
    connect(m_audioRemoveBtn, &QPushButton::clicked, this, &MainWindow::onAudioRemoveSelected);
    connect(m_audioClearBtn, &QPushButton::clicked, this, &MainWindow::onAudioClearFiles);

    // --- Settings row ---
    QHBoxLayout *settingsLayout = new QHBoxLayout();

    // Format group
    QGroupBox *formatGroup = new QGroupBox(QStringLiteral("输出格式"), tab);
    QHBoxLayout *formatLayout = new QHBoxLayout(formatGroup);
    m_audioFormatCombo = new QComboBox(tab);
    m_audioFormatCombo->addItem(QStringLiteral("保持原格式"), QString());
    m_audioFormatCombo->addItem(QStringLiteral("MP3"), QStringLiteral("mp3"));
    m_audioFormatCombo->addItem(QStringLiteral("AAC (M4A)"), QStringLiteral("m4a"));
    m_audioFormatCombo->addItem(QStringLiteral("FLAC"), QStringLiteral("flac"));
    m_audioFormatCombo->addItem(QStringLiteral("WAV"), QStringLiteral("wav"));
    m_audioFormatCombo->addItem(QStringLiteral("OGG (Vorbis)"), QStringLiteral("ogg"));
    m_audioFormatCombo->addItem(QStringLiteral("Opus"), QStringLiteral("opus"));
    formatLayout->addWidget(m_audioFormatCombo);

    // Bitrate group
    QGroupBox *bitrateGroup = new QGroupBox(QStringLiteral("码率"), tab);
    QHBoxLayout *bitrateLayout = new QHBoxLayout(bitrateGroup);
    m_audioBitrateCombo = new QComboBox(tab);
    m_audioBitrateCombo->addItem(QStringLiteral("64 kbps"), QStringLiteral("64k"));
    m_audioBitrateCombo->addItem(QStringLiteral("96 kbps"), QStringLiteral("96k"));
    m_audioBitrateCombo->addItem(QStringLiteral("128 kbps"), QStringLiteral("128k"));
    m_audioBitrateCombo->addItem(QStringLiteral("192 kbps"), QStringLiteral("192k"));
    m_audioBitrateCombo->addItem(QStringLiteral("256 kbps"), QStringLiteral("256k"));
    m_audioBitrateCombo->addItem(QStringLiteral("320 kbps"), QStringLiteral("320k"));
    m_audioBitrateCombo->setCurrentIndex(3);
    bitrateLayout->addWidget(m_audioBitrateCombo);

    settingsLayout->addWidget(formatGroup);
    settingsLayout->addWidget(bitrateGroup);
    settingsLayout->setStretch(0, 1);
    settingsLayout->setStretch(1, 1);

    // --- Advanced row ---
    QHBoxLayout *advancedLayout = new QHBoxLayout();

    // Sample rate group
    QGroupBox *sampleRateGroup = new QGroupBox(QStringLiteral("采样率"), tab);
    QHBoxLayout *sampleRateLayout = new QHBoxLayout(sampleRateGroup);
    m_audioSampleRateCombo = new QComboBox(tab);
    m_audioSampleRateCombo->addItem(QStringLiteral("保持原样"), QString());
    m_audioSampleRateCombo->addItem(QStringLiteral("22050 Hz"), QStringLiteral("22050"));
    m_audioSampleRateCombo->addItem(QStringLiteral("44100 Hz"), QStringLiteral("44100"));
    m_audioSampleRateCombo->addItem(QStringLiteral("48000 Hz"), QStringLiteral("48000"));
    sampleRateLayout->addWidget(m_audioSampleRateCombo);

    // Channels group
    QGroupBox *channelsGroup = new QGroupBox(QStringLiteral("声道"), tab);
    QHBoxLayout *channelsLayout = new QHBoxLayout(channelsGroup);
    m_audioChannelsCombo = new QComboBox(tab);
    m_audioChannelsCombo->addItem(QStringLiteral("保持原样"), QString());
    m_audioChannelsCombo->addItem(QStringLiteral("单声道"), QStringLiteral("1"));
    m_audioChannelsCombo->addItem(QStringLiteral("立体声"), QStringLiteral("2"));
    channelsLayout->addWidget(m_audioChannelsCombo);

    advancedLayout->addWidget(sampleRateGroup);
    advancedLayout->addWidget(channelsGroup);
    advancedLayout->setStretch(0, 1);
    advancedLayout->setStretch(1, 1);

    // --- Output directory ---
    QGroupBox *outDirGroup = new QGroupBox(QStringLiteral("输出目录"), tab);
    QHBoxLayout *outDirLayout = new QHBoxLayout(outDirGroup);
    m_audioOutputDir = new QLineEdit(tab);
    m_audioOutputBrowse = new QPushButton(QStringLiteral("浏览"), tab);
    outDirLayout->addWidget(m_audioOutputDir);
    outDirLayout->addWidget(m_audioOutputBrowse);

    connect(m_audioOutputBrowse, &QPushButton::clicked, this, &MainWindow::onAudioOutputBrowse);

    // --- Info preview ---
    m_audioInfoPreview = new QLabel(tab);
    m_audioInfoPreview->setVisible(false);

    // --- Progress / Status ---
    m_audioProgressBar = new QProgressBar(tab);
    m_audioProgressBar->setRange(0, 100);
    m_audioProgressBar->setValue(0);
    m_audioProgressBar->setTextVisible(true);
    m_audioProgressBar->setFixedHeight(12);

    m_audioStatusLabel = new QLabel(QStringLiteral("就绪"), tab);

    // --- Start/Cancel ---
    QHBoxLayout *actionLayout = new QHBoxLayout();
    m_audioStartBtn = new QPushButton(QStringLiteral("开始处理"), tab);
    m_audioStartBtn->setFixedHeight(42);
    m_audioStartBtn->setFixedWidth(160);
    m_audioStartBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_audioCancelBtn = new QPushButton(QStringLiteral("取消"), tab);
    m_audioCancelBtn->setEnabled(false);
    m_audioCancelBtn->setFixedHeight(42);
    m_audioCancelBtn->setFixedWidth(160);
    m_audioCancelBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_audioCancelBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    actionLayout->addStretch();
    actionLayout->addWidget(m_audioStartBtn);
    actionLayout->addSpacing(16);
    actionLayout->addWidget(m_audioCancelBtn);
    actionLayout->addStretch();

    connect(m_audioStartBtn, &QPushButton::clicked, this, &MainWindow::onAudioStart);
    connect(m_audioCancelBtn, &QPushButton::clicked, this, &MainWindow::onAudioCancel);

    // --- Assemble ---
    mainLayout->addWidget(m_audioFileList);
    mainLayout->addLayout(fileBtnLayout);
    mainLayout->addLayout(settingsLayout);
    mainLayout->addLayout(advancedLayout);
    mainLayout->addWidget(outDirGroup);
    mainLayout->addWidget(m_audioInfoPreview);
    mainLayout->addWidget(m_audioProgressBar);
    mainLayout->addWidget(m_audioStatusLabel);
    mainLayout->addLayout(actionLayout);

    return tab;
}

// ==================== Image Slots ====================

void MainWindow::onImageAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择图片文件"), QString(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp);;所有文件 (*.*)"));
    for (const QString &f : files) {
        m_imageFileList->addItem(f);
    }
}

void MainWindow::onImageRemoveSelected()
{
    QList<QListWidgetItem *> items = m_imageFileList->selectedItems();
    for (QListWidgetItem *item : items) {
        delete m_imageFileList->takeItem(m_imageFileList->row(item));
    }
}

void MainWindow::onImageClearFiles()
{
    m_imageFileList->clear();
}

void MainWindow::onImageOutputBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
    if (!dir.isEmpty())
        m_imageOutputDir->setText(dir);
}

void MainWindow::onImageStart()
{
    if (m_imageFileList->count() == 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加文件。"));
        return;
    }
    if (m_imageOutputDir->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择输出目录。"));
        return;
    }

    m_imageTaskQueue.clear();
    m_imageCurrentIndex = 0;
    m_imageCancelling = false;
    m_imageSuccessCount = 0;
    m_imageFailedCount = 0;
    m_imageSizeBefore = 0;
    m_imageSizeAfter = 0;
    m_imageFailedFiles.clear();

    for (int i = 0; i < m_imageFileList->count(); ++i) {
        ImageTask task;
        task.inputPath = m_imageFileList->item(i)->text();
        task.outputDir = m_imageOutputDir->text();
        task.format = m_imageFormatCombo->currentData().toString();
        task.quality = m_imageQualitySlider->value();
        task.enableScale = m_imageScaleCheck->isChecked();
        task.scaleWidth = m_imageScaleWidth->value();
        m_imageTaskQueue.append(task);
    }

    setImageUiEnabled(false);
    processNextImage();
}

void MainWindow::onImageCancel()
{
    m_imageCancelling = true;
    m_ffmpegImage->cancel();
    m_imageStatusLabel->setText(QStringLiteral("已取消"));
    setImageUiEnabled(true);
    m_imageProgressBar->setValue(m_imageTaskQueue.isEmpty() ? 0 : m_imageCurrentIndex * 100 / m_imageTaskQueue.size());
}

void MainWindow::onImageSelectionChanged()
{
    updateImageResolutionPreview();
}

// ==================== Video Slots ====================

void MainWindow::onVideoAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择视频文件"), QString(),
        QStringLiteral("视频文件 (*.mp4 *.webm *.avi *.mov *.mkv);;所有文件 (*.*)"));
    for (const QString &f : files) {
        m_videoFileList->addItem(f);
    }
}

void MainWindow::onVideoRemoveSelected()
{
    QList<QListWidgetItem *> items = m_videoFileList->selectedItems();
    for (QListWidgetItem *item : items) {
        delete m_videoFileList->takeItem(m_videoFileList->row(item));
    }
}

void MainWindow::onVideoClearFiles()
{
    m_videoFileList->clear();
}

void MainWindow::onVideoOutputBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
    if (!dir.isEmpty())
        m_videoOutputDir->setText(dir);
}

void MainWindow::onVideoStart()
{
    if (m_videoFileList->count() == 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加文件。"));
        return;
    }
    if (m_videoOutputDir->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择输出目录。"));
        return;
    }

    m_videoTaskQueue.clear();
    m_videoCurrentIndex = 0;
    m_videoCancelling = false;
    m_videoSuccessCount = 0;
    m_videoFailedCount = 0;
    m_videoSizeBefore = 0;
    m_videoSizeAfter = 0;
    m_videoFailedFiles.clear();

    for (int i = 0; i < m_videoFileList->count(); ++i) {
        VideoTask task;
        task.inputPath = m_videoFileList->item(i)->text();
        task.outputDir = m_videoOutputDir->text();
        task.format = m_videoFormatCombo->currentData().toString();
        task.crf = m_videoCrfSlider->value();
        task.enableScale = m_videoScaleCheck->isChecked();
        task.presetRes = m_videoPresetRes->currentData().toString();
        m_videoTaskQueue.append(task);
    }

    setVideoUiEnabled(false);
    processNextVideo();
}

void MainWindow::onVideoCancel()
{
    m_videoCancelling = true;
    m_ffmpegVideo->cancel();
    m_videoStatusLabel->setText(QStringLiteral("已取消"));
    setVideoUiEnabled(true);
    m_videoProgressBar->setValue(m_videoTaskQueue.isEmpty() ? 0 : m_videoCurrentIndex * 100 / m_videoTaskQueue.size());
}

void MainWindow::onVideoSelectionChanged()
{
    updateVideoInfoPreview();
}

// ==================== Audio Slots ====================

void MainWindow::onAudioAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择音频文件"), QString(),
        QStringLiteral("音频文件 (*.mp3 *.m4a *.aac *.flac *.wav *.ogg *.opus *.wma);;视频文件 (*.mp4 *.webm *.avi *.mov *.mkv);;所有文件 (*.*)"));
    for (const QString &f : files) {
        m_audioFileList->addItem(f);
    }
}

void MainWindow::onAudioRemoveSelected()
{
    QList<QListWidgetItem *> items = m_audioFileList->selectedItems();
    for (QListWidgetItem *item : items) {
        delete m_audioFileList->takeItem(m_audioFileList->row(item));
    }
}

void MainWindow::onAudioClearFiles()
{
    m_audioFileList->clear();
}

void MainWindow::onAudioOutputBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
    if (!dir.isEmpty())
        m_audioOutputDir->setText(dir);
}

void MainWindow::onAudioStart()
{
    if (m_audioFileList->count() == 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加文件。"));
        return;
    }
    if (m_audioOutputDir->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择输出目录。"));
        return;
    }

    m_audioTaskQueue.clear();
    m_audioCurrentIndex = 0;
    m_audioCancelling = false;
    m_audioSuccessCount = 0;
    m_audioFailedCount = 0;
    m_audioSizeBefore = 0;
    m_audioSizeAfter = 0;
    m_audioFailedFiles.clear();

    for (int i = 0; i < m_audioFileList->count(); ++i) {
        AudioTask task;
        task.inputPath = m_audioFileList->item(i)->text();
        task.outputDir = m_audioOutputDir->text();
        task.format = m_audioFormatCombo->currentData().toString();
        task.bitrate = m_audioBitrateCombo->currentData().toString();
        task.sampleRate = m_audioSampleRateCombo->currentData().toString();
        task.channels = m_audioChannelsCombo->currentData().toString();
        m_audioTaskQueue.append(task);
    }

    setAudioUiEnabled(false);
    processNextAudio();
}

void MainWindow::onAudioCancel()
{
    m_audioCancelling = true;
    m_ffmpegAudio->cancel();
    m_audioStatusLabel->setText(QStringLiteral("已取消"));
    setAudioUiEnabled(true);
    m_audioProgressBar->setValue(m_audioTaskQueue.isEmpty() ? 0 : m_audioCurrentIndex * 100 / m_audioTaskQueue.size());
}

void MainWindow::onAudioSelectionChanged()
{
    updateAudioInfoPreview();
}

// ==================== Batch Processing ====================

void MainWindow::processNextImage()
{
    if (m_imageCancelling) {
        setImageUiEnabled(true);
        return;
    }

    if (m_imageCurrentIndex >= m_imageTaskQueue.size()) {
        m_imageStatusLabel->setText(QStringLiteral("处理完成"));
        showBatchSummary(QStringLiteral("图片"), m_imageCurrentIndex,
                         m_imageSuccessCount, m_imageFailedCount,
                         m_imageSizeBefore, m_imageSizeAfter, m_imageFailedFiles);
        setImageUiEnabled(true);
        return;
    }

    const ImageTask &task = m_imageTaskQueue[m_imageCurrentIndex];
    QStringList args = ImageProcessor::buildArgs(task);
    QString outputPath = ImageProcessor::buildOutputPath(task);
    args << outputPath;

    int total = m_imageTaskQueue.size();
    int current = m_imageCurrentIndex + 1;
    m_imageProgressBar->setValue(total > 0 ? (m_imageCurrentIndex * 100 / total) : 0);
    m_imageStatusLabel->setText(
        QStringLiteral("处理中 %1/%2").arg(current).arg(total));

    QString cmdLine = m_ffmpegPath + QStringLiteral(" ") + args.join(QStringLiteral(" "));
    Utils::logToFile(QStringLiteral("[IMAGE] ") + cmdLine);

    m_ffmpegImage->start(m_ffmpegPath, args);
}

void MainWindow::processNextVideo()
{
    if (m_videoCancelling) {
        setVideoUiEnabled(true);
        return;
    }

    if (m_videoCurrentIndex >= m_videoTaskQueue.size()) {
        m_videoStatusLabel->setText(QStringLiteral("处理完成"));
        showBatchSummary(QStringLiteral("视频"), m_videoCurrentIndex,
                         m_videoSuccessCount, m_videoFailedCount,
                         m_videoSizeBefore, m_videoSizeAfter, m_videoFailedFiles);
        setVideoUiEnabled(true);
        return;
    }

    const VideoTask &task = m_videoTaskQueue[m_videoCurrentIndex];
    QStringList args = VideoProcessor::buildArgs(task);
    QString outputPath = VideoProcessor::buildOutputPath(task);
    args << outputPath;

    int total = m_videoTaskQueue.size();
    int current = m_videoCurrentIndex + 1;
    m_videoProgressBar->setValue(total > 0 ? (m_videoCurrentIndex * 100 / total) : 0);
    m_videoStatusLabel->setText(
        QStringLiteral("处理中 %1/%2").arg(current).arg(total));

    QString cmdLine = m_ffmpegPath + QStringLiteral(" ") + args.join(QStringLiteral(" "));
    Utils::logToFile(QStringLiteral("[VIDEO] ") + cmdLine);

    m_ffmpegVideo->start(m_ffmpegPath, args);
}

void MainWindow::processNextAudio()
{
    if (m_audioCancelling) {
        setAudioUiEnabled(true);
        return;
    }

    if (m_audioCurrentIndex >= m_audioTaskQueue.size()) {
        m_audioStatusLabel->setText(QStringLiteral("处理完成"));
        showBatchSummary(QStringLiteral("音频"), m_audioCurrentIndex,
                         m_audioSuccessCount, m_audioFailedCount,
                         m_audioSizeBefore, m_audioSizeAfter, m_audioFailedFiles);
        setAudioUiEnabled(true);
        return;
    }

    const AudioTask &task = m_audioTaskQueue[m_audioCurrentIndex];
    QStringList args = AudioProcessor::buildArgs(task);
    QString outputPath = AudioProcessor::buildOutputPath(task);
    args << outputPath;

    int total = m_audioTaskQueue.size();
    int current = m_audioCurrentIndex + 1;
    m_audioProgressBar->setValue(total > 0 ? (m_audioCurrentIndex * 100 / total) : 0);
    m_audioStatusLabel->setText(
        QStringLiteral("处理中 %1/%2").arg(current).arg(total));

    QString cmdLine = m_ffmpegPath + QStringLiteral(" ") + args.join(QStringLiteral(" "));
    Utils::logToFile(QStringLiteral("[AUDIO] ") + cmdLine);

    m_ffmpegAudio->start(m_ffmpegPath, args);
}

// ==================== Color Picker Tab ====================

QWidget *MainWindow::createColorPickerPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *group = new QGroupBox(QStringLiteral("屏幕取色"), page);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(14);

    m_pickColorBtn = new QPushButton(QStringLiteral("开始取色"), group);
    m_pickColorBtn->setFixedHeight(48);
    m_pickColorBtn->setFixedWidth(200);
    m_pickColorBtn->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    connect(m_pickColorBtn, &QPushButton::clicked, this, &MainWindow::onStartPickColor);

    QHBoxLayout *colorRow = new QHBoxLayout();
    m_colorSwatch = new QLabel(group);
    m_colorSwatch->setFixedSize(100, 80);
    m_colorSwatch->setStyleSheet(QStringLiteral(
        "background-color: #e9ecef; border: 2px solid #ced4da; border-radius: 6px;"));

    QGridLayout *infoLayout = new QGridLayout();
    infoLayout->setSpacing(6);
    infoLayout->setColumnStretch(2, 0);
    infoLayout->setColumnStretch(3, 1);

    QFont infoFont;
    infoFont.setPointSize(11);
    m_hexLabel = new QLabel(QStringLiteral("HEX:"), group);
    m_hexLabel->setFont(infoFont);
    m_hexLabel->setStyleSheet(QStringLiteral("color: #212529;"));
    m_hexValue = new QLabel(group);
    m_hexValue->setFont(infoFont);
    m_hexValue->setStyleSheet(QStringLiteral("color: #212529;"));
    m_copyHexBtn = new QPushButton(QStringLiteral("复制 HEX"), group);
    m_copyHexBtn->setEnabled(false);
    m_copyHexBtn->setFixedWidth(110);
    infoLayout->addWidget(m_hexLabel, 0, 0);
    infoLayout->addWidget(m_hexValue, 0, 1);
    infoLayout->addWidget(m_copyHexBtn, 0, 2);

    infoFont.setPointSize(9);
    m_rgbLabel = new QLabel(QStringLiteral("RGB:"), group);
    m_rgbLabel->setFont(infoFont);
    m_rgbLabel->setStyleSheet(QStringLiteral("color: #6c757d;"));
    m_rgbValue = new QLabel(group);
    m_rgbValue->setFont(infoFont);
    m_rgbValue->setStyleSheet(QStringLiteral("color: #6c757d;"));
    m_copyRgbBtn = new QPushButton(QStringLiteral("复制 RGB"), group);
    m_copyRgbBtn->setEnabled(false);
    m_copyRgbBtn->setFixedWidth(110);
    infoLayout->addWidget(m_rgbLabel, 1, 0);
    infoLayout->addWidget(m_rgbValue, 1, 1);
    infoLayout->addWidget(m_copyRgbBtn, 1, 2);

    m_hslLabel = new QLabel(QStringLiteral("HSL:"), group);
    m_hslLabel->setFont(infoFont);
    m_hslLabel->setStyleSheet(QStringLiteral("color: #6c757d;"));
    m_hslValue = new QLabel(group);
    m_hslValue->setFont(infoFont);
    m_hslValue->setStyleSheet(QStringLiteral("color: #6c757d;"));
    infoLayout->addWidget(m_hslLabel, 2, 0);
    infoLayout->addWidget(m_hslValue, 2, 1);

    connect(m_copyHexBtn, &QPushButton::clicked, this, &MainWindow::onCopyHex);
    connect(m_copyRgbBtn, &QPushButton::clicked, this, &MainWindow::onCopyRgb);

    colorRow->addWidget(m_colorSwatch);
    colorRow->addLayout(infoLayout);
    colorRow->addStretch();

    QLabel *historyLabel = new QLabel(QStringLiteral("最近颜色"), group);
    historyLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-weight: bold; margin-top: 4px;"));

    m_historyContainer = new QWidget(group);
    m_historyLayout = new QHBoxLayout(m_historyContainer);
    m_historyLayout->setContentsMargins(0, 0, 0, 0);
    m_historyLayout->setSpacing(6);
    m_historyLayout->addStretch();

    groupLayout->addWidget(m_pickColorBtn, 0, Qt::AlignCenter);
    groupLayout->addSpacing(8);
    groupLayout->addLayout(colorRow);
    groupLayout->addWidget(historyLabel);
    groupLayout->addWidget(m_historyContainer);
    groupLayout->addStretch();

    mainLayout->addWidget(group);
    mainLayout->addStretch();

    return page;
}

void MainWindow::onStartPickColor()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ShowWindow(hwnd, SW_HIDE);
    QTimer::singleShot(200, this, [this]() {
        m_colorPicker->begin();
    });
}

void MainWindow::onColorPicked(const QColor &color)
{
    m_pickedColor = color;

    m_colorHistory.removeAll(color);
    m_colorHistory.prepend(color);
    while (m_colorHistory.size() > 10)
        m_colorHistory.removeLast();

    HWND hwnd = reinterpret_cast<HWND>(winId());
    ShowWindow(hwnd, SW_SHOW);
    activateWindow();
    updateColorDisplay();
    updateColorHistory();
}

void MainWindow::onPickCancelled()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ShowWindow(hwnd, SW_SHOW);
    activateWindow();
}

void MainWindow::onStartScreenshot()
{
    m_screenshotForQr = false;
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ShowWindow(hwnd, SW_HIDE);
    QTimer::singleShot(200, this, [this]() {
        m_screenshotPicker->begin();
    });
}

void MainWindow::onScreenshotCaptured(const QPixmap &pixmap, QPoint globalPos)
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ShowWindow(hwnd, SW_SHOW);
    activateWindow();

    if (m_screenshotForQr) {
        m_screenshotForQr = false;
        processQrDecodeImage(pixmap.toImage(), QStringLiteral("屏幕截图"));
        return;
    }

    PinWindow *pin = new PinWindow(pixmap, globalPos);
    m_pinnedWindows.append(pin);
    connect(pin, &PinWindow::destroyed, this, [this, pin]() {
        m_pinnedWindows.removeAll(pin);
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    unregisterGlobalHotkey();
    QMainWindow::closeEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == static_cast<WPARAM>(m_hotkeyId)) {
            onStartScreenshot();
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::registerGlobalHotkey()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    unregisterGlobalHotkey();
    RegisterHotKey(hwnd, m_hotkeyId, m_hotkeyModifiers, m_hotkeyVk);
}

void MainWindow::unregisterGlobalHotkey()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    UnregisterHotKey(hwnd, m_hotkeyId);
}

QString MainWindow::hotkeyDisplayText() const
{
    QStringList parts;
    if (m_hotkeyModifiers & MOD_CONTROL) parts << QStringLiteral("Ctrl");
    if (m_hotkeyModifiers & MOD_ALT)     parts << QStringLiteral("Alt");
    if (m_hotkeyModifiers & MOD_SHIFT)   parts << QStringLiteral("Shift");
    if (m_hotkeyModifiers & MOD_WIN)     parts << QStringLiteral("Win");

    switch (m_hotkeyVk) {
        case VK_F1:  parts << QStringLiteral("F1");  break;
        case VK_F2:  parts << QStringLiteral("F2");  break;
        case VK_F3:  parts << QStringLiteral("F3");  break;
        case VK_F4:  parts << QStringLiteral("F4");  break;
        case VK_F5:  parts << QStringLiteral("F5");  break;
        case VK_F6:  parts << QStringLiteral("F6");  break;
        case VK_F7:  parts << QStringLiteral("F7");  break;
        case VK_F8:  parts << QStringLiteral("F8");  break;
        case VK_F9:  parts << QStringLiteral("F9");  break;
        case VK_F10: parts << QStringLiteral("F10"); break;
        case VK_F11: parts << QStringLiteral("F11"); break;
        case VK_F12: parts << QStringLiteral("F12"); break;
        default: {
            UINT scan = MapVirtualKeyW(m_hotkeyVk, MAPVK_VK_TO_VSC);
            wchar_t name[64] = {};
            if (GetKeyNameTextW(scan << 16, name, 64))
                parts << QString::fromWCharArray(name);
            else
                parts << QStringLiteral("0x%1").arg(m_hotkeyVk, 0, 16);
            break;
        }
    }

    return parts.join(QStringLiteral(" + "));
}

void MainWindow::onChangeScreenshotHotkey()
{
    HotkeyDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    UINT newVk = dlg.vk;
    UINT newMod = dlg.modifiers;

    if (newVk == m_hotkeyVk && newMod == m_hotkeyModifiers)
        return;

    m_hotkeyVk = newVk;
    m_hotkeyModifiers = newMod;
    registerGlobalHotkey();

    QString configPath = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("screenshot/hotkeyVk"), m_hotkeyVk);
    settings.setValue(QStringLiteral("screenshot/hotkeyModifiers"), m_hotkeyModifiers);

    if (m_screenshotHotkeyLabel) {
        m_screenshotHotkeyLabel->setText(
            QStringLiteral("\u5F53\u524D\u5FEB\u6377\u952E\uFF1A%1").arg(hotkeyDisplayText()));
    }
}

void MainWindow::onCopyHex()
{
    QApplication::clipboard()->setText(m_pickedColor.name().toUpper());
    QString original = m_copyHexBtn->text();
    m_copyHexBtn->setText(QStringLiteral("已复制 HEX"));
    m_copyHexBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyHexBtn->setText(original);
        m_copyHexBtn->setEnabled(true);
    });
}

void MainWindow::onCopyRgb()
{
    QString rgb = QStringLiteral("rgb(%1, %2, %3)")
        .arg(m_pickedColor.red())
        .arg(m_pickedColor.green())
        .arg(m_pickedColor.blue());
    QApplication::clipboard()->setText(rgb);
    QString original = m_copyRgbBtn->text();
    m_copyRgbBtn->setText(QStringLiteral("已复制 RGB"));
    m_copyRgbBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyRgbBtn->setText(original);
        m_copyRgbBtn->setEnabled(true);
    });
}

void MainWindow::onHistoryColorClicked(int index)
{
    if (index < 0 || index >= m_colorHistory.size()) return;
    m_pickedColor = m_colorHistory[index];
    updateColorDisplay();
}

void MainWindow::updateColorDisplay()
{
    QString colorHex = m_pickedColor.name().toUpper();
    m_colorSwatch->setStyleSheet(QStringLiteral(
        "background-color: %1; border: 2px solid #adb5bd; border-radius: 6px;").arg(colorHex));
    m_hexValue->setText(colorHex);
    m_rgbValue->setText(QStringLiteral("%1, %2, %3")
        .arg(m_pickedColor.red())
        .arg(m_pickedColor.green())
        .arg(m_pickedColor.blue()));

    int hue = m_pickedColor.hslHue();
    int sat = m_pickedColor.hslSaturation() * 100 / 255;
    int light = m_pickedColor.lightness() * 100 / 255;
    m_hslValue->setText(QStringLiteral("%1°, %2%, %3%")
        .arg(hue >= 0 ? hue : 0)
        .arg(sat)
        .arg(light));

    m_copyHexBtn->setEnabled(true);
    m_copyRgbBtn->setEnabled(true);
}

void MainWindow::updateColorHistory()
{
    QLayoutItem *item;
    while ((item = m_historyLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            delete item->widget();
        delete item;
    }

    for (int i = 0; i < m_colorHistory.size(); i++) {
        QPushButton *btn = new QPushButton(m_historyContainer);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(m_colorHistory[i].name().toUpper());
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; border: 2px solid #dee2e6; border-radius: 4px; }"
            "QPushButton:hover { border-color: #0d6efd; }"
        ).arg(m_colorHistory[i].name()));

        int idx = i;
        connect(btn, &QPushButton::clicked, this, [this, idx]() {
            onHistoryColorClicked(idx);
        });

        m_historyLayout->addWidget(btn);
    }
    m_historyLayout->addStretch();
}

QWidget *MainWindow::createStickyNotePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *group = new QGroupBox(QStringLiteral("\u622A\u56FE\u8D34\u56FE"), page);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(14);
    groupLayout->setAlignment(Qt::AlignCenter);

    QPushButton *startBtn = new QPushButton(QStringLiteral("\u5F00\u59CB\u8D34\u56FE"), group);
    startBtn->setFixedHeight(48);
    startBtn->setFixedWidth(220);
    startBtn->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::onStartScreenshot);

    QHBoxLayout *hotkeyRow = new QHBoxLayout();
    hotkeyRow->setSpacing(8);
    hotkeyRow->setAlignment(Qt::AlignCenter);

    m_screenshotHotkeyLabel = new QLabel(group);
    m_screenshotHotkeyLabel->setStyleSheet(QStringLiteral("color: #0d6efd; font-size: 13px; font-weight: bold;"));
    m_screenshotHotkeyLabel->setText(
        QStringLiteral("\u5F53\u524D\u5FEB\u6377\u952E\uFF1A%1").arg(hotkeyDisplayText()));

    QPushButton *changeHotkeyBtn = new QPushButton(QStringLiteral("\u4FEE\u6539"), group);
    changeHotkeyBtn->setFixedHeight(28);
    changeHotkeyBtn->setFixedWidth(56);
    changeHotkeyBtn->setCursor(Qt::PointingHandCursor);
    changeHotkeyBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: #0d6efd; border: 1px solid #0d6efd; "
        "  border-radius: 4px; font-size: 12px; padding: 2px 8px; }"
        "QPushButton:hover { background-color: #0d6efd; color: #ffffff; }"));
    connect(changeHotkeyBtn, &QPushButton::clicked, this, &MainWindow::onChangeScreenshotHotkey);

    hotkeyRow->addWidget(m_screenshotHotkeyLabel);
    hotkeyRow->addWidget(changeHotkeyBtn);

    QLabel *tipLabel = new QLabel(group);
    tipLabel->setAlignment(Qt::AlignCenter);
    tipLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px; line-height: 1.6;"));
    tipLabel->setText(QStringLiteral(
        "\u70B9\u51FB\u201C\u5F00\u59CB\u8D34\u56FE\u201D\u6216\u6309\u5FEB\u6377\u952E\uFF0C\u7A97\u53E3\u4F1A\u81EA\u52A8\u9690\u85CF\n"
        "\u7528\u9F20\u6807\u62D6\u62FD\u9009\u62E9\u622A\u56FE\u533A\u57DF\uFF0C\u677E\u5F00\u5373\u53EF\u8D34\u56FE\n"
        "\u652F\u6301\u591A\u4E2A\u8D34\u56FE\u540C\u65F6\u663E\u793A\uFF0C\u53F3\u952E\u83DC\u5355\u590D\u5236/\u4FDD\u5B58"));

    groupLayout->addWidget(startBtn);
    groupLayout->addLayout(hotkeyRow);
    groupLayout->addSpacing(4);
    groupLayout->addWidget(tipLabel);
    groupLayout->addStretch();

    mainLayout->addWidget(group);
    mainLayout->addStretch();

    return page;
}

QWidget *MainWindow::createTransparencyPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *listGroup = new QGroupBox(QStringLiteral("运行中的窗口"), page);
    QVBoxLayout *listLayout = new QVBoxLayout(listGroup);
    listLayout->setSpacing(10);

    m_transparencyRefreshBtn = new QPushButton(QStringLiteral("刷新窗口列表"), listGroup);
    m_transparencyRefreshBtn->setFixedHeight(36);
    m_transparencyRefreshBtn->setFixedWidth(180);
    m_transparencyRefreshBtn->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    connect(m_transparencyRefreshBtn, &QPushButton::clicked, this, &MainWindow::onTransparencyRefresh);

    m_transparencyPickBtn = new QPushButton(QStringLiteral("选取窗口"), listGroup);
    m_transparencyPickBtn->setFixedHeight(36);
    m_transparencyPickBtn->setFixedWidth(140);
    m_transparencyPickBtn->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    connect(m_transparencyPickBtn, &QPushButton::clicked, this, &MainWindow::onTransparencyPickWindow);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addWidget(m_transparencyPickBtn);
    btnRow->addWidget(m_transparencyRefreshBtn);
    btnRow->addStretch();

    m_transparencyWindowList = new QListWidget(listGroup);
    m_transparencyWindowList->setMinimumHeight(280);
    m_transparencyWindowList->setStyleSheet(QStringLiteral(
        "QListWidget::item { padding: 6px 10px; }"));
    connect(m_transparencyWindowList, &QListWidget::currentRowChanged,
            this, &MainWindow::onTransparencySelectionChanged);

    listLayout->addLayout(btnRow);
    listLayout->addWidget(m_transparencyWindowList);

    QGroupBox *opacityGroup = new QGroupBox(QStringLiteral("不透明度设置"), page);
    opacityGroup->setEnabled(false);
    QVBoxLayout *opacityLayout = new QVBoxLayout(opacityGroup);
    opacityLayout->setSpacing(10);

    QHBoxLayout *sliderRow = new QHBoxLayout();
    QLabel *opacityLabel = new QLabel(QStringLiteral("不透明度:"), opacityGroup);
    opacityLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: #212529; font-weight: normal;"));

    m_transparencySlider = new QSlider(Qt::Horizontal, opacityGroup);
    m_transparencySlider->setRange(30, 100);
    m_transparencySlider->setValue(100);
    m_transparencySlider->setTickPosition(QSlider::NoTicks);
    m_transparencySlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 8px; background: #dee2e6; border-radius: 4px; }"
        "QSlider::sub-page:horizontal { background: #0d6efd; border-radius: 4px; }"
        "QSlider::handle:horizontal { width: 18px; height: 18px; margin: -5px 0; "
        "  background: #0d6efd; border-radius: 9px; }"
        "QSlider::handle:horizontal:hover { background: #0b5ed7; }"
    ));
    connect(m_transparencySlider, &QSlider::valueChanged,
            this, &MainWindow::onTransparencySliderChanged);

    m_transparencyValueLabel = new QLabel(QStringLiteral("100%"), opacityGroup);
    m_transparencyValueLabel->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: bold; color: #0d6efd;"));
    m_transparencyValueLabel->setFixedWidth(45);

    sliderRow->addWidget(opacityLabel);
    sliderRow->addWidget(m_transparencySlider, 1);
    sliderRow->addWidget(m_transparencyValueLabel);

    opacityLayout->addLayout(sliderRow);

    m_transparencyStatusLabel = new QLabel(
        QStringLiteral("提示：请先选择目标窗口，再拖动滑块调整不透明度"), page);
    m_transparencyStatusLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 12px; padding-top: 4px;"));

    mainLayout->addWidget(listGroup);
    mainLayout->addWidget(opacityGroup);
    mainLayout->addWidget(m_transparencyStatusLabel);
    mainLayout->addStretch();

    return page;
}

void MainWindow::onTransparencyRefresh()
{
    m_transparencyWindowList->clear();

    EnumWindowsContext ctx;
    ctx.hwnds = new QList<HWND>();
    ctx.exclude = reinterpret_cast<HWND>(winId());

    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    for (int i = 0; i < ctx.hwnds->size(); ++i) {
        HWND hwnd = ctx.hwnds->at(i);
        int len = GetWindowTextLengthW(hwnd);
        wchar_t *buf = new wchar_t[len + 1];
        GetWindowTextW(hwnd, buf, len + 1);
        QString title = QString::fromWCharArray(buf);
        delete[] buf;

        QListWidgetItem *item = new QListWidgetItem(title, m_transparencyWindowList);
        item->setData(Qt::UserRole, reinterpret_cast<quintptr>(hwnd));
    }

    delete ctx.hwnds;

    HWND selfHwnd = reinterpret_cast<HWND>(winId());
    for (int i = m_transparencyWindowList->count() - 1; i >= 0; --i) {
        QListWidgetItem *item = m_transparencyWindowList->item(i);
        HWND hwnd = reinterpret_cast<HWND>(item->data(Qt::UserRole).value<quintptr>());
        if (hwnd == selfHwnd || !IsWindow(hwnd)) {
            delete m_transparencyWindowList->takeItem(i);
        }
    }

    if (m_transparencyWindowList->count() == 0) {
        QWidget *opacityGroup = m_transparencySlider->parentWidget();
        if (opacityGroup)
            opacityGroup->setEnabled(false);
    }

    m_transparencyStatusLabel->setText(
        QStringLiteral("找到 %1 个窗口，请选择目标窗口").arg(m_transparencyWindowList->count()));
}

void MainWindow::onTransparencySelectionChanged()
{
    QListWidgetItem *item = m_transparencyWindowList->currentItem();
    if (!item) {
        QWidget *opacityGroup = m_transparencySlider->parentWidget();
        if (opacityGroup)
            opacityGroup->setEnabled(false);
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(item->data(Qt::UserRole).value<quintptr>());
    HWND selfHwnd = reinterpret_cast<HWND>(winId());

    if (!IsWindow(hwnd) || hwnd == selfHwnd) {
        QWidget *opacityGroup = m_transparencySlider->parentWidget();
        if (opacityGroup)
            opacityGroup->setEnabled(false);
        return;
    }

    m_transparencyTargetHwnd = hwnd;
    m_transparencyOriginalExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    QWidget *opacityGroup = m_transparencySlider->parentWidget();
    opacityGroup->setEnabled(true);

    m_transparencySlider->blockSignals(true);
    m_transparencySlider->setValue(100);
    m_transparencySlider->blockSignals(false);
    m_transparencyValueLabel->setText(QStringLiteral("100%"));

    m_transparencyStatusLabel->setText(
        QStringLiteral("已选择 \"%1\"，拖动滑块调整不透明度").arg(item->text()));
}

void MainWindow::onTransparencySliderChanged(int value)
{
    m_transparencyValueLabel->setText(QStringLiteral("%1%").arg(value));

    if (!m_transparencyTargetHwnd || !IsWindow(m_transparencyTargetHwnd)
        || m_transparencyTargetHwnd == reinterpret_cast<HWND>(winId())) {
        m_transparencyStatusLabel->setText(QStringLiteral("目标窗口已不存在，请重新选择"));
        return;
    }

    if (value >= 100) {
        SetWindowLongPtrW(m_transparencyTargetHwnd, GWL_EXSTYLE,
                          m_transparencyOriginalExStyle);
        SetWindowPos(m_transparencyTargetHwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        m_transparencyStatusLabel->setText(QStringLiteral("窗口已恢复为不透明"));
        return;
    }

    LONG_PTR newExStyle = m_transparencyOriginalExStyle | WS_EX_LAYERED;
    SetWindowLongPtrW(m_transparencyTargetHwnd, GWL_EXSTYLE, newExStyle);

    int alpha = qRound(value * 255.0 / 100.0);
    SetLayeredWindowAttributes(m_transparencyTargetHwnd, 0, alpha, LWA_ALPHA);

    m_transparencyStatusLabel->setText(
        QStringLiteral("当前不透明度: %1%").arg(value));
}

void MainWindow::onTransparencyPickWindow()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ShowWindow(hwnd, SW_HIDE);
    QTimer::singleShot(200, this, [this]() {
        m_windowPicker->begin();
    });
}

void MainWindow::onWindowPicked(HWND hwnd)
{
    HWND selfHwnd = reinterpret_cast<HWND>(winId());
    ShowWindow(selfHwnd, SW_SHOW);
    activateWindow();

    if (!hwnd || !IsWindow(hwnd) || hwnd == selfHwnd)
        return;

    for (int i = 0; i < m_transparencyWindowList->count(); ++i) {
        QListWidgetItem *item = m_transparencyWindowList->item(i);
        HWND itemHwnd = reinterpret_cast<HWND>(item->data(Qt::UserRole).value<quintptr>());
        if (itemHwnd == hwnd) {
            m_transparencyWindowList->setCurrentRow(i);
            return;
        }
    }

    int len = GetWindowTextLengthW(hwnd);
    if (len > 0) {
        wchar_t *buf = new wchar_t[len + 1];
        GetWindowTextW(hwnd, buf, len + 1);
        QString title = QString::fromWCharArray(buf);
        delete[] buf;

        QListWidgetItem *item = new QListWidgetItem(title);
        item->setData(Qt::UserRole, reinterpret_cast<quintptr>(hwnd));
        m_transparencyWindowList->insertItem(0, item);
        m_transparencyWindowList->setCurrentRow(0);
    }
}

QWidget *MainWindow::createTimerPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *displayGroup = new QGroupBox(QStringLiteral("计时"), page);
    QVBoxLayout *displayLayout = new QVBoxLayout(displayGroup);
    displayLayout->setAlignment(Qt::AlignCenter);

    m_timerDisplay = new QLabel(QStringLiteral("00:00:00.00"), displayGroup);
    m_timerDisplay->setAlignment(Qt::AlignCenter);
    m_timerDisplay->setStyleSheet(QStringLiteral(
        "font-family: 'Consolas', 'Courier New', monospace;"
        "font-size: 52px;"
        "font-weight: bold;"
        "color: #212529;"
        "padding: 16px 0;"
    ));

    displayLayout->addWidget(m_timerDisplay);

    QGroupBox *controlsGroup = new QGroupBox(QStringLiteral("控制"), page);
    QHBoxLayout *controlsLayout = new QHBoxLayout(controlsGroup);
    controlsLayout->setSpacing(10);

    m_timerStartBtn = new QPushButton(QStringLiteral("开始"), controlsGroup);
    m_timerStartBtn->setFixedHeight(40);
    m_timerStartBtn->setMinimumWidth(80);
    m_timerStartBtn->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 15px; font-weight: bold; background-color: #198754; }"
        "QPushButton:hover { background-color: #157347; }"
        "QPushButton:pressed { background-color: #146c43; }"
        "QPushButton:disabled { background-color: #6c757d; color: #ced4da; }"
    ));
    connect(m_timerStartBtn, &QPushButton::clicked, this, &MainWindow::onTimerStart);

    m_timerPauseBtn = new QPushButton(QStringLiteral("暂停"), controlsGroup);
    m_timerPauseBtn->setFixedHeight(40);
    m_timerPauseBtn->setMinimumWidth(80);
    m_timerPauseBtn->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 15px; font-weight: bold; background-color: #fd7e14; }"
        "QPushButton:hover { background-color: #e06b0c; }"
        "QPushButton:pressed { background-color: #c95e0a; }"
        "QPushButton:disabled { background-color: #6c757d; color: #ced4da; }"
    ));
    m_timerPauseBtn->setEnabled(false);
    connect(m_timerPauseBtn, &QPushButton::clicked, this, &MainWindow::onTimerPause);

    m_timerStopBtn = new QPushButton(QStringLiteral("停止"), controlsGroup);
    m_timerStopBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_timerStopBtn->setFixedHeight(40);
    m_timerStopBtn->setMinimumWidth(80);
    m_timerStopBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_timerStopBtn->setEnabled(false);
    connect(m_timerStopBtn, &QPushButton::clicked, this, &MainWindow::onTimerStop);

    m_timerLapBtn = new QPushButton(QStringLiteral("计次"), controlsGroup);
    m_timerLapBtn->setFixedHeight(40);
    m_timerLapBtn->setMinimumWidth(80);
    m_timerLapBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_timerLapBtn->setEnabled(false);
    connect(m_timerLapBtn, &QPushButton::clicked, this, &MainWindow::onTimerLap);

    controlsLayout->addWidget(m_timerStartBtn);
    controlsLayout->addWidget(m_timerPauseBtn);
    controlsLayout->addWidget(m_timerStopBtn);
    controlsLayout->addWidget(m_timerLapBtn);

    QGroupBox *lapsGroup = new QGroupBox(QStringLiteral("计次记录"), page);
    QVBoxLayout *lapsLayout = new QVBoxLayout(lapsGroup);
    lapsLayout->setSpacing(8);

    m_timerLapList = new QTableWidget(0, 3, lapsGroup);
    m_timerLapList->setMinimumHeight(180);
    m_timerLapList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_timerLapList->setSelectionMode(QAbstractItemView::NoSelection);
    m_timerLapList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_timerLapList->setFocusPolicy(Qt::NoFocus);
    m_timerLapList->verticalHeader()->setVisible(false);
    m_timerLapList->horizontalHeader()->setStretchLastSection(true);
    m_timerLapList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_timerLapList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_timerLapList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    QStringList headers;
    headers << QStringLiteral("序号") << QStringLiteral("分段时间") << QStringLiteral("累计时间");
    m_timerLapList->setHorizontalHeaderLabels(headers);

    m_timerLapList->setStyleSheet(QStringLiteral(
        "QTableWidget { font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f3f5; color: #495057; font-weight: bold; "
        "  padding: 4px 8px; border: none; border-bottom: 2px solid #dee2e6; font-size: 12px; }"
    ));

    m_timerClearBtn = new QPushButton(QStringLiteral("清除记录"), lapsGroup);
    m_timerClearBtn->setFixedHeight(32);
    m_timerClearBtn->setFixedWidth(100);
    m_timerClearBtn->setStyleSheet(QStringLiteral("font-size: 12px;"));
    connect(m_timerClearBtn, &QPushButton::clicked, this, &MainWindow::onTimerClearLaps);

    lapsLayout->addWidget(m_timerLapList);
    lapsLayout->addWidget(m_timerClearBtn, 0, Qt::AlignLeft);

    mainLayout->addWidget(displayGroup);
    mainLayout->addWidget(controlsGroup);
    mainLayout->addWidget(lapsGroup, 1);

    return page;
}

QWidget *MainWindow::createBase64Page()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *group = new QGroupBox(QStringLiteral("图片转Base64"), page);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(12);

    QHBoxLayout *fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);

    m_base64FilePath = new QLineEdit(group);
    m_base64FilePath->setReadOnly(true);
    m_base64FilePath->setPlaceholderText(QStringLiteral("请选择图片文件，或拖放图片到下方区域..."));

    m_base64SelectBtn = new QPushButton(QStringLiteral("选择图片"), group);
    m_base64SelectBtn->setFixedHeight(34);
    m_base64SelectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_base64SelectBtn, &QPushButton::clicked, this, &MainWindow::onBase64SelectFile);

    m_base64ClearBtn = new QPushButton(QStringLiteral("清除"), group);
    m_base64ClearBtn->setFixedHeight(34);
    m_base64ClearBtn->setCursor(Qt::PointingHandCursor);
    m_base64ClearBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    m_base64ClearBtn->setEnabled(false);
    connect(m_base64ClearBtn, &QPushButton::clicked, this, &MainWindow::onBase64Clear);

    fileRow->addWidget(m_base64FilePath, 1);
    fileRow->addWidget(m_base64SelectBtn);
    fileRow->addWidget(m_base64ClearBtn);

    m_base64DropZone = new QLabel(group);
    m_base64DropZone->setFixedHeight(100);
    m_base64DropZone->setAlignment(Qt::AlignCenter);
    m_base64DropZone->setAcceptDrops(true);
    m_base64DropZone->setCursor(Qt::PointingHandCursor);
    m_base64DropZone->setStyleSheet(QStringLiteral(
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
    m_base64DropZone->setText(QStringLiteral("将图片拖放到此处\n或点击上方按钮选择文件"));
    m_base64DropZone->installEventFilter(this);

    QGroupBox *resultGroup = new QGroupBox(QStringLiteral("转换结果"), page);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setSpacing(10);

    m_base64Output = new QTextEdit(resultGroup);
    m_base64Output->setReadOnly(true);
    m_base64Output->setPlaceholderText(QStringLiteral("选择图片后将在此显示 Base64 编码结果..."));
    m_base64Output->setMinimumHeight(180);
    m_base64Output->setStyleSheet(QStringLiteral(
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

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(16);

    m_base64InfoLabel = new QLabel(resultGroup);
    m_base64InfoLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));

    m_base64CopyBtn = new QPushButton(QStringLiteral("复制到剪贴板"), resultGroup);
    m_base64CopyBtn->setFixedHeight(34);
    m_base64CopyBtn->setCursor(Qt::PointingHandCursor);
    m_base64CopyBtn->setEnabled(false);
    connect(m_base64CopyBtn, &QPushButton::clicked, this, &MainWindow::onBase64Copy);

    bottomRow->addWidget(m_base64InfoLabel, 1);
    bottomRow->addWidget(m_base64CopyBtn);

    resultLayout->addWidget(m_base64Output);
    resultLayout->addLayout(bottomRow);

    groupLayout->addLayout(fileRow);
    groupLayout->addWidget(m_base64DropZone);
    groupLayout->addWidget(resultGroup, 1);

    mainLayout->addWidget(group, 1);

    return page;
}

void MainWindow::onBase64SelectFile()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        QStringLiteral("选择图片文件"), QString(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp *.gif *.svg *.ico *.tiff *.tif)"));
    if (filePath.isEmpty())
        return;
    processBase64File(filePath);
}

void MainWindow::onBase64Clear()
{
    m_base64FilePath->clear();
    m_base64Output->clear();
    m_base64InfoLabel->clear();
    m_base64CopyBtn->setEnabled(false);
    m_base64ClearBtn->setEnabled(false);
    m_base64DropZone->setText(QStringLiteral("将图片拖放到此处\n或点击上方按钮选择文件"));
    m_base64DropZone->setStyleSheet(QStringLiteral(
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
}

void MainWindow::onBase64Copy()
{
    QString text = m_base64Output->toPlainText();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_base64CopyBtn->text();
    m_base64CopyBtn->setText(QStringLiteral("已复制"));
    m_base64CopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_base64CopyBtn->setText(original);
        m_base64CopyBtn->setEnabled(true);
    });
}

void MainWindow::onTimestampUpdate()
{
    QDateTime now = QDateTime::currentDateTime();
    m_timestampNowSecEdit->setText(QString::number(now.toSecsSinceEpoch()));
    m_timestampNowMsEdit->setText(QString::number(now.toMSecsSinceEpoch()));
    m_timestampNowLabel->setText(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

void MainWindow::onTimestampNowSecCopy()
{
    QString text = m_timestampNowSecEdit->text();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_timestampNowSecCopyBtn->text();
    m_timestampNowSecCopyBtn->setText(QStringLiteral("已复制"));
    m_timestampNowSecCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_timestampNowSecCopyBtn->setText(original);
        m_timestampNowSecCopyBtn->setEnabled(true);
    });
}

void MainWindow::onTimestampNowMsCopy()
{
    QString text = m_timestampNowMsEdit->text();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_timestampNowMsCopyBtn->text();
    m_timestampNowMsCopyBtn->setText(QStringLiteral("已复制"));
    m_timestampNowMsCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_timestampNowMsCopyBtn->setText(original);
        m_timestampNowMsCopyBtn->setEnabled(true);
    });
}

void MainWindow::onTimestampInputChanged()
{
    QString text = m_timestampInputEdit->text();
    if (text.isEmpty()) {
        m_timestampResultLabel->setText(QStringLiteral("-"));
        m_timestampResultCopyBtn->setEnabled(false);
        return;
    }

    bool ok;
    qint64 ts = text.toLongLong(&ok);
    if (!ok) {
        m_timestampResultLabel->setText(QStringLiteral("无效的时间戳"));
        m_timestampResultCopyBtn->setEnabled(false);
        return;
    }

    QDateTime dt;
    if (m_timestampMsRadio->isChecked()) {
        dt = QDateTime::fromMSecsSinceEpoch(ts);
    } else {
        dt = QDateTime::fromSecsSinceEpoch(ts);
    }

    if (!dt.isValid()) {
        m_timestampResultLabel->setText(QStringLiteral("无效的时间戳"));
        m_timestampResultCopyBtn->setEnabled(false);
        return;
    }

    m_timestampResultLabel->setText(dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    m_timestampResultCopyBtn->setEnabled(true);
}

void MainWindow::onTimestampResultCopy()
{
    QString text = m_timestampResultLabel->text();
    if (text.isEmpty() || text == QStringLiteral("-"))
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_timestampResultCopyBtn->text();
    m_timestampResultCopyBtn->setText(QStringLiteral("已复制"));
    m_timestampResultCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_timestampResultCopyBtn->setText(original);
        m_timestampResultCopyBtn->setEnabled(true);
    });
}

void MainWindow::onDatetimeInputChanged()
{
    QDateTime dt = m_datetimeInputEdit->dateTime();
    m_datetimeSecResultEdit->setText(QString::number(dt.toSecsSinceEpoch()));
    m_datetimeMsResultEdit->setText(QString::number(dt.toMSecsSinceEpoch()));
}

void MainWindow::onDatetimeSecCopy()
{
    QString text = m_datetimeSecResultEdit->text();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_datetimeSecCopyBtn->text();
    m_datetimeSecCopyBtn->setText(QStringLiteral("已复制"));
    m_datetimeSecCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_datetimeSecCopyBtn->setText(original);
        m_datetimeSecCopyBtn->setEnabled(true);
    });
}

void MainWindow::onDatetimeMsCopy()
{
    QString text = m_datetimeMsResultEdit->text();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_datetimeMsCopyBtn->text();
    m_datetimeMsCopyBtn->setText(QStringLiteral("已复制"));
    m_datetimeMsCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_datetimeMsCopyBtn->setText(original);
        m_datetimeMsCopyBtn->setEnabled(true);
    });
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_base64DropZone) {
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
                        processBase64File(url.toLocalFile());
                        return true;
                    }
                }
            }
        }
    }
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

void MainWindow::processBase64File(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法读取文件：%1").arg(filePath));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("文件为空：%1").arg(filePath));
        return;
    }

    QString base64 = data.toBase64();
    QString mime = mimeTypeForFile(filePath);
    QString result = QStringLiteral("data:%1;base64,%2").arg(mime, base64);

    QFileInfo fi(filePath);
    qint64 originalSize = data.size();
    qint64 base64Size = result.size();

    m_base64FilePath->setText(QDir::toNativeSeparators(filePath));
    m_base64Output->setPlainText(result);
    m_base64InfoLabel->setText(QStringLiteral("文件: %1 | Base64: %2")
        .arg(Utils::formatFileSize(originalSize), Utils::formatFileSize(base64Size)));
    m_base64CopyBtn->setEnabled(true);
    m_base64ClearBtn->setEnabled(true);
    m_base64DropZone->setText(fi.fileName());
    m_base64DropZone->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  border: 2px solid #198754;"
        "  border-radius: 8px;"
        "  background-color: #d1e7dd;"
        "  color: #0f5132;"
        "  font-size: 13px;"
        "}"));
}

QString MainWindow::mimeTypeForFile(const QString &filePath) const
{
    static const QMap<QString, QString> extMap = {
        {QStringLiteral("jpg"),  QStringLiteral("image/jpeg")},
        {QStringLiteral("jpeg"), QStringLiteral("image/jpeg")},
        {QStringLiteral("png"),  QStringLiteral("image/png")},
        {QStringLiteral("webp"), QStringLiteral("image/webp")},
        {QStringLiteral("bmp"),  QStringLiteral("image/bmp")},
        {QStringLiteral("gif"),  QStringLiteral("image/gif")},
        {QStringLiteral("svg"),  QStringLiteral("image/svg+xml")},
        {QStringLiteral("ico"),  QStringLiteral("image/x-icon")},
        {QStringLiteral("tiff"), QStringLiteral("image/tiff")},
        {QStringLiteral("tif"),  QStringLiteral("image/tiff")},
    };

    QString ext = QFileInfo(filePath).suffix().toLower();
    return extMap.value(ext, QStringLiteral("image/png"));
}

QWidget *MainWindow::createTimestampPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *nowGroup = new QGroupBox(QStringLiteral("当前时间戳"), page);
    QVBoxLayout *nowLayout = new QVBoxLayout(nowGroup);
    nowLayout->setSpacing(10);

    QHBoxLayout *secRow = new QHBoxLayout();
    secRow->setSpacing(8);
    QLabel *secLabel = new QLabel(QStringLiteral("秒级:"), nowGroup);
    secLabel->setFixedWidth(60);
    m_timestampNowSecEdit = new QLineEdit(nowGroup);
    m_timestampNowSecEdit->setReadOnly(true);
    m_timestampNowSecEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    m_timestampNowSecCopyBtn = new QPushButton(QStringLiteral("复制"), nowGroup);
    m_timestampNowSecCopyBtn->setFixedHeight(30);
    m_timestampNowSecCopyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_timestampNowSecCopyBtn, &QPushButton::clicked,
            this, &MainWindow::onTimestampNowSecCopy);
    secRow->addWidget(secLabel);
    secRow->addWidget(m_timestampNowSecEdit, 1);
    secRow->addWidget(m_timestampNowSecCopyBtn);

    QHBoxLayout *msRow = new QHBoxLayout();
    msRow->setSpacing(8);
    QLabel *msLabel = new QLabel(QStringLiteral("毫秒级:"), nowGroup);
    msLabel->setFixedWidth(60);
    m_timestampNowMsEdit = new QLineEdit(nowGroup);
    m_timestampNowMsEdit->setReadOnly(true);
    m_timestampNowMsEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    m_timestampNowMsCopyBtn = new QPushButton(QStringLiteral("复制"), nowGroup);
    m_timestampNowMsCopyBtn->setFixedHeight(30);
    m_timestampNowMsCopyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_timestampNowMsCopyBtn, &QPushButton::clicked,
            this, &MainWindow::onTimestampNowMsCopy);
    msRow->addWidget(msLabel);
    msRow->addWidget(m_timestampNowMsEdit, 1);
    msRow->addWidget(m_timestampNowMsCopyBtn);

    QHBoxLayout *localRow = new QHBoxLayout();
    localRow->setSpacing(8);
    QLabel *localLabel = new QLabel(QStringLiteral("本地时间:"), nowGroup);
    localLabel->setFixedWidth(60);
    m_timestampNowLabel = new QLabel(nowGroup);
    m_timestampNowLabel->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace; "
                       "font-size: 14px; font-weight: bold;"));
    localRow->addWidget(localLabel);
    localRow->addWidget(m_timestampNowLabel, 1);

    nowLayout->addLayout(secRow);
    nowLayout->addLayout(msRow);
    nowLayout->addLayout(localRow);

    QGroupBox *tsToDtGroup = new QGroupBox(QStringLiteral("时间戳 → 日期时间"), page);
    QVBoxLayout *tsToDtLayout = new QVBoxLayout(tsToDtGroup);
    tsToDtLayout->setSpacing(10);

    QHBoxLayout *inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);
    m_timestampInputEdit = new QLineEdit(tsToDtGroup);
    m_timestampInputEdit->setPlaceholderText(QStringLiteral("输入时间戳..."));
    m_timestampInputEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    m_timestampInputEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("-?\\d+")),
                                        m_timestampInputEdit));
    connect(m_timestampInputEdit, &QLineEdit::textChanged,
            this, &MainWindow::onTimestampInputChanged);

    m_timestampSecRadio = new QRadioButton(QStringLiteral("秒"), tsToDtGroup);
    m_timestampMsRadio = new QRadioButton(QStringLiteral("毫秒"), tsToDtGroup);
    m_timestampSecRadio->setChecked(true);
    QButtonGroup *unitGroup = new QButtonGroup(this);
    unitGroup->addButton(m_timestampSecRadio);
    unitGroup->addButton(m_timestampMsRadio);
    connect(unitGroup, &QButtonGroup::buttonClicked,
            this, [this](QAbstractButton *) { onTimestampInputChanged(); });

    inputRow->addWidget(m_timestampInputEdit, 1);
    inputRow->addWidget(m_timestampSecRadio);
    inputRow->addWidget(m_timestampMsRadio);

    QHBoxLayout *tsResultRow = new QHBoxLayout();
    tsResultRow->setSpacing(8);
    QLabel *tsResultLabel = new QLabel(QStringLiteral("结果:"), tsToDtGroup);
    tsResultLabel->setFixedWidth(60);
    m_timestampResultLabel = new QLabel(QStringLiteral("-"), tsToDtGroup);
    m_timestampResultLabel->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace; "
                       "font-size: 14px; font-weight: bold;"));
    m_timestampResultCopyBtn = new QPushButton(QStringLiteral("复制"), tsToDtGroup);
    m_timestampResultCopyBtn->setFixedHeight(30);
    m_timestampResultCopyBtn->setCursor(Qt::PointingHandCursor);
    m_timestampResultCopyBtn->setEnabled(false);
    connect(m_timestampResultCopyBtn, &QPushButton::clicked,
            this, &MainWindow::onTimestampResultCopy);
    tsResultRow->addWidget(tsResultLabel);
    tsResultRow->addWidget(m_timestampResultLabel, 1);
    tsResultRow->addWidget(m_timestampResultCopyBtn);

    tsToDtLayout->addLayout(inputRow);
    tsToDtLayout->addLayout(tsResultRow);

    QGroupBox *dtToTsGroup = new QGroupBox(QStringLiteral("日期时间 → 时间戳"), page);
    QVBoxLayout *dtToTsLayout = new QVBoxLayout(dtToTsGroup);
    dtToTsLayout->setSpacing(10);

    m_datetimeInputEdit = new QDateTimeEdit(QDateTime::currentDateTime(), dtToTsGroup);
    m_datetimeInputEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_datetimeInputEdit->setCalendarPopup(true);
    m_datetimeInputEdit->setStyleSheet(
        QStringLiteral("QDateTimeEdit {"
                       "  border: 1px solid #ced4da;"
                       "  border-radius: 4px;"
                       "  padding: 6px 10px;"
                       "  background-color: #ffffff;"
                       "  color: #212529;"
                       "  font-size: 13px;"
                       "}"
                       "QDateTimeEdit:focus { border-color: #86b7fe; }"
                       "QDateTimeEdit::drop-down {"
                       "  subcontrol-origin: padding;"
                       "  subcontrol-position: top right;"
                       "  width: 24px;"
                       "  border-left: 1px solid #ced4da;"
                       "}"
                       "QDateTimeEdit::down-arrow { width: 10px; height: 10px; }"
                       "QDateTimeEdit QAbstractSpinBox::up-button,"
                       "QDateTimeEdit QAbstractSpinBox::down-button {"
                       "  border: none;"
                       "  background: #f1f3f5;"
                       "  width: 22px;"
                       "}"));
    connect(m_datetimeInputEdit, &QDateTimeEdit::dateTimeChanged,
            this, &MainWindow::onDatetimeInputChanged);

    QHBoxLayout *dtSecRow = new QHBoxLayout();
    dtSecRow->setSpacing(8);
    QLabel *dtSecLabel = new QLabel(QStringLiteral("秒级:"), dtToTsGroup);
    dtSecLabel->setFixedWidth(60);
    m_datetimeSecResultEdit = new QLineEdit(dtToTsGroup);
    m_datetimeSecResultEdit->setReadOnly(true);
    m_datetimeSecResultEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    m_datetimeSecCopyBtn = new QPushButton(QStringLiteral("复制"), dtToTsGroup);
    m_datetimeSecCopyBtn->setFixedHeight(30);
    m_datetimeSecCopyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_datetimeSecCopyBtn, &QPushButton::clicked,
            this, &MainWindow::onDatetimeSecCopy);
    dtSecRow->addWidget(dtSecLabel);
    dtSecRow->addWidget(m_datetimeSecResultEdit, 1);
    dtSecRow->addWidget(m_datetimeSecCopyBtn);

    QHBoxLayout *dtMsRow = new QHBoxLayout();
    dtMsRow->setSpacing(8);
    QLabel *dtMsLabel = new QLabel(QStringLiteral("毫秒级:"), dtToTsGroup);
    dtMsLabel->setFixedWidth(60);
    m_datetimeMsResultEdit = new QLineEdit(dtToTsGroup);
    m_datetimeMsResultEdit->setReadOnly(true);
    m_datetimeMsResultEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    m_datetimeMsCopyBtn = new QPushButton(QStringLiteral("复制"), dtToTsGroup);
    m_datetimeMsCopyBtn->setFixedHeight(30);
    m_datetimeMsCopyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_datetimeMsCopyBtn, &QPushButton::clicked,
            this, &MainWindow::onDatetimeMsCopy);
    dtMsRow->addWidget(dtMsLabel);
    dtMsRow->addWidget(m_datetimeMsResultEdit, 1);
    dtMsRow->addWidget(m_datetimeMsCopyBtn);

    dtToTsLayout->addWidget(m_datetimeInputEdit);
    dtToTsLayout->addLayout(dtSecRow);
    dtToTsLayout->addLayout(dtMsRow);

    mainLayout->addWidget(nowGroup);
    mainLayout->addWidget(tsToDtGroup);
    mainLayout->addWidget(dtToTsGroup);
    mainLayout->addStretch();

    m_timestampTimer = new QTimer(this);
    connect(m_timestampTimer, &QTimer::timeout, this, &MainWindow::onTimestampUpdate);
    m_timestampTimer->start(200);

    onTimestampUpdate();
    onDatetimeInputChanged();

    return page;
}

QWidget *MainWindow::createCronPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *exprGroup = new QGroupBox(QStringLiteral("Cron 表达式"), page);
    QVBoxLayout *exprLayout = new QVBoxLayout(exprGroup);
    exprLayout->setSpacing(10);

    QHBoxLayout *inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);

    m_cronInputEdit = new QLineEdit(exprGroup);
    m_cronInputEdit->setPlaceholderText(QStringLiteral("分 时 日 月 周，如: */15 8-18 * * 1-5"));
    m_cronInputEdit->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace;"));
    connect(m_cronInputEdit, &QLineEdit::textChanged,
            this, &MainWindow::onCronInputChanged);

    m_cronPresetCombo = new QComboBox(exprGroup);
    m_cronPresetCombo->setFixedWidth(130);
    m_cronPresetCombo->addItem(QStringLiteral("自定义"));
    m_cronPresetCombo->addItem(QStringLiteral("每1分钟"));
    m_cronPresetCombo->addItem(QStringLiteral("每5分钟"));
    m_cronPresetCombo->addItem(QStringLiteral("每15分钟"));
    m_cronPresetCombo->addItem(QStringLiteral("每30分钟"));
    m_cronPresetCombo->addItem(QStringLiteral("每小时"));
    m_cronPresetCombo->addItem(QStringLiteral("每天零点"));
    m_cronPresetCombo->addItem(QStringLiteral("每周一零点"));
    m_cronPresetCombo->addItem(QStringLiteral("每月1号零点"));
    m_cronPresetCombo->addItem(QStringLiteral("工作日每小时"));
    connect(m_cronPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCronPresetChanged);

    inputRow->addWidget(m_cronInputEdit, 1);
    inputRow->addWidget(m_cronPresetCombo);

    m_cronErrorLabel = new QLabel(exprGroup);
    m_cronErrorLabel->setStyleSheet(
        QStringLiteral("color: #dc3545; font-size: 12px; font-weight: bold;"));
    m_cronErrorLabel->setVisible(false);

    exprLayout->addLayout(inputRow);
    exprLayout->addWidget(m_cronErrorLabel);

    QGroupBox *fieldGroup = new QGroupBox(QStringLiteral("字段解析"), page);
    QGridLayout *fieldGrid = new QGridLayout(fieldGroup);
    fieldGrid->setSpacing(6);
    fieldGrid->setColumnStretch(1, 1);

    auto addFieldRow = [&](int row, const QString &name, QLabel *&valueLabel) {
        QLabel *nameLabel = new QLabel(name, fieldGroup);
        nameLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #495057;"));
        valueLabel = new QLabel(QStringLiteral("-"), fieldGroup);
        valueLabel->setStyleSheet(
            QStringLiteral("font-family: 'Consolas', 'Courier New', monospace; "
                           "color: #212529;"));
        fieldGrid->addWidget(nameLabel, row, 0);
        fieldGrid->addWidget(valueLabel, row, 1);
    };

    QLabel *descTitle = new QLabel(QStringLiteral("描述:"), fieldGroup);
    descTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #495057;"));
    m_cronDescLabel = new QLabel(QStringLiteral("-"), fieldGroup);
    m_cronDescLabel->setStyleSheet(
        QStringLiteral("color: #0d6efd; font-size: 13px;"));
    m_cronDescLabel->setWordWrap(true);
    fieldGrid->addWidget(descTitle, 5, 0);
    fieldGrid->addWidget(m_cronDescLabel, 5, 1);

    addFieldRow(0, QStringLiteral("分钟:"), m_cronMinField);
    addFieldRow(1, QStringLiteral("小时:"), m_cronHourField);
    addFieldRow(2, QStringLiteral("日期:"), m_cronDomField);
    addFieldRow(3, QStringLiteral("月份:"), m_cronMonthField);
    addFieldRow(4, QStringLiteral("星期:"), m_cronDowField);

    QGroupBox *nextGroup = new QGroupBox(QStringLiteral("未来执行时间"), page);
    QVBoxLayout *nextLayout = new QVBoxLayout(nextGroup);
    nextLayout->setSpacing(8);

    QHBoxLayout *nextTopRow = new QHBoxLayout();
    nextTopRow->setSpacing(4);
    QLabel *showLabel = new QLabel(QStringLiteral("显示最近"), nextGroup);
    m_cronCountCombo = new QComboBox(nextGroup);
    m_cronCountCombo->setFixedWidth(70);
    m_cronCountCombo->addItems({QStringLiteral("5"), QStringLiteral("10"),
                                QStringLiteral("20"), QStringLiteral("50")});
    m_cronCountCombo->setCurrentIndex(1);
    connect(m_cronCountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCronCountChanged);
    QLabel *timesLabel = new QLabel(QStringLiteral("次执行"), nextGroup);
    nextTopRow->addWidget(showLabel);
    nextTopRow->addWidget(m_cronCountCombo);
    nextTopRow->addWidget(timesLabel);
    nextTopRow->addStretch();

    m_cronNextTable = new QTableWidget(0, 3, nextGroup);
    m_cronNextTable->setMinimumHeight(180);
    m_cronNextTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cronNextTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_cronNextTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cronNextTable->setFocusPolicy(Qt::NoFocus);
    m_cronNextTable->verticalHeader()->setVisible(false);
    m_cronNextTable->horizontalHeader()->setStretchLastSection(true);
    m_cronNextTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_cronNextTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_cronNextTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    QStringList headers;
    headers << QStringLiteral("序号") << QStringLiteral("执行时间") << QStringLiteral("相对时间");
    m_cronNextTable->setHorizontalHeaderLabels(headers);

    m_cronNextTable->setStyleSheet(QStringLiteral(
        "QTableWidget { font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; }"
        "QHeaderView::section { background-color: #f1f3f5; color: #495057; font-weight: bold; "
        "  padding: 4px 8px; border: none; border-bottom: 2px solid #dee2e6; font-size: 12px; }"));

    m_cronCopyBtn = new QPushButton(QStringLiteral("复制全部"), nextGroup);
    m_cronCopyBtn->setFixedHeight(32);
    m_cronCopyBtn->setFixedWidth(100);
    m_cronCopyBtn->setCursor(Qt::PointingHandCursor);
    m_cronCopyBtn->setEnabled(false);
    connect(m_cronCopyBtn, &QPushButton::clicked, this, &MainWindow::onCronCopyAll);

    nextLayout->addLayout(nextTopRow);
    nextLayout->addWidget(m_cronNextTable, 1);
    nextLayout->addWidget(m_cronCopyBtn, 0, Qt::AlignLeft);

    mainLayout->addWidget(exprGroup);
    mainLayout->addWidget(fieldGroup);
    mainLayout->addWidget(nextGroup, 1);

    m_cronTimer = new QTimer(this);
    connect(m_cronTimer, &QTimer::timeout, this, &MainWindow::onCronUpdateTimes);
    m_cronTimer->start(30000);

    return page;
}

QWidget *MainWindow::createJwtPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *inputGroup = new QGroupBox(QStringLiteral("JWT 输入"), page);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(12);

    m_jwtInputEdit = new QTextEdit(inputGroup);
    m_jwtInputEdit->setPlaceholderText(QStringLiteral("请输入或粘贴 JWT 字符串 (header.payload.signature)..."));
    m_jwtInputEdit->setMaximumHeight(100);
    m_jwtInputEdit->setAcceptRichText(false);
    m_jwtInputEdit->setStyleSheet(QStringLiteral(
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

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_jwtParseBtn = new QPushButton(QStringLiteral("解析"), inputGroup);
    m_jwtParseBtn->setFixedHeight(34);
    m_jwtParseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_jwtParseBtn, &QPushButton::clicked, this, &MainWindow::onJwtParse);

    m_jwtClearBtn = new QPushButton(QStringLiteral("清除"), inputGroup);
    m_jwtClearBtn->setFixedHeight(34);
    m_jwtClearBtn->setCursor(Qt::PointingHandCursor);
    m_jwtClearBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(m_jwtClearBtn, &QPushButton::clicked, this, &MainWindow::onJwtClear);

    btnRow->addStretch();
    btnRow->addWidget(m_jwtParseBtn);
    btnRow->addWidget(m_jwtClearBtn);

    inputLayout->addWidget(m_jwtInputEdit);
    inputLayout->addLayout(btnRow);

    QGroupBox *resultGroup = new QGroupBox(QStringLiteral("解析结果"), page);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setSpacing(10);

    m_jwtResultTabs = new QTabWidget(resultGroup);
    m_jwtResultTabs->setStyleSheet(QStringLiteral(
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

    auto createResultTab = [](QTabWidget *parent) -> QTextEdit * {
        QTextEdit *edit = new QTextEdit(parent);
        edit->setReadOnly(true);
        edit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px;"
            "  border: none;"
            "  padding: 10px;"
            "  background-color: #ffffff;"
            "  color: #212529;"
            "}"));
        return edit;
    };

    m_jwtHeaderEdit = createResultTab(m_jwtResultTabs);
    m_jwtPayloadEdit = createResultTab(m_jwtResultTabs);
    m_jwtSignatureEdit = createResultTab(m_jwtResultTabs);

    m_jwtResultTabs->addTab(m_jwtHeaderEdit, QStringLiteral("Header"));
    m_jwtResultTabs->addTab(m_jwtPayloadEdit, QStringLiteral("Payload"));
    m_jwtResultTabs->addTab(m_jwtSignatureEdit, QStringLiteral("Signature"));

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(16);

    m_jwtInfoLabel = new QLabel(resultGroup);
    m_jwtInfoLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));
    m_jwtInfoLabel->setWordWrap(true);

    m_jwtCopyCurrentBtn = new QPushButton(QStringLiteral("复制当前"), resultGroup);
    m_jwtCopyCurrentBtn->setFixedHeight(34);
    m_jwtCopyCurrentBtn->setCursor(Qt::PointingHandCursor);
    m_jwtCopyCurrentBtn->setEnabled(false);
    connect(m_jwtCopyCurrentBtn, &QPushButton::clicked, this, &MainWindow::onJwtCopyCurrent);

    m_jwtCopyAllBtn = new QPushButton(QStringLiteral("复制全部"), resultGroup);
    m_jwtCopyAllBtn->setFixedHeight(34);
    m_jwtCopyAllBtn->setCursor(Qt::PointingHandCursor);
    m_jwtCopyAllBtn->setEnabled(false);
    connect(m_jwtCopyAllBtn, &QPushButton::clicked, this, &MainWindow::onJwtCopyAll);

    bottomRow->addWidget(m_jwtInfoLabel, 1);
    bottomRow->addWidget(m_jwtCopyCurrentBtn);
    bottomRow->addWidget(m_jwtCopyAllBtn);

    resultLayout->addWidget(m_jwtResultTabs, 1);
    resultLayout->addLayout(bottomRow);

    mainLayout->addWidget(inputGroup);
    mainLayout->addWidget(resultGroup, 1);

    return page;
}

static QByteArray jwtBase64UrlDecode(const QByteArray &input)
{
    QByteArray data = input;
    data.replace('-', '+');
    data.replace('_', '/');
    while (data.size() % 4 != 0)
        data.append('=');
    return QByteArray::fromBase64(data);
}

void MainWindow::onJwtParse()
{
    QString input = m_jwtInputEdit->toPlainText().trimmed();
    if (input.isEmpty())
        return;

    m_jwtHeaderEdit->clear();
    m_jwtPayloadEdit->clear();
    m_jwtSignatureEdit->clear();
    m_jwtInfoLabel->clear();

    QStringList parts = input.split('.');
    if (parts.size() != 3) {
        m_jwtHeaderEdit->setPlainText(QStringLiteral("不是有效的 JWT 格式\n\nJWT 应包含三部分，以点号分隔：header.payload.signature"));
        m_jwtCopyCurrentBtn->setEnabled(false);
        m_jwtCopyAllBtn->setEnabled(false);
        return;
    }

    {
        QByteArray decoded = jwtBase64UrlDecode(parts[0].toUtf8());
        if (decoded.isEmpty() && !parts[0].isEmpty()) {
            m_jwtHeaderEdit->setPlainText(QStringLiteral("无法解码 Header (Base64url 解码失败)"));
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(decoded);
            if (doc.isObject()) {
                m_jwtHeaderEdit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
            } else {
                m_jwtHeaderEdit->setPlainText(QString::fromUtf8(decoded) + QStringLiteral("\n\n[警告: 不是有效的 JSON]"));
            }
        }
    }

    {
        QByteArray decoded = jwtBase64UrlDecode(parts[1].toUtf8());
        if (decoded.isEmpty() && !parts[1].isEmpty()) {
            m_jwtPayloadEdit->setPlainText(QStringLiteral("无法解码 Payload (Base64url 解码失败)"));
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(decoded);
            if (doc.isObject()) {
                QString payloadStr = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

                QJsonObject obj = doc.object();
                QStringList timeInfo;
                auto addTimeField = [&](const QString &key, const QString &label) {
                    if (obj.contains(key)) {
                        QJsonValue val = obj.value(key);
                        if (val.isDouble()) {
                            qint64 ts = static_cast<qint64>(val.toDouble());
                            QDateTime dt;
                            if (ts > 1000000000000)
                                dt = QDateTime::fromMSecsSinceEpoch(ts);
                            else
                                dt = QDateTime::fromSecsSinceEpoch(ts);
                            if (dt.isValid()) {
                                timeInfo.append(QStringLiteral("%1: %2").arg(label, dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
                            }
                        }
                    }
                };
                addTimeField(QStringLiteral("iat"), QStringLiteral("签发时间 (iat)"));
                addTimeField(QStringLiteral("exp"), QStringLiteral("过期时间 (exp)"));
                addTimeField(QStringLiteral("nbf"), QStringLiteral("生效时间 (nbf)"));

                if (!timeInfo.isEmpty()) {
                    payloadStr += QStringLiteral("\n\n--- 时间戳转换 ---\n") + timeInfo.join('\n');
                }

                m_jwtPayloadEdit->setPlainText(payloadStr);
            } else {
                m_jwtPayloadEdit->setPlainText(QString::fromUtf8(decoded) + QStringLiteral("\n\n[警告: 不是有效的 JSON]"));
            }
        }
    }

    m_jwtSignatureEdit->setPlainText(parts[2]);

    m_jwtResultTabs->setCurrentIndex(1);
    m_jwtCopyCurrentBtn->setEnabled(true);
    m_jwtCopyAllBtn->setEnabled(true);
}

void MainWindow::onJwtClear()
{
    m_jwtInputEdit->clear();
    m_jwtHeaderEdit->clear();
    m_jwtPayloadEdit->clear();
    m_jwtSignatureEdit->clear();
    m_jwtInfoLabel->clear();
    m_jwtCopyCurrentBtn->setEnabled(false);
    m_jwtCopyAllBtn->setEnabled(false);
}

void MainWindow::onJwtCopyCurrent()
{
    QTextEdit *current = qobject_cast<QTextEdit *>(m_jwtResultTabs->currentWidget());
    if (!current)
        return;
    QString text = current->toPlainText();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_jwtCopyCurrentBtn->text();
    m_jwtCopyCurrentBtn->setText(QStringLiteral("已复制"));
    m_jwtCopyCurrentBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_jwtCopyCurrentBtn->setText(original);
        m_jwtCopyCurrentBtn->setEnabled(true);
    });
}

void MainWindow::onJwtCopyAll()
{
    QString text = QStringLiteral("=== Header ===\n%1\n\n=== Payload ===\n%2\n\n=== Signature ===\n%3")
        .arg(m_jwtHeaderEdit->toPlainText(), m_jwtPayloadEdit->toPlainText(), m_jwtSignatureEdit->toPlainText());
    QApplication::clipboard()->setText(text);
    QString original = m_jwtCopyAllBtn->text();
    m_jwtCopyAllBtn->setText(QStringLiteral("已复制"));
    m_jwtCopyAllBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_jwtCopyAllBtn->setText(original);
        m_jwtCopyAllBtn->setEnabled(true);
    });
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
        "<h3>Zane Tool v0.1.0</h3>"
        "<p>集成 ffmpeg 与 aria2c 的桌面端效率工具箱。</p>"
        "<p><b>媒体工具</b><br>"
        "图片/视频/音频批量处理：压缩、缩放、格式转换</p>"
        "<p><b>系统工具</b><br>"
        "屏幕取色 &middot; 截图贴图 &middot; 窗口透明 &middot; 秒表计时</p>"
        "<p><b>开发工具</b><br>"
        "图片转Base64 &middot; 时间戳转换 &middot; Cron 解析 &middot; JWT 解析 &middot; 随机字符串</p>"
        "<p><b>网络工具</b><br>"
        "批量文件下载（aria2c）</p>"
        "<p><b>技术栈</b><br>"
        "Qt 6 (Widgets) &middot; C++17 &middot; ffmpeg &middot; aria2c<br>"
        "MinGW GCC 13.1 &middot; CMake 3.16+</p>"
        "<p><b>作者:</b> Zane</p>"
    );
    QMessageBox::about(this, QStringLiteral("关于"), msg);
}

void MainWindow::showBatchSummary(const QString &type, int total, int success, int failed,
                                  qint64 sizeBefore, qint64 sizeAfter, const QStringList &failedFiles)
{
    QElapsedTimer totalTimer;
    totalTimer.invalidate();

    Q_UNUSED(totalTimer)

    QString msg;
    msg += QStringLiteral("<h3>%1 批量处理完成</h3>").arg(type);
    msg += QStringLiteral("<table>");
    msg += QStringLiteral("<tr><td>总数:</td><td><b>%1</b></td></tr>").arg(total);
    msg += QStringLiteral("<tr><td>成功:</td><td style='color:green'><b>%1</b></td></tr>").arg(success);
    msg += QStringLiteral("<tr><td>失败:</td><td style='color:red'><b>%1</b></td></tr>").arg(failed);
    msg += QStringLiteral("<tr><td>原始大小:</td><td>%1</td></tr>").arg(Utils::formatFileSize(sizeBefore));
    msg += QStringLiteral("<tr><td>压缩后:</td><td>%1</td></tr>").arg(Utils::formatFileSize(sizeAfter));
    if (sizeBefore > 0) {
        double ratio = 100.0 * (1.0 - static_cast<double>(sizeAfter) / static_cast<double>(sizeBefore));
        msg += QStringLiteral("<tr><td>压缩率:</td><td>%1%</td></tr>").arg(QString::number(ratio, 'f', 1));
    }
    msg += QStringLiteral("</table>");

    if (!failedFiles.isEmpty()) {
        msg += QStringLiteral("<br><b>失败文件:</b><br>");
        for (const QString &f : failedFiles) {
            msg += f + QStringLiteral("<br>");
        }
    }

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("处理完成"));
    box.setTextFormat(Qt::RichText);
    box.setText(msg);
    box.exec();
}

// ==================== Helpers ====================

void MainWindow::setImageUiEnabled(bool enabled)
{
    m_imageAddBtn->setEnabled(enabled);
    m_imageRemoveBtn->setEnabled(enabled);
    m_imageClearBtn->setEnabled(enabled);
    m_imageQualitySlider->setEnabled(enabled);
    m_imageScaleCheck->setEnabled(enabled);
    m_imageScaleWidth->setEnabled(enabled && m_imageScaleCheck->isChecked());
    m_imageFormatCombo->setEnabled(enabled);
    m_imageOutputDir->setEnabled(enabled);
    m_imageOutputBrowse->setEnabled(enabled);
    m_imageStartBtn->setEnabled(enabled);
    m_imageCancelBtn->setEnabled(!enabled);

    if (enabled) {
        m_imageProgressBar->setValue(100);
    }
}

void MainWindow::setVideoUiEnabled(bool enabled)
{
    m_videoAddBtn->setEnabled(enabled);
    m_videoRemoveBtn->setEnabled(enabled);
    m_videoClearBtn->setEnabled(enabled);
    m_videoFormatCombo->setEnabled(enabled);
    m_videoCrfSlider->setEnabled(enabled);
    m_videoScaleCheck->setEnabled(enabled);
    m_videoPresetRes->setEnabled(enabled && m_videoScaleCheck->isChecked());
    m_videoOutputDir->setEnabled(enabled);
    m_videoOutputBrowse->setEnabled(enabled);
    m_videoStartBtn->setEnabled(enabled);
    m_videoCancelBtn->setEnabled(!enabled);

    if (enabled) {
        m_videoProgressBar->setValue(100);
    }
}

void MainWindow::setAudioUiEnabled(bool enabled)
{
    m_audioAddBtn->setEnabled(enabled);
    m_audioRemoveBtn->setEnabled(enabled);
    m_audioClearBtn->setEnabled(enabled);
    m_audioFormatCombo->setEnabled(enabled);
    m_audioBitrateCombo->setEnabled(enabled);
    m_audioSampleRateCombo->setEnabled(enabled);
    m_audioChannelsCombo->setEnabled(enabled);
    m_audioOutputDir->setEnabled(enabled);
    m_audioOutputBrowse->setEnabled(enabled);
    m_audioStartBtn->setEnabled(enabled);
    m_audioCancelBtn->setEnabled(!enabled);

    if (enabled) {
        m_audioProgressBar->setValue(100);
    }
}

void MainWindow::updateImageResolutionPreview()
{
    if (m_imageFileList->selectedItems().size() != 1) {
        m_imageResPreview->setVisible(false);
        return;
    }
    m_imageResPreview->setVisible(false);
}

void MainWindow::updateVideoInfoPreview()
{
    if (m_videoFileList->selectedItems().size() != 1) {
        m_videoInfoPreview->setVisible(false);
        return;
    }
    m_videoInfoPreview->setVisible(false);
}

void MainWindow::updateAudioInfoPreview()
{
    if (m_audioFileList->selectedItems().size() != 1) {
        m_audioInfoPreview->setVisible(false);
        return;
    }
    m_audioInfoPreview->setVisible(false);
}

// ==================== Timer Tab ====================

void MainWindow::onTimerStart()
{
    if (m_stopwatch->state() == StopwatchTimer::Paused) {
        m_stopwatch->start();
    } else {
        m_stopwatch->start();
    }
}

void MainWindow::onTimerPause()
{
    m_stopwatch->pause();
}

void MainWindow::onTimerStop()
{
    m_stopwatch->stop();
}

void MainWindow::onTimerLap()
{
    m_stopwatch->lap();
}

void MainWindow::onTimerClearLaps()
{
    m_stopwatch->reset();
    m_timerLapList->setRowCount(0);
}

void MainWindow::onTimerTick(const QString &formatted)
{
    m_timerDisplay->setText(formatted);
}

void MainWindow::onTimerStateChanged(StopwatchTimer::State state)
{
    bool running = (state == StopwatchTimer::Running);
    bool idle = (state == StopwatchTimer::Idle);
    bool paused = (state == StopwatchTimer::Paused);

    m_timerStartBtn->setEnabled(!running);
    m_timerStartBtn->setText(paused ? QStringLiteral("继续") : QStringLiteral("开始"));
    m_timerPauseBtn->setEnabled(running);
    m_timerStopBtn->setEnabled(!idle);
    m_timerLapBtn->setEnabled(running);
    m_timerClearBtn->setEnabled(!running);
}

void MainWindow::onTimerLapRecorded(const StopwatchTimer::LapEntry &entry)
{
    int row = m_timerLapList->rowCount();
    m_timerLapList->insertRow(row);

    QTableWidgetItem *indexItem = new QTableWidgetItem(
        QStringLiteral("#%1").arg(entry.index, 2));
    indexItem->setTextAlignment(Qt::AlignCenter);

    QTableWidgetItem *lapItem = new QTableWidgetItem(
        StopwatchTimer::formatTime(entry.lapMs));

    QTableWidgetItem *totalItem = new QTableWidgetItem(
        StopwatchTimer::formatTime(entry.totalMs));

    m_timerLapList->setItem(row, 0, indexItem);
    m_timerLapList->setItem(row, 1, lapItem);
    m_timerLapList->setItem(row, 2, totalItem);
    m_timerLapList->scrollToBottom();
}

// ==================== Cron helpers ====================

namespace {

struct CronFields {
    std::set<int> minutes;
    std::set<int> hours;
    std::set<int> doms;
    std::set<int> months;
    std::set<int> dows;
    bool valid = false;
    QString error;
};

std::set<int> parseCronField(const QString &field, int minVal, int maxVal, bool &ok)
{
    std::set<int> result;
    ok = true;

    if (field.isEmpty()) {
        ok = false;
        return result;
    }

    if (field == QStringLiteral("*")) {
        return result;
    }

    const QStringList parts = field.split(',');
    for (const QString &part : parts) {
        QString p = part.trimmed();
        if (p.isEmpty()) { ok = false; return {}; }

        int step = 1;
        int slashPos = p.indexOf('/');
        if (slashPos != -1) {
            bool stepOk = false;
            step = p.mid(slashPos + 1).toInt(&stepOk);
            if (!stepOk || step < 1) { ok = false; return {}; }
            p = p.left(slashPos);
        }

        if (p == QStringLiteral("*")) {
            for (int i = minVal; i <= maxVal; i += step) {
                result.insert(i);
            }
        } else if (p.contains('-')) {
            QStringList range = p.split('-');
            if (range.size() != 2) { ok = false; return {}; }
            bool ok1 = false, ok2 = false;
            int start = range[0].toInt(&ok1);
            int end = range[1].toInt(&ok2);
            if (!ok1 || !ok2 || start < minVal || end > maxVal || start > end) {
                ok = false;
                return {};
            }
            for (int i = start; i <= end; i += step) {
                result.insert(i);
            }
        } else {
            bool valOk = false;
            int val = p.toInt(&valOk);
            if (!valOk || val < minVal || val > maxVal) { ok = false; return {}; }
            result.insert(val);
        }
    }

    return result;
}

CronFields parseCron(const QString &expr)
{
    CronFields cf;
    QString trimmed = expr.trimmed();

    if (trimmed == QStringLiteral("@yearly") || trimmed == QStringLiteral("@annually")) {
        trimmed = QStringLiteral("0 0 1 1 *");
    } else if (trimmed == QStringLiteral("@monthly")) {
        trimmed = QStringLiteral("0 0 1 * *");
    } else if (trimmed == QStringLiteral("@weekly")) {
        trimmed = QStringLiteral("0 0 * * 0");
    } else if (trimmed == QStringLiteral("@daily") || trimmed == QStringLiteral("@midnight")) {
        trimmed = QStringLiteral("0 0 * * *");
    } else if (trimmed == QStringLiteral("@hourly")) {
        trimmed = QStringLiteral("0 * * * *");
    }

    QStringList fields = trimmed.split(' ', Qt::SkipEmptyParts);
    if (fields.size() != 5) {
        cf.error = QStringLiteral("需要5个字段（分 时 日 月 周），当前 %1 个字段").arg(fields.size());
        return cf;
    }

    bool ok = false;
    cf.minutes = parseCronField(fields[0], 0, 59, ok);
    if (!ok) { cf.error = QStringLiteral("分钟字段无效: ") + fields[0]; return cf; }

    cf.hours = parseCronField(fields[1], 0, 23, ok);
    if (!ok) { cf.error = QStringLiteral("小时字段无效: ") + fields[1]; return cf; }

    cf.doms = parseCronField(fields[2], 1, 31, ok);
    if (!ok) { cf.error = QStringLiteral("日期字段无效: ") + fields[2]; return cf; }

    cf.months = parseCronField(fields[3], 1, 12, ok);
    if (!ok) { cf.error = QStringLiteral("月份字段无效: ") + fields[3]; return cf; }

    cf.dows = parseCronField(fields[4], 0, 6, ok);
    if (!ok) { cf.error = QStringLiteral("星期字段无效: ") + fields[4]; return cf; }

    cf.valid = true;
    return cf;
}

int firstVal(const std::set<int> &s, int defVal)
{
    return s.empty() ? defVal : *s.begin();
}

int nextGe(const std::set<int> &s, int val)
{
    auto it = s.lower_bound(val);
    return (it == s.end()) ? -1 : *it;
}

QVector<QDateTime> computeNextTimes(const CronFields &cf, int count, const QDateTime &from)
{
    QVector<QDateTime> result;
    if (count <= 0) return result;

    QDateTime t(from.date(), QTime(from.time().hour(), from.time().minute()));
    t = t.addSecs(60);

    const int maxIter = 525600 * 10;

    for (int iter = 0; iter < maxIter && result.size() < count; ++iter) {
        int minute = t.time().minute();
        int hour = t.time().hour();
        int day = t.date().day();
        int month = t.date().month();
        int dow = t.date().dayOfWeek() % 7;

        if (!cf.minutes.empty()) {
            int next = nextGe(cf.minutes, minute);
            if (next == -1) {
                t = t.addSecs((60 - minute) * 60);
                t.setTime(QTime(t.time().hour(), firstVal(cf.minutes, 0), 0));
                continue;
            }
            if (next != minute) {
                t.setTime(QTime(hour, next, 0));
                continue;
            }
        }

        if (!cf.hours.empty()) {
            int next = nextGe(cf.hours, hour);
            if (next == -1) {
                int fm = firstVal(cf.minutes, 0);
                int fh = firstVal(cf.hours, 0);
                t = t.addDays(1);
                t.setTime(QTime(fh, fm, 0));
                continue;
            }
            if (next != hour) {
                int fm = firstVal(cf.minutes, 0);
                t.setTime(QTime(next, fm, 0));
                continue;
            }
        }

        bool domR = !cf.doms.empty();
        bool dowR = !cf.dows.empty();
        bool dayOk = false;
        if (!domR && !dowR) {
            dayOk = true;
        } else if (domR && dowR) {
            dayOk = cf.doms.find(day) != cf.doms.end()
                 || cf.dows.find(dow) != cf.dows.end();
        } else if (domR) {
            dayOk = cf.doms.find(day) != cf.doms.end();
        } else {
            dayOk = cf.dows.find(dow) != cf.dows.end();
        }

        if (!dayOk) {
            int fm = firstVal(cf.minutes, 0);
            int fh = firstVal(cf.hours, 0);
            t = t.addDays(1);
            t.setTime(QTime(fh, fm, 0));
            continue;
        }

        if (!cf.months.empty()) {
            int next = nextGe(cf.months, month);
            if (next == -1) {
                int fm = firstVal(cf.minutes, 0);
                int fh = firstVal(cf.hours, 0);
                int fmon = firstVal(cf.months, 1);
                t.setDate(QDate(t.date().year() + 1, fmon, 1));
                t.setTime(QTime(fh, fm, 0));
                continue;
            }
            if (next != month) {
                int fm = firstVal(cf.minutes, 0);
                int fh = firstVal(cf.hours, 0);
                t.setDate(QDate(t.date().year(), next, 1));
                t.setTime(QTime(fh, fm, 0));
                continue;
            }
        }

        result.append(t);
        t = t.addSecs(60);
    }

    return result;
}

QString describeFieldValues(const std::set<int> &values, int minVal, int maxVal,
                            const QString &unitSingular)
{
    if (values.empty())
        return QStringLiteral("每%1").arg(unitSingular);

    if (values.size() == 1)
        return QString::number(*values.begin());

    if (values.size() >= 2) {
        auto it = values.begin();
        int first = *it;
        ++it;
        int second = *it;
        int step = second - first;
        bool isUniform = (step > 0);
        int prev = second;
        ++it;
        while (it != values.end() && isUniform) {
            if (*it - prev != step) isUniform = false;
            prev = *it;
            ++it;
        }
        if (isUniform && first == minVal) {
            return QStringLiteral("每%1%2").arg(step).arg(unitSingular);
        }
    }

    int first = *values.begin();
    int last = *values.rbegin();
    bool isRange = true;
    int expected = first;
    for (int v : values) {
        if (v != expected) { isRange = false; break; }
        expected++;
    }
    if (isRange && first == minVal && last == maxVal)
        return QStringLiteral("每%1").arg(unitSingular);
    if (isRange)
        return QStringLiteral("%1-%2").arg(first).arg(last);

    QStringList parts;
    for (int v : values)
        parts.append(QString::number(v));
    return parts.join(',');
}

QString describeDow(const std::set<int> &values)
{
    if (values.empty()) return QStringLiteral("每天");

    static const QString dowNames[] = {
        QStringLiteral("日"), QStringLiteral("一"), QStringLiteral("二"),
        QStringLiteral("三"), QStringLiteral("四"), QStringLiteral("五"),
        QStringLiteral("六")
    };
    QStringList parts;
    for (int v : values)
        parts.append(QStringLiteral("周%1").arg(dowNames[v]));

    if (parts.size() == 7) return QStringLiteral("每天");
    if (parts.size() == 2 && values.find(0) != values.end()
        && values.find(6) != values.end())
        return QStringLiteral("周末");
    if (values.find(0) == values.end() && values.find(6) == values.end()
        && values.size() == 5)
        return QStringLiteral("工作日");

    return parts.join(QStringLiteral(", "));
}

QString describeDom(const std::set<int> &values)
{
    if (values.empty()) return QStringLiteral("每天");
    if (values.size() == 1) return QStringLiteral("%1号").arg(*values.begin());
    QStringList parts;
    for (int v : values)
        parts.append(QStringLiteral("%1号").arg(v));
    return parts.join(QStringLiteral(", "));
}

QString describeMonth(const std::set<int> &values)
{
    if (values.empty()) return QStringLiteral("每月");
    if (values.size() == 1) return QStringLiteral("%1月").arg(*values.begin());
    QStringList parts;
    for (int v : values)
        parts.append(QStringLiteral("%1月").arg(v));
    return parts.join(QStringLiteral(", "));
}

QString relativeTimeDesc(const QDateTime &target, const QDateTime &now)
{
    qint64 secs = now.secsTo(target);
    if (secs < 0) return QStringLiteral("已过期");
    if (secs < 60) return QStringLiteral("不到 1 分钟");
    if (secs < 3600) return QStringLiteral("约 %1 分钟后").arg(secs / 60);
    if (secs < 86400) return QStringLiteral("约 %1 小时 %2 分钟后")
                            .arg(secs / 3600).arg((secs % 3600) / 60);
    if (secs < 2592000) return QStringLiteral("约 %1 天后").arg(secs / 86400);
    if (secs < 31536000) return QStringLiteral("约 %1 个月后").arg(secs / 2592000);
    return QStringLiteral("约 %1 年后").arg(secs / 31536000);
}

} // anonymous namespace

// ==================== Cron Tab ====================

void MainWindow::onCronInputChanged()
{
    const QString text = m_cronInputEdit->text().trimmed();

    if (text.isEmpty()) {
        m_cronErrorLabel->setVisible(false);
        m_cronMinField->setText(QStringLiteral("-"));
        m_cronHourField->setText(QStringLiteral("-"));
        m_cronDomField->setText(QStringLiteral("-"));
        m_cronMonthField->setText(QStringLiteral("-"));
        m_cronDowField->setText(QStringLiteral("-"));
        m_cronDescLabel->setText(QStringLiteral("-"));
        m_cronNextTable->setRowCount(0);
        m_cronCopyBtn->setEnabled(false);
        return;
    }

    CronFields cf = parseCron(text);

    if (!cf.valid) {
        m_cronErrorLabel->setText(cf.error);
        m_cronErrorLabel->setVisible(true);
        m_cronMinField->setText(QStringLiteral("-"));
        m_cronHourField->setText(QStringLiteral("-"));
        m_cronDomField->setText(QStringLiteral("-"));
        m_cronMonthField->setText(QStringLiteral("-"));
        m_cronDowField->setText(QStringLiteral("-"));
        m_cronDescLabel->setText(QStringLiteral("-"));
        m_cronNextTable->setRowCount(0);
        m_cronCopyBtn->setEnabled(false);
        return;
    }

    m_cronErrorLabel->setVisible(false);

    m_cronMinField->setText(
        describeFieldValues(cf.minutes, 0, 59, QStringLiteral("分钟")));
    m_cronHourField->setText(
        describeFieldValues(cf.hours, 0, 23, QStringLiteral("小时")));
    m_cronDomField->setText(describeDom(cf.doms));
    m_cronMonthField->setText(describeMonth(cf.months));
    m_cronDowField->setText(describeDow(cf.dows));

    QStringList descParts;
    if (!cf.dows.empty()) descParts.append(describeDow(cf.dows));
    if (!cf.doms.empty()) descParts.append(describeDom(cf.doms));
    if (!cf.months.empty() && cf.months.size() < 12)
        descParts.append(describeMonth(cf.months));
    QString timePart;
    if (!cf.hours.empty() && !cf.minutes.empty()) {
        timePart = describeFieldValues(cf.hours, 0, 23, QStringLiteral("小时"));
        if (!cf.minutes.empty())
            timePart += QStringLiteral(":") + describeFieldValues(cf.minutes, 0, 59,
                                                                   QStringLiteral("分钟"));
    } else if (!cf.hours.empty()) {
        timePart = describeFieldValues(cf.hours, 0, 23, QStringLiteral("小时"));
    } else if (!cf.minutes.empty()) {
        timePart = describeFieldValues(cf.minutes, 0, 59, QStringLiteral("分钟"));
    }
    if (!timePart.isEmpty()) descParts.append(timePart);
    m_cronDescLabel->setText(descParts.isEmpty() ? QStringLiteral("每分钟执行")
                                                  : descParts.join(QStringLiteral(", ")));

    onCronUpdateTimes();
}

void MainWindow::onCronPresetChanged(int index)
{
    if (index == 0) return;

    const char *presets[] = {
        "",
        "* * * * *",
        "*/5 * * * *",
        "*/15 * * * *",
        "*/30 * * * *",
        "0 * * * *",
        "0 0 * * *",
        "0 0 * * 1",
        "0 0 1 * *",
        "0 * * * 1-5",
    };
    if (index >= 1 && index <= 9) {
        m_cronInputEdit->setText(QString::fromLatin1(presets[index]));
    }
}

void MainWindow::onCronCountChanged(int /*index*/)
{
    onCronUpdateTimes();
}

void MainWindow::onCronCopyAll()
{
    QStringList lines;
    for (int row = 0; row < m_cronNextTable->rowCount(); ++row) {
        QTableWidgetItem *idxItem = m_cronNextTable->item(row, 0);
        QTableWidgetItem *timeItem = m_cronNextTable->item(row, 1);
        QTableWidgetItem *relItem = m_cronNextTable->item(row, 2);
        if (idxItem && timeItem && relItem) {
            lines.append(QStringLiteral("%1  %2  %3")
                             .arg(idxItem->text(), timeItem->text(), relItem->text()));
        }
    }

    if (lines.isEmpty()) return;

    QApplication::clipboard()->setText(lines.join('\n'));
    QString original = m_cronCopyBtn->text();
    m_cronCopyBtn->setText(QStringLiteral("已复制"));
    m_cronCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_cronCopyBtn->setText(original);
        m_cronCopyBtn->setEnabled(true);
    });
}

void MainWindow::onCronUpdateTimes()
{
    const QString text = m_cronInputEdit->text().trimmed();
    if (text.isEmpty()) {
        m_cronNextTable->setRowCount(0);
        m_cronCopyBtn->setEnabled(false);
        return;
    }

    CronFields cf = parseCron(text);
    if (!cf.valid) {
        m_cronNextTable->setRowCount(0);
        m_cronCopyBtn->setEnabled(false);
        return;
    }

    int count = m_cronCountCombo->currentText().toInt();
    QDateTime now = QDateTime::currentDateTime();
    QVector<QDateTime> times = computeNextTimes(cf, count, now);

    m_cronNextTable->setRowCount(0);
    m_cronNextTable->setRowCount(times.size());

    for (int i = 0; i < times.size(); ++i) {
        QTableWidgetItem *idxItem = new QTableWidgetItem(
            QStringLiteral("#%1").arg(i + 1, 2));
        idxItem->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *timeItem = new QTableWidgetItem(
            times[i].toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

        QTableWidgetItem *relItem = new QTableWidgetItem(
            relativeTimeDesc(times[i], now));

        m_cronNextTable->setItem(i, 0, idxItem);
        m_cronNextTable->setItem(i, 1, timeItem);
        m_cronNextTable->setItem(i, 2, relItem);
    }

    m_cronCopyBtn->setEnabled(times.size() > 0);
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
    m_randomOutput->setStyleSheet(
        QStringLiteral("font-family: 'Consolas', 'Courier New', monospace; font-size: 14px;"));
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
                processBase64File(url.toLocalFile());
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
        list = m_imageFileList;
    else if (pageIndex == 1)
        list = m_videoFileList;
    else if (pageIndex == 2)
        list = m_audioFileList;

    if (!list) return;

    for (const QUrl &url : mimeData->urls()) {
        if (url.isLocalFile())
            list->addItem(url.toLocalFile());
    }
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
        "  border: none;"
        "  border-top: 1px solid #dee2e6;"
        "  background-color: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  color: #0d6efd;"
        "  background-color: transparent;"
        "  border: 1px solid transparent;"
        "  border-top-left-radius: 6px;"
        "  border-top-right-radius: 6px;"
        "  padding: 8px 16px;"
        "  margin-right: 4px;"
        "  font-size: 14px;"
        "}"
        "QTabBar::tab:hover {"
        "  color: #0a58ca;"
        "  border-color: #e9ecef #e9ecef transparent;"
        "}"
        "QTabBar::tab:selected {"
        "  color: #495057;"
        "  background-color: #ffffff;"
        "  border: 1px solid #dee2e6;"
        "  border-bottom-color: #ffffff;"
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
    m_screenshotForQr = true;
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ShowWindow(hwnd, SW_HIDE);
    QTimer::singleShot(200, this, [this]() {
        m_screenshotPicker->begin();
    });
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
