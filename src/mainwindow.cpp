#include "mainwindow.h"
#include "ffmpegprocess.h"
#include "utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
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

MainWindow::MainWindow(const QString &ffmpegPath, QWidget *parent)
    : QMainWindow(parent)
    , m_ffmpegPath(ffmpegPath)
    , m_imageCurrentIndex(0)
    , m_videoCurrentIndex(0)
    , m_imageCancelling(false)
    , m_videoCancelling(false)
    , m_imageSizeBefore(0)
    , m_imageSizeAfter(0)
    , m_imageSuccessCount(0)
    , m_imageFailedCount(0)
    , m_videoSizeBefore(0)
    , m_videoSizeAfter(0)
    , m_videoSuccessCount(0)
    , m_videoFailedCount(0)
    , m_ffmpegImage(new FFmpegProcess(this))
    , m_ffmpegVideo(new FFmpegProcess(this))
{
    setWindowTitle(QStringLiteral("FFmpeg Wrapper"));
    resize(720, 520);
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
}

void MainWindow::setupUi()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createImageTab(), QStringLiteral("图片处理"));
    m_tabWidget->addTab(createVideoTab(), QStringLiteral("视频处理"));
    setCentralWidget(m_tabWidget);

    m_aboutLabel = new QLabel(QStringLiteral("<a href='about' style='color:#6c757d;text-decoration:none;'>v1.0.0 &middot; Zane</a>"), this);
    m_aboutLabel->setCursor(Qt::PointingHandCursor);
    connect(m_aboutLabel, &QLabel::linkActivated, this, &MainWindow::showAbout);
    statusBar()->addPermanentWidget(m_aboutLabel);
    statusBar()->setSizeGripEnabled(false);
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

// ==================== Results ====================

void MainWindow::showAbout()
{
    QString msg = QStringLiteral(
        "<h3>FFmpeg Wrapper v1.0.0</h3>"
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

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (!mimeData->hasUrls()) return;

    int tabIndex = m_tabWidget->currentIndex();
    QListWidget *list = (tabIndex == 0) ? m_imageFileList : m_videoFileList;

    for (const QUrl &url : mimeData->urls()) {
        if (url.isLocalFile())
            list->addItem(url.toLocalFile());
    }
}
