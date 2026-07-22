#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDesktopServices>
#include <QImage>
#include <QPainter>
#include <QTabWidget>

#include <cstring>

#include "qrcodetool.h"
#include "screenshottool.h"

#include "third_party/qrcodegen.hpp"
#include "third_party/quirc.h"

QrCodeTool::QrCodeTool(ScreenshotTool *screenshotTool, QObject *parent)
    : QObject(parent)
    , m_screenshotTool(screenshotTool)
    , m_genInput(nullptr)
    , m_genEccCombo(nullptr)
    , m_genScaleSpin(nullptr)
    , m_genPreview(nullptr)
    , m_genStatusLabel(nullptr)
    , m_genSaveBtn(nullptr)
    , m_genCopyBtn(nullptr)
    , m_decFilePath(nullptr)
    , m_decSelectBtn(nullptr)
    , m_decScreenBtn(nullptr)
    , m_decDropZone(nullptr)
    , m_decPreview(nullptr)
    , m_decOutput(nullptr)
    , m_decInfoLabel(nullptr)
    , m_decCopyBtn(nullptr)
    , m_decOpenBtn(nullptr)
{
    connect(m_screenshotTool, &ScreenshotTool::qrScreenshotCaptured,
            this, [this](const QImage &image) {
        processQrDecodeImage(image, QStringLiteral("屏幕截图"));
    });
}

QWidget *QrCodeTool::createPage()
{
    QWidget *page = new QWidget();
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

    QWidget *genTab = new QWidget(tabs);
    QVBoxLayout *genLayout = new QVBoxLayout(genTab);
    genLayout->setSpacing(12);
    genLayout->setContentsMargins(0, 12, 0, 12);

    QGroupBox *inputGroup = new QGroupBox(QStringLiteral("内容"), genTab);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(10);

    m_genInput = new QTextEdit(inputGroup);
    m_genInput->setPlaceholderText(QStringLiteral("输入要生成二维码的文本或链接..."));
    m_genInput->setMaximumHeight(100);
    m_genInput->setAcceptRichText(false);
    m_genInput->setStyleSheet(QStringLiteral(
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
    connect(m_genInput, &QTextEdit::textChanged, this, &QrCodeTool::onGenerate);

    QHBoxLayout *optionRow = new QHBoxLayout();
    optionRow->setSpacing(8);

    QLabel *eccLabel = new QLabel(QStringLiteral("纠错等级:"), inputGroup);
    m_genEccCombo = new QComboBox(inputGroup);
    m_genEccCombo->addItem(QStringLiteral("L (7%)"), 0);
    m_genEccCombo->addItem(QStringLiteral("M (15%)"), 1);
    m_genEccCombo->addItem(QStringLiteral("Q (25%)"), 2);
    m_genEccCombo->addItem(QStringLiteral("H (30%)"), 3);
    m_genEccCombo->setCurrentIndex(1);
    connect(m_genEccCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QrCodeTool::onGenerate);

    QLabel *scaleLabel = new QLabel(QStringLiteral("尺寸:"), inputGroup);
    m_genScaleSpin = new QSpinBox(inputGroup);
    m_genScaleSpin->setRange(2, 10);
    m_genScaleSpin->setValue(4);
    m_genScaleSpin->setSuffix(QStringLiteral(" px/格"));
    connect(m_genScaleSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &QrCodeTool::onGenerate);

    optionRow->addWidget(eccLabel);
    optionRow->addWidget(m_genEccCombo);
    optionRow->addSpacing(16);
    optionRow->addWidget(scaleLabel);
    optionRow->addWidget(m_genScaleSpin);
    optionRow->addStretch(1);

    inputLayout->addWidget(m_genInput);
    inputLayout->addLayout(optionRow);

    QGroupBox *previewGroup = new QGroupBox(QStringLiteral("预览"), genTab);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setSpacing(10);

    m_genPreview = new QLabel(previewGroup);
    m_genPreview->setAlignment(Qt::AlignCenter);
    m_genPreview->setMinimumHeight(260);
    m_genPreview->setStyleSheet(QStringLiteral(
        "QLabel { border: 1px solid #ced4da; border-radius: 6px; background-color: #ffffff; }"));
    m_genPreview->setText(QStringLiteral("输入内容后自动生成二维码"));

    m_genStatusLabel = new QLabel(previewGroup);
    m_genStatusLabel->setAlignment(Qt::AlignCenter);
    m_genStatusLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));

    QHBoxLayout *genBtnRow = new QHBoxLayout();
    genBtnRow->setSpacing(8);

    m_genSaveBtn = new QPushButton(QStringLiteral("保存为 PNG"), previewGroup);
    m_genSaveBtn->setFixedHeight(34);
    m_genSaveBtn->setCursor(Qt::PointingHandCursor);
    m_genSaveBtn->setEnabled(false);
    connect(m_genSaveBtn, &QPushButton::clicked, this, &QrCodeTool::onSaveImage);

    m_genCopyBtn = new QPushButton(QStringLiteral("复制到剪贴板"), previewGroup);
    m_genCopyBtn->setFixedHeight(34);
    m_genCopyBtn->setCursor(Qt::PointingHandCursor);
    m_genCopyBtn->setEnabled(false);
    connect(m_genCopyBtn, &QPushButton::clicked, this, &QrCodeTool::onCopyImage);

    genBtnRow->addStretch(1);
    genBtnRow->addWidget(m_genSaveBtn);
    genBtnRow->addWidget(m_genCopyBtn);

    previewLayout->addWidget(m_genPreview, 1);
    previewLayout->addWidget(m_genStatusLabel);
    previewLayout->addLayout(genBtnRow);

    genLayout->addWidget(inputGroup);
    genLayout->addWidget(previewGroup, 1);

    QWidget *decTab = new QWidget(tabs);
    QVBoxLayout *decLayout = new QVBoxLayout(decTab);
    decLayout->setSpacing(12);
    decLayout->setContentsMargins(0, 12, 0, 12);

    QGroupBox *sourceGroup = new QGroupBox(QStringLiteral("选择图片来源"), decTab);
    QVBoxLayout *sourceLayout = new QVBoxLayout(sourceGroup);
    sourceLayout->setSpacing(10);

    QHBoxLayout *fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);

    m_decFilePath = new QLineEdit(sourceGroup);
    m_decFilePath->setReadOnly(true);
    m_decFilePath->setPlaceholderText(QStringLiteral("请选择图片文件，或使用屏幕识别 / 拖放图片..."));

    m_decSelectBtn = new QPushButton(QStringLiteral("选择图片"), sourceGroup);
    m_decSelectBtn->setFixedHeight(34);
    m_decSelectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_decSelectBtn, &QPushButton::clicked, this, &QrCodeTool::onSelectImage);

    m_decScreenBtn = new QPushButton(QStringLiteral("屏幕识别"), sourceGroup);
    m_decScreenBtn->setFixedHeight(34);
    m_decScreenBtn->setCursor(Qt::PointingHandCursor);
    connect(m_decScreenBtn, &QPushButton::clicked, this, &QrCodeTool::onScreenCapture);

    fileRow->addWidget(m_decFilePath, 1);
    fileRow->addWidget(m_decSelectBtn);
    fileRow->addWidget(m_decScreenBtn);

    m_decDropZone = new QLabel(sourceGroup);
    m_decDropZone->setFixedHeight(70);
    m_decDropZone->setAlignment(Qt::AlignCenter);
    m_decDropZone->setAcceptDrops(true);
    m_decDropZone->setCursor(Qt::PointingHandCursor);
    m_decDropZone->setStyleSheet(QStringLiteral(
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
    m_decDropZone->setText(QStringLiteral("将图片拖放到此处进行识别"));
    m_decDropZone->installEventFilter(this);

    sourceLayout->addLayout(fileRow);
    sourceLayout->addWidget(m_decDropZone);

    QGroupBox *resultGroup = new QGroupBox(QStringLiteral("识别结果"), decTab);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setSpacing(10);

    QHBoxLayout *resultBody = new QHBoxLayout();
    resultBody->setSpacing(12);

    m_decPreview = new QLabel(resultGroup);
    m_decPreview->setFixedSize(200, 200);
    m_decPreview->setAlignment(Qt::AlignCenter);
    m_decPreview->setStyleSheet(QStringLiteral(
        "QLabel { border: 1px solid #ced4da; border-radius: 6px; background-color: #f8f9fa; color: #6c757d; }"));
    m_decPreview->setText(QStringLiteral("图片预览"));

    m_decOutput = new QTextEdit(resultGroup);
    m_decOutput->setReadOnly(true);
    m_decOutput->setPlaceholderText(QStringLiteral("识别结果将在此显示..."));
    m_decOutput->setStyleSheet(QStringLiteral(
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

    resultBody->addWidget(m_decPreview);
    resultBody->addWidget(m_decOutput, 1);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(8);

    m_decInfoLabel = new QLabel(resultGroup);
    m_decInfoLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));

    m_decCopyBtn = new QPushButton(QStringLiteral("复制到剪贴板"), resultGroup);
    m_decCopyBtn->setFixedHeight(34);
    m_decCopyBtn->setCursor(Qt::PointingHandCursor);
    m_decCopyBtn->setEnabled(false);
    connect(m_decCopyBtn, &QPushButton::clicked, this, &QrCodeTool::onCopyResult);

    m_decOpenBtn = new QPushButton(QStringLiteral("打开链接"), resultGroup);
    m_decOpenBtn->setFixedHeight(34);
    m_decOpenBtn->setCursor(Qt::PointingHandCursor);
    m_decOpenBtn->setEnabled(false);
    connect(m_decOpenBtn, &QPushButton::clicked, this, &QrCodeTool::onOpenLink);

    bottomRow->addWidget(m_decInfoLabel, 1);
    bottomRow->addWidget(m_decCopyBtn);
    bottomRow->addWidget(m_decOpenBtn);

    resultLayout->addLayout(resultBody, 1);
    resultLayout->addLayout(bottomRow);

    decLayout->addWidget(sourceGroup);
    decLayout->addWidget(resultGroup, 1);

    tabs->addTab(genTab, QStringLiteral("生成"));
    tabs->addTab(decTab, QStringLiteral("识别"));

    mainLayout->addWidget(tabs, 1);

    return page;
}

bool QrCodeTool::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_decDropZone) {
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
                            QMessageBox::warning(m_decDropZone, QStringLiteral("错误"),
                                QStringLiteral("无法加载图片：%1").arg(filePath));
                            return true;
                        }
                        m_decFilePath->setText(QDir::toNativeSeparators(filePath));
                        processQrDecodeImage(image, QFileInfo(filePath).fileName());
                        return true;
                    }
                }
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void QrCodeTool::onGenerate()
{
    QString text = m_genInput->toPlainText();
    if (text.isEmpty()) {
        m_genPixmap = QPixmap();
        m_genPreview->setPixmap(QPixmap());
        m_genPreview->setText(QStringLiteral("输入内容后自动生成二维码"));
        m_genStatusLabel->clear();
        m_genSaveBtn->setEnabled(false);
        m_genCopyBtn->setEnabled(false);
        return;
    }

    static const qrcodegen::QrCode::Ecc eccTable[] = {
        qrcodegen::QrCode::Ecc::LOW,
        qrcodegen::QrCode::Ecc::MEDIUM,
        qrcodegen::QrCode::Ecc::QUARTILE,
        qrcodegen::QrCode::Ecc::HIGH,
    };
    int eccIndex = m_genEccCombo->currentData().toInt();
    qrcodegen::QrCode::Ecc ecc = eccTable[qBound(0, eccIndex, 3)];

    try {
        QByteArray utf8 = text.toUtf8();
        qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(utf8.constData(), ecc);

        int scale = m_genScaleSpin->value();
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

        m_genPixmap = QPixmap::fromImage(img);
        m_genPreview->setPixmap(m_genPixmap.scaled(m_genPreview->size(),
            Qt::KeepAspectRatio, Qt::FastTransformation));
        m_genStatusLabel->setText(QStringLiteral("版本 %1 | %2×%3 模块 | 输出 %4×%4 px")
            .arg(qr.getVersion()).arg(modules).arg(modules).arg(imgSize));
        m_genSaveBtn->setEnabled(true);
        m_genCopyBtn->setEnabled(true);
    } catch (const std::length_error &) {
        m_genPixmap = QPixmap();
        m_genPreview->setPixmap(QPixmap());
        m_genPreview->setText(QStringLiteral("内容过长，无法生成"));
        m_genStatusLabel->setText(QStringLiteral("请缩短内容或降低纠错等级"));
        m_genSaveBtn->setEnabled(false);
        m_genCopyBtn->setEnabled(false);
    }
}

void QrCodeTool::onSaveImage()
{
    if (m_genPixmap.isNull())
        return;
    QString filePath = QFileDialog::getSaveFileName(m_genPreview,
        QStringLiteral("保存二维码"), QStringLiteral("qrcode.png"),
        QStringLiteral("PNG 图片 (*.png)"));
    if (filePath.isEmpty())
        return;
    if (!m_genPixmap.save(filePath, "PNG")) {
        QMessageBox::warning(m_genPreview, QStringLiteral("错误"),
            QStringLiteral("保存失败：%1").arg(filePath));
    }
}

void QrCodeTool::onCopyImage()
{
    if (m_genPixmap.isNull())
        return;
    QApplication::clipboard()->setPixmap(m_genPixmap);
    QString original = m_genCopyBtn->text();
    m_genCopyBtn->setText(QStringLiteral("已复制"));
    m_genCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_genCopyBtn->setText(original);
        m_genCopyBtn->setEnabled(true);
    });
}

void QrCodeTool::onSelectImage()
{
    QString filePath = QFileDialog::getOpenFileName(m_decDropZone,
        QStringLiteral("选择图片文件"), QString(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp *.gif *.ico *.tiff *.tif)"));
    if (filePath.isEmpty())
        return;
    QImage image(filePath);
    if (image.isNull()) {
        QMessageBox::warning(m_decDropZone, QStringLiteral("错误"),
            QStringLiteral("无法加载图片：%1").arg(filePath));
        return;
    }
    m_decFilePath->setText(QDir::toNativeSeparators(filePath));
    processQrDecodeImage(image, QFileInfo(filePath).fileName());
}

void QrCodeTool::onScreenCapture()
{
    m_screenshotTool->startScreenshotForQr();
}

void QrCodeTool::processQrDecodeImage(const QImage &image, const QString &sourceDesc)
{
    m_decPreview->setPixmap(QPixmap::fromImage(image).scaled(200, 200,
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
        m_decOutput->setPlainText(QStringLiteral("未识别到二维码"));
        m_decInfoLabel->setText(QStringLiteral("来源: %1").arg(sourceDesc));
        m_decCopyBtn->setEnabled(false);
        m_decOpenBtn->setEnabled(false);
        return;
    }

    QStringList numbered;
    for (int i = 0; i < results.size(); ++i) {
        if (results.size() > 1)
            numbered << QStringLiteral("[%1] %2").arg(i + 1).arg(results[i]);
        else
            numbered << results[i];
    }
    m_decOutput->setPlainText(numbered.join(QStringLiteral("\n\n")));
    m_decInfoLabel->setText(QStringLiteral("来源: %1 | 识别到 %2 个二维码")
        .arg(sourceDesc).arg(results.size()));
    m_decCopyBtn->setEnabled(true);

    bool isUrl = results.first().startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
              || results.first().startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    m_decOpenBtn->setEnabled(isUrl);
}

void QrCodeTool::onCopyResult()
{
    QString text = m_decOutput->toPlainText();
    if (text.isEmpty() || text == QStringLiteral("未识别到二维码"))
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_decCopyBtn->text();
    m_decCopyBtn->setText(QStringLiteral("已复制"));
    m_decCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_decCopyBtn->setText(original);
        m_decCopyBtn->setEnabled(true);
    });
}

void QrCodeTool::onOpenLink()
{
    QString text = m_decOutput->toPlainText();
    if (text.startsWith(QStringLiteral("[1] ")))
        text = text.mid(4).section(QStringLiteral("\n"), 0, 0);
    QUrl url(text.trimmed());
    if (url.isValid())
        QDesktopServices::openUrl(url);
}
