#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QMainWindow>
#include <QCloseEvent>
#include <QStatusBar>
#include <QElapsedTimer>

#include "image_tool.h"
#include "ffmpegprocess.h"
#include "utils.h"

ImageTool::ImageTool(const QString &ffmpegPath, QWidget *parent)
    : QWidget(parent)
    , m_ffmpegPath(ffmpegPath)
    , m_currentIndex(0)
    , m_cancelling(false)
    , m_sizeBefore(0)
    , m_sizeAfter(0)
    , m_successCount(0)
    , m_failedCount(0)
    , m_ffmpeg(new FFmpegProcess(this))
{
    setupUi();

    connect(m_ffmpeg, &FFmpegProcess::finished, this, [this](bool success, int) {
        if (m_cancelling) return;
        QFileInfo fi(m_taskQueue[m_currentIndex].inputPath);
        qint64 inputSize = fi.size();
        QFileInfo fo(ImageProcessor::buildOutputPath(m_taskQueue[m_currentIndex]));
        qint64 outputSize = fo.exists() ? fo.size() : 0;

        if (success) {
            m_successCount++;
            m_sizeBefore += inputSize;
            m_sizeAfter += outputSize;
            Utils::logToFile(QStringLiteral("[IMAGE] OK: %1 -> %2 (%3 -> %4)")
                .arg(fi.fileName(), fo.fileName())
                .arg(Utils::formatFileSize(inputSize), Utils::formatFileSize(outputSize)));
        } else {
            m_failedCount++;
            m_failedFiles.append(fi.fileName());
            Utils::logToFile(QStringLiteral("[IMAGE] FAIL: %1").arg(fi.fileName()));
        }
        m_currentIndex++;
        processNextImage();
    });

    connect(m_ffmpeg, &FFmpegProcess::errorOccurred, this, [this](const QString &msg) {
        m_statusLabel->setText(QStringLiteral("错误: ") + msg);
    });
}

void ImageTool::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // --- File list section ---
    m_fileList = new QListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setMinimumHeight(60);
    m_fileList->setAcceptDrops(true);
    m_fileList->setDragDropMode(QAbstractItemView::DropOnly);

    connect(m_fileList, &QListWidget::itemSelectionChanged,
            this, &ImageTool::onSelectionChanged);

    QHBoxLayout *fileBtnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton(QStringLiteral("添加文件"), this);
    m_removeBtn = new QPushButton(QStringLiteral("移除选中"), this);
    m_removeBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_clearBtn = new QPushButton(QStringLiteral("清空"), this);
    m_clearBtn->setObjectName(QStringLiteral("dangerBtn"));
    fileBtnLayout->addWidget(m_addBtn);
    fileBtnLayout->addWidget(m_removeBtn);
    fileBtnLayout->addWidget(m_clearBtn);
    fileBtnLayout->addStretch();

    connect(m_addBtn, &QPushButton::clicked, this, &ImageTool::onAddFiles);
    connect(m_removeBtn, &QPushButton::clicked, this, &ImageTool::onRemoveSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &ImageTool::onClearFiles);

    // --- Settings row ---
    QHBoxLayout *settingsLayout = new QHBoxLayout();

    QGroupBox *compGroup = new QGroupBox(QStringLiteral("压缩设置"), this);
    QVBoxLayout *compLayout = new QVBoxLayout(compGroup);
    QHBoxLayout *qualityLayout = new QHBoxLayout();
    QLabel *qualityLabel = new QLabel(QStringLiteral("质量:"), this);
    m_qualitySlider = new QSlider(Qt::Horizontal, this);
    m_qualitySlider->setRange(0, 100);
    m_qualitySlider->setValue(75);
    m_qualityLabel = new QLabel(QStringLiteral("75"), this);
    m_qualityLabel->setFixedWidth(30);
    qualityLayout->addWidget(qualityLabel);
    qualityLayout->addWidget(m_qualitySlider);
    qualityLayout->addWidget(m_qualityLabel);
    compLayout->addLayout(qualityLayout);

    connect(m_qualitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_qualityLabel->setText(QString::number(v));
    });

    QGroupBox *scaleGroup = new QGroupBox(QStringLiteral("缩放设置"), this);
    QVBoxLayout *scaleLayout = new QVBoxLayout(scaleGroup);
    m_scaleCheck = new QCheckBox(QStringLiteral("启用缩放"), this);
    QHBoxLayout *scaleWidthLayout = new QHBoxLayout();
    QLabel *widthLabel = new QLabel(QStringLiteral("目标宽度:"), this);
    m_scaleWidth = new QSpinBox(this);
    m_scaleWidth->setRange(1, 10000);
    m_scaleWidth->setValue(1920);
    m_scaleWidth->setSuffix(QStringLiteral(" px"));
    m_scaleWidth->setEnabled(false);
    scaleWidthLayout->addWidget(widthLabel);
    scaleWidthLayout->addWidget(m_scaleWidth);
    scaleLayout->addWidget(m_scaleCheck);
    scaleLayout->addLayout(scaleWidthLayout);

    connect(m_scaleCheck, &QCheckBox::toggled, m_scaleWidth, &QSpinBox::setEnabled);

    settingsLayout->addWidget(compGroup);
    settingsLayout->addWidget(scaleGroup);
    settingsLayout->setStretch(0, 1);
    settingsLayout->setStretch(1, 1);

    // --- Format + Output row ---
    QHBoxLayout *outputRow = new QHBoxLayout();

    QGroupBox *formatGroup = new QGroupBox(QStringLiteral("输出格式"), this);
    QHBoxLayout *formatLayout = new QHBoxLayout(formatGroup);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(QStringLiteral("保持原格式"), QString());
    m_formatCombo->addItem(QStringLiteral("JPG"), QStringLiteral("jpg"));
    m_formatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    m_formatCombo->addItem(QStringLiteral("WebP"), QStringLiteral("webp"));
    m_formatCombo->addItem(QStringLiteral("BMP"), QStringLiteral("bmp"));
    formatLayout->addWidget(m_formatCombo);

    QGroupBox *outDirGroup = new QGroupBox(QStringLiteral("输出目录"), this);
    QHBoxLayout *outDirLayout = new QHBoxLayout(outDirGroup);
    m_outputDir = new QLineEdit(this);
    m_outputBrowse = new QPushButton(QStringLiteral("浏览"), this);
    outDirLayout->addWidget(m_outputDir);
    outDirLayout->addWidget(m_outputBrowse);

    connect(m_outputBrowse, &QPushButton::clicked, this, &ImageTool::onOutputBrowse);

    outputRow->addWidget(formatGroup);
    outputRow->addWidget(outDirGroup);

    // --- Resolution preview ---
    m_resPreview = new QLabel(this);
    m_resPreview->setVisible(false);

    // --- Progress / Status ---
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(12);

    m_statusLabel = new QLabel(QStringLiteral("就绪"), this);

    // --- Start/Cancel ---
    QHBoxLayout *actionLayout = new QHBoxLayout();
    m_startBtn = new QPushButton(QStringLiteral("开始处理"), this);
    m_startBtn->setFixedHeight(42);
    m_startBtn->setFixedWidth(160);
    m_startBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setFixedHeight(42);
    m_cancelBtn->setFixedWidth(160);
    m_cancelBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_cancelBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    actionLayout->addStretch();
    actionLayout->addWidget(m_startBtn);
    actionLayout->addSpacing(16);
    actionLayout->addWidget(m_cancelBtn);
    actionLayout->addStretch();

    connect(m_startBtn, &QPushButton::clicked, this, &ImageTool::onStart);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ImageTool::onCancel);

    // --- Assemble ---
    mainLayout->addWidget(m_fileList);
    mainLayout->addLayout(fileBtnLayout);
    mainLayout->addLayout(settingsLayout);
    mainLayout->addLayout(outputRow);
    mainLayout->addWidget(m_resPreview);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(actionLayout);
}

void ImageTool::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择图片文件"), QString(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp);;所有文件 (*.*)"));
    int skipped = 0;
    for (const QString &f : files) {
        if (Utils::isSupportedImageFile(f))
            m_fileList->addItem(f);
        else
            ++skipped;
    }
    if (skipped > 0) {
        QWidget *topLevel = window();
        if (QMainWindow *mw = qobject_cast<QMainWindow *>(topLevel))
            mw->statusBar()->showMessage(QStringLiteral("已跳过 %1 个不支持的图片文件").arg(skipped), 3000);
    }
}

void ImageTool::onRemoveSelected()
{
    QList<QListWidgetItem *> items = m_fileList->selectedItems();
    for (QListWidgetItem *item : items) {
        delete m_fileList->takeItem(m_fileList->row(item));
    }
}

void ImageTool::onClearFiles()
{
    m_fileList->clear();
}

void ImageTool::onOutputBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
    if (!dir.isEmpty())
        m_outputDir->setText(dir);
}

void ImageTool::onStart()
{
    if (m_fileList->count() == 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加文件。"));
        return;
    }
    if (m_outputDir->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择输出目录。"));
        return;
    }

    m_taskQueue.clear();
    m_currentIndex = 0;
    m_cancelling = false;
    m_successCount = 0;
    m_failedCount = 0;
    m_sizeBefore = 0;
    m_sizeAfter = 0;
    m_failedFiles.clear();

    for (int i = 0; i < m_fileList->count(); ++i) {
        ImageTask task;
        task.inputPath = m_fileList->item(i)->text();
        task.outputDir = m_outputDir->text();
        task.format = m_formatCombo->currentData().toString();
        task.quality = m_qualitySlider->value();
        task.enableScale = m_scaleCheck->isChecked();
        task.scaleWidth = m_scaleWidth->value();
        m_taskQueue.append(task);
    }

    setImageUiEnabled(false);
    processNextImage();
}

void ImageTool::onCancel()
{
    m_cancelling = true;
    m_ffmpeg->cancel();
    m_statusLabel->setText(QStringLiteral("已取消"));
    setImageUiEnabled(true);
    m_progressBar->setValue(m_taskQueue.isEmpty() ? 0 : m_currentIndex * 100 / m_taskQueue.size());
}

void ImageTool::onSelectionChanged()
{
    updateImageResolutionPreview();
}

void ImageTool::processNextImage()
{
    if (m_cancelling) {
        setImageUiEnabled(true);
        return;
    }

    if (m_currentIndex >= m_taskQueue.size()) {
        m_statusLabel->setText(QStringLiteral("处理完成"));
        showBatchSummary();
        setImageUiEnabled(true);
        return;
    }

    const ImageTask &task = m_taskQueue[m_currentIndex];
    QStringList args = ImageProcessor::buildArgs(task);
    QString outputPath = ImageProcessor::buildOutputPath(task);
    args << outputPath;

    int total = m_taskQueue.size();
    int current = m_currentIndex + 1;
    m_progressBar->setValue(total > 0 ? (m_currentIndex * 100 / total) : 0);
    m_statusLabel->setText(
        QStringLiteral("处理中 %1/%2").arg(current).arg(total));

    QString cmdLine = m_ffmpegPath + QStringLiteral(" ") + args.join(QStringLiteral(" "));
    Utils::logToFile(QStringLiteral("[IMAGE] ") + cmdLine);

    m_ffmpeg->start(m_ffmpegPath, args);
}

void ImageTool::setImageUiEnabled(bool enabled)
{
    m_addBtn->setEnabled(enabled);
    m_removeBtn->setEnabled(enabled);
    m_clearBtn->setEnabled(enabled);
    m_qualitySlider->setEnabled(enabled);
    m_scaleCheck->setEnabled(enabled);
    m_scaleWidth->setEnabled(enabled && m_scaleCheck->isChecked());
    m_formatCombo->setEnabled(enabled);
    m_outputDir->setEnabled(enabled);
    m_outputBrowse->setEnabled(enabled);
    m_startBtn->setEnabled(enabled);
    m_cancelBtn->setEnabled(!enabled);

    if (enabled) {
        m_progressBar->setValue(100);
    }
}

void ImageTool::updateImageResolutionPreview()
{
    if (m_fileList->selectedItems().size() != 1) {
        m_resPreview->setVisible(false);
        return;
    }
    m_resPreview->setVisible(false);
}

void ImageTool::showBatchSummary()
{
    int total = m_currentIndex;
    int success = m_successCount;
    int failed = m_failedCount;
    qint64 sizeBefore = m_sizeBefore;
    qint64 sizeAfter = m_sizeAfter;
    QStringList failedFiles = m_failedFiles;

    QString msg;
    msg += QStringLiteral("<h3>图片 批量处理完成</h3>");
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

void ImageTool::closeEvent(QCloseEvent *event)
{
    if (m_ffmpeg->isRunning())
        m_ffmpeg->cancel();
    QWidget::closeEvent(event);
}
