#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QHeaderView>

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

MainWindow::MainWindow(const QString &ffmpegPath, QWidget *parent)
    : QMainWindow(parent)
    , m_ffmpegPath(ffmpegPath)
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

    m_stackedWidget->setCurrentIndex(0);

    m_aboutLabel = new QLabel(QStringLiteral("<a href='about' style='color:#6c757d;text-decoration:none;'>v0.1.0 &middot; Zane</a>"), this);
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
    addTool(QStringLiteral("计时器"), 6);

    addCategory(QStringLiteral("\U0001F527 开发工具"));
    addTool(QStringLiteral("图片转Base64"), 7);
    addTool(QStringLiteral("时间戳转换"), 8);
    addTool(QStringLiteral("定时任务"), 9);
    addTool(QStringLiteral("JWT 解析"), 10);

    addCategory(QStringLiteral("\U0001F310 网络工具"));
    addTool(QStringLiteral("批量下载"), 11);

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
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    QLabel *title = new QLabel(QStringLiteral("图片转Base64"), page);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #6c757d;"));
    QLabel *desc = new QLabel(QStringLiteral("功能开发中..."), page);
    desc->setStyleSheet(QStringLiteral("font-size: 14px; color: #adb5bd;"));
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(desc);
    return page;
}

QWidget *MainWindow::createTimestampPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    QLabel *title = new QLabel(QStringLiteral("时间戳转换"), page);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #6c757d;"));
    QLabel *desc = new QLabel(QStringLiteral("功能开发中..."), page);
    desc->setStyleSheet(QStringLiteral("font-size: 14px; color: #adb5bd;"));
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(desc);
    return page;
}

QWidget *MainWindow::createCronPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    QLabel *title = new QLabel(QStringLiteral("定时任务（Cron 解析）"), page);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #6c757d;"));
    QLabel *desc = new QLabel(QStringLiteral("功能开发中..."), page);
    desc->setStyleSheet(QStringLiteral("font-size: 14px; color: #adb5bd;"));
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(desc);
    return page;
}

QWidget *MainWindow::createJwtPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    QLabel *title = new QLabel(QStringLiteral("JWT 解析"), page);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #6c757d;"));
    QLabel *desc = new QLabel(QStringLiteral("功能开发中..."), page);
    desc->setStyleSheet(QStringLiteral("font-size: 14px; color: #adb5bd;"));
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(desc);
    return page;
}

QWidget *MainWindow::createDownloadPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    QLabel *title = new QLabel(QStringLiteral("批量下载"), page);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #6c757d;"));
    QLabel *desc = new QLabel(QStringLiteral("功能开发中..."), page);
    desc->setStyleSheet(QStringLiteral("font-size: 14px; color: #adb5bd;"));
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(desc);
    return page;
}

// ==================== Results ====================

void MainWindow::showAbout()
{
    QString msg = QStringLiteral(
        "<h3>Zane Tool v0.1.0</h3>"
        "<p>基于 ffmpeg 的桌面端图片/视频批量压缩、缩放、格式转换工具。</p>"
        "<p><b>技术栈</b><br>"
        "Qt 6 (Widgets) &middot; C++17 &middot; ffmpeg<br>"
        "MinGW GCC 13.1 &middot; CMake 3.16+<br>"
        "Bootstrap v5 配色 QSS</p>"
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
