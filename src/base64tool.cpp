#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QMap>

#include "base64tool.h"
#include "utils.h"

Base64Tool::Base64Tool(QObject *parent)
    : QObject(parent)
    , m_filePath(nullptr)
    , m_selectBtn(nullptr)
    , m_clearBtn(nullptr)
    , m_dropZone(nullptr)
    , m_output(nullptr)
    , m_infoLabel(nullptr)
    , m_copyBtn(nullptr)
{
}

QWidget *Base64Tool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *group = new QGroupBox(QStringLiteral("图片转Base64"), page);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(12);

    QHBoxLayout *fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);

    m_filePath = new QLineEdit(group);
    m_filePath->setReadOnly(true);
    m_filePath->setPlaceholderText(QStringLiteral("请选择图片文件，或拖放图片到下方区域..."));

    m_selectBtn = new QPushButton(QStringLiteral("选择图片"), group);
    m_selectBtn->setFixedHeight(34);
    m_selectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_selectBtn, &QPushButton::clicked, this, &Base64Tool::onSelectFile);

    m_clearBtn = new QPushButton(QStringLiteral("清除"), group);
    m_clearBtn->setFixedHeight(34);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    m_clearBtn->setEnabled(false);
    connect(m_clearBtn, &QPushButton::clicked, this, &Base64Tool::onClear);

    fileRow->addWidget(m_filePath, 1);
    fileRow->addWidget(m_selectBtn);
    fileRow->addWidget(m_clearBtn);

    m_dropZone = new QLabel(group);
    m_dropZone->setFixedHeight(100);
    m_dropZone->setAlignment(Qt::AlignCenter);
    m_dropZone->setAcceptDrops(true);
    m_dropZone->setCursor(Qt::PointingHandCursor);
    m_dropZone->setStyleSheet(QStringLiteral(
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
    m_dropZone->setText(QStringLiteral("将图片拖放到此处\n或点击上方按钮选择文件"));
    m_dropZone->installEventFilter(this);

    QGroupBox *resultGroup = new QGroupBox(QStringLiteral("转换结果"), page);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setSpacing(10);

    m_output = new QTextEdit(resultGroup);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(QStringLiteral("选择图片后将在此显示 Base64 编码结果..."));
    m_output->setMinimumHeight(180);
    m_output->setStyleSheet(QStringLiteral(
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

    m_infoLabel = new QLabel(resultGroup);
    m_infoLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));

    m_copyBtn = new QPushButton(QStringLiteral("复制到剪贴板"), resultGroup);
    m_copyBtn->setFixedHeight(34);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setEnabled(false);
    connect(m_copyBtn, &QPushButton::clicked, this, &Base64Tool::onCopy);

    bottomRow->addWidget(m_infoLabel, 1);
    bottomRow->addWidget(m_copyBtn);

    resultLayout->addWidget(m_output);
    resultLayout->addLayout(bottomRow);

    groupLayout->addLayout(fileRow);
    groupLayout->addWidget(m_dropZone);
    groupLayout->addWidget(resultGroup, 1);

    mainLayout->addWidget(group, 1);

    return page;
}

void Base64Tool::processFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(m_dropZone, QStringLiteral("错误"),
            QStringLiteral("无法读取文件：%1").arg(filePath));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        QMessageBox::warning(m_dropZone, QStringLiteral("错误"),
            QStringLiteral("文件为空：%1").arg(filePath));
        return;
    }

    QString base64 = data.toBase64();
    QString mime = mimeTypeForFile(filePath);
    QString result = QStringLiteral("data:%1;base64,%2").arg(mime, base64);

    QFileInfo fi(filePath);
    qint64 originalSize = data.size();
    qint64 base64Size = result.size();

    m_filePath->setText(QDir::toNativeSeparators(filePath));
    m_output->setPlainText(result);
    m_infoLabel->setText(QStringLiteral("文件: %1 | Base64: %2")
        .arg(Utils::formatFileSize(originalSize), Utils::formatFileSize(base64Size)));
    m_copyBtn->setEnabled(true);
    m_clearBtn->setEnabled(true);
    m_dropZone->setText(fi.fileName());
    m_dropZone->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  border: 2px solid #198754;"
        "  border-radius: 8px;"
        "  background-color: #d1e7dd;"
        "  color: #0f5132;"
        "  font-size: 13px;"
        "}"));
}

bool Base64Tool::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_dropZone) {
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
                        processFile(url.toLocalFile());
                        return true;
                    }
                }
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void Base64Tool::onSelectFile()
{
    QString filePath = QFileDialog::getOpenFileName(m_dropZone,
        QStringLiteral("选择图片文件"), QString(),
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp *.gif *.svg *.ico *.tiff *.tif)"));
    if (filePath.isEmpty())
        return;
    processFile(filePath);
}

void Base64Tool::onClear()
{
    m_filePath->clear();
    m_output->clear();
    m_infoLabel->clear();
    m_copyBtn->setEnabled(false);
    m_clearBtn->setEnabled(false);
    m_dropZone->setText(QStringLiteral("将图片拖放到此处\n或点击上方按钮选择文件"));
    m_dropZone->setStyleSheet(QStringLiteral(
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

void Base64Tool::onCopy()
{
    QString text = m_output->toPlainText();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_copyBtn->text();
    m_copyBtn->setText(QStringLiteral("已复制"));
    m_copyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyBtn->setText(original);
        m_copyBtn->setEnabled(true);
    });
}

QString Base64Tool::mimeTypeForFile(const QString &filePath) const
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
