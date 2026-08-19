#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QFont>
#include <QFileDialog>
#include <QMessageBox>

#include "jsontool.h"

JsonTool::JsonTool(QObject *parent)
    : QObject(parent)
    , m_inputEdit(nullptr)
    , m_outputStack(nullptr)
    , m_outputTree(nullptr)
    , m_outputText(nullptr)
    , m_formatBtn(nullptr)
    , m_compressBtn(nullptr)
    , m_toggleBtn(nullptr)
    , m_copyBtn(nullptr)
    , m_exportBtn(nullptr)
    , m_clearBtn(nullptr)
    , m_statusLabel(nullptr)
{
}

QJsonDocument JsonTool::parseInput()
{
    QString input = m_inputEdit->toPlainText().trimmed();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(input.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        updateStatus(QStringLiteral("JSON 解析错误: %1 (位置 %2)")
            .arg(error.errorString())
            .arg(error.offset), true);
    }

    return doc;
}

void JsonTool::buildTree(const QJsonDocument &doc)
{
    m_outputTree->clear();

    if (doc.isNull())
        return;

    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        addJsonValue(m_outputTree->invisibleRootItem(), QString(), QJsonValue(obj), QStringList());
    } else if (doc.isArray()) {
        QJsonArray arr = doc.array();
        addJsonValue(m_outputTree->invisibleRootItem(), QString(), QJsonValue(arr), QStringList());
    }
}

QTreeWidgetItem *JsonTool::createItem(QTreeWidgetItem *parent, const QString &key, const QString &value, const QColor &color, const QStringList &path)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parent);
    item->setText(0, key);
    item->setText(1, value);
    item->setForeground(1, color);
    item->setData(0, Qt::UserRole, QVariant(path));

    QFont valueFont = item->font(1);
    valueFont.setFamily(QStringLiteral("Consolas"));
    item->setFont(1, valueFont);

    if (!key.isEmpty()) {
        QFont keyFont = item->font(0);
        keyFont.setBold(true);
        item->setFont(0, keyFont);
    }

    return item;
}

void JsonTool::addJsonValue(QTreeWidgetItem *parent, const QString &key, const QJsonValue &value, const QStringList &path)
{
    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        int count = obj.size();
        QTreeWidgetItem *item = createItem(parent, key,
            QStringLiteral("{ %1 }").arg(count),
            QColor(QStringLiteral("#495057")), path);

        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QStringList childPath = path;
            childPath.append(it.key());
            addJsonValue(item, it.key(), it.value(), childPath);
        }
    } else if (value.isArray()) {
        QJsonArray arr = value.toArray();
        int count = arr.size();
        QTreeWidgetItem *item = createItem(parent, key,
            QStringLiteral("[ %1 ]").arg(count),
            QColor(QStringLiteral("#495057")), path);

        for (int i = 0; i < arr.size(); ++i) {
            QStringList childPath = path;
            childPath.append(QString::number(i));
            addJsonValue(item, QStringLiteral("[%1]").arg(i), arr[i], childPath);
        }
    } else if (value.isString()) {
        createItem(parent, key, QStringLiteral("\"%1\"").arg(value.toString()),
            QColor(QStringLiteral("#198754")), path);
    } else if (value.isDouble()) {
        double d = value.toDouble();
        qint64 i = static_cast<qint64>(d);
        if (d == i) {
            createItem(parent, key, QString::number(i),
                QColor(QStringLiteral("#0d6efd")), path);
        } else {
            createItem(parent, key, QString::number(d, 'g', 15),
                QColor(QStringLiteral("#0d6efd")), path);
        }
    } else if (value.isBool()) {
        createItem(parent, key, value.toBool() ? QStringLiteral("true") : QStringLiteral("false"),
            QColor(QStringLiteral("#fd7e14")), path);
    } else if (value.isNull()) {
        createItem(parent, key, QStringLiteral("null"),
            QColor(QStringLiteral("#6c757d")), path);
    }
}

QJsonValue JsonTool::valueByPath(const QStringList &path) const
{
    QJsonValue current;
    if (m_doc.isArray())
        current = QJsonValue(m_doc.array());
    else if (m_doc.isObject())
        current = QJsonValue(m_doc.object());
    else
        return QJsonValue();

    for (const QString &token : path) {
        if (current.isArray()) {
            bool ok = false;
            int index = token.toInt(&ok);
            if (!ok)
                return QJsonValue();
            QJsonArray arr = current.toArray();
            if (index < 0 || index >= arr.size())
                return QJsonValue();
            current = arr.at(index);
        } else if (current.isObject()) {
            QJsonObject obj = current.toObject();
            if (!obj.contains(token))
                return QJsonValue();
            current = obj.value(token);
        } else {
            return QJsonValue();
        }
    }

    return current;
}

XlsxWriter::Cell JsonTool::cellFromJsonValue(const QJsonValue &value)
{
    if (value.isString())
        return XlsxWriter::Cell::text(value.toString());
    if (value.isDouble())
        return XlsxWriter::Cell::num(value.toDouble());
    if (value.isBool())
        return XlsxWriter::Cell::flag(value.toBool());
    if (value.isObject())
        return XlsxWriter::Cell::text(QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)));
    if (value.isArray())
        return XlsxWriter::Cell::text(QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)));
    return XlsxWriter::Cell();
}

void JsonTool::updateStatus(const QString &text, bool isError)
{
    m_statusLabel->setText(text);
    if (isError) {
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-size: 13px;"));
    } else {
        m_statusLabel->setStyleSheet(QStringLiteral("color: #198754; font-size: 13px;"));
    }
}

QWidget *JsonTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *inputGroup = new QGroupBox(QStringLiteral("输入"), page);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(12);

    m_inputEdit = new QTextEdit(inputGroup);
    m_inputEdit->setPlaceholderText(QStringLiteral("请输入或粘贴 JSON..."));
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setStyleSheet(QStringLiteral(
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
    connect(m_inputEdit, &QTextEdit::textChanged, this, &JsonTool::onInputChanged);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_formatBtn = new QPushButton(QStringLiteral("格式化"), inputGroup);
    m_formatBtn->setFixedHeight(34);
    m_formatBtn->setCursor(Qt::PointingHandCursor);
    connect(m_formatBtn, &QPushButton::clicked, this, &JsonTool::onFormat);

    m_compressBtn = new QPushButton(QStringLiteral("压缩"), inputGroup);
    m_compressBtn->setFixedHeight(34);
    m_compressBtn->setCursor(Qt::PointingHandCursor);
    m_compressBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #198754; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #157347; }"));
    connect(m_compressBtn, &QPushButton::clicked, this, &JsonTool::onCompress);

    m_clearBtn = new QPushButton(QStringLiteral("清除"), inputGroup);
    m_clearBtn->setFixedHeight(34);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(m_clearBtn, &QPushButton::clicked, this, &JsonTool::onClear);

    btnRow->addStretch();
    btnRow->addWidget(m_formatBtn);
    btnRow->addWidget(m_compressBtn);
    btnRow->addWidget(m_clearBtn);

    inputLayout->addWidget(m_inputEdit, 1);
    inputLayout->addLayout(btnRow);

    QGroupBox *outputGroup = new QGroupBox(QStringLiteral("输出"), page);
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    outputLayout->setSpacing(10);

    m_outputStack = new QStackedWidget(outputGroup);

    m_outputTree = new QTreeWidget(m_outputStack);
    m_outputTree->setColumnCount(2);
    m_outputTree->setHeaderLabels({QStringLiteral("Key"), QStringLiteral("Value")});
    m_outputTree->setAlternatingRowColors(false);
    m_outputTree->setRootIsDecorated(true);
    m_outputTree->setAnimated(true);
    m_outputTree->setIndentation(20);
    m_outputTree->header()->setStretchLastSection(true);
    m_outputTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_outputTree->setStyleSheet(QStringLiteral(
        "QTreeWidget {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  border: 1px solid #ced4da;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #212529;"
        "  padding: 4px;"
        "}"
        "QTreeWidget::item {"
        "  padding: 2px 0;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: #e7f1ff;"
        "  color: #212529;"
        "}"
        "QTreeWidget::branch:has-siblings:!adjoins-item {"
        "  border-image: none;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f8f9fa;"
        "  border: none;"
        "  border-bottom: 1px solid #dee2e6;"
        "  padding: 4px 8px;"
        "  font-size: 12px;"
        "  color: #495057;"
        "}"
    ));

    m_outputText = new QTextEdit(m_outputStack);
    m_outputText->setReadOnly(true);
    m_outputText->setAcceptRichText(false);
    m_outputText->setStyleSheet(QStringLiteral(
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

    m_outputStack->addWidget(m_outputTree);
    m_outputStack->addWidget(m_outputText);
    m_outputStack->setCurrentIndex(0);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(16);

    m_statusLabel = new QLabel(outputGroup);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));
    m_statusLabel->setWordWrap(true);

    m_toggleBtn = new QPushButton(QStringLiteral("切换视图"), outputGroup);
    m_toggleBtn->setFixedHeight(34);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setEnabled(false);
    m_toggleBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(m_toggleBtn, &QPushButton::clicked, this, &JsonTool::onToggleView);

    m_copyBtn = new QPushButton(QStringLiteral("复制结果"), outputGroup);
    m_copyBtn->setFixedHeight(34);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setEnabled(false);
    connect(m_copyBtn, &QPushButton::clicked, this, &JsonTool::onCopyResult);

    m_exportBtn = new QPushButton(QStringLiteral("导出 Excel"), outputGroup);
    m_exportBtn->setFixedHeight(34);
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setEnabled(false);
    m_exportBtn->setToolTip(QStringLiteral("选中输出树中的数组节点后，可将其导出为 Excel"));
    m_exportBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #198754; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #157347; }"
        "QPushButton:disabled { background-color: #a9c0b4; color: #fff; }"));
    connect(m_exportBtn, &QPushButton::clicked, this, &JsonTool::onExportExcel);

    connect(m_outputTree, &QTreeWidget::currentItemChanged, this, &JsonTool::onTreeSelectionChanged);

    bottomRow->addWidget(m_statusLabel, 1);
    bottomRow->addWidget(m_toggleBtn);
    bottomRow->addWidget(m_copyBtn);
    bottomRow->addWidget(m_exportBtn);

    outputLayout->addWidget(m_outputStack, 1);
    outputLayout->addLayout(bottomRow);

    mainLayout->addWidget(inputGroup);
    mainLayout->addWidget(outputGroup, 1);

    return page;
}

void JsonTool::showOutput()
{
    m_outputText->setPlainText(m_lastFormattedJson);
    m_toggleBtn->setEnabled(true);
    m_copyBtn->setEnabled(true);
}

void JsonTool::onFormat()
{
    QJsonDocument doc = parseInput();
    if (doc.isNull()) {
        m_outputTree->clear();
        m_outputText->clear();
        m_toggleBtn->setEnabled(false);
        m_copyBtn->setEnabled(false);
        return;
    }

    m_lastFormattedJson = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    m_doc = doc;

    int inChars = m_inputEdit->toPlainText().trimmed().length();
    int outChars = m_lastFormattedJson.length();
    updateStatus(QStringLiteral("有效的 JSON · 输入 %1 字符 · 输出 %2 字符")
        .arg(inChars)
        .arg(outChars), false);

    buildTree(doc);
    m_outputTree->setCurrentItem(nullptr);
    showOutput();
    m_outputTree->expandAll();

    if (m_outputStack->currentIndex() != 0)
        m_outputStack->setCurrentIndex(0);
}

void JsonTool::onCompress()
{
    QJsonDocument doc = parseInput();
    if (doc.isNull()) {
        m_outputTree->clear();
        m_outputText->clear();
        m_toggleBtn->setEnabled(false);
        m_copyBtn->setEnabled(false);
        return;
    }

    m_lastFormattedJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    m_doc = doc;

    int inChars = m_inputEdit->toPlainText().trimmed().length();
    int outChars = m_lastFormattedJson.length();
    updateStatus(QStringLiteral("有效的 JSON · 输入 %1 字符 · 输出 %2 字符 (压缩)")
        .arg(inChars)
        .arg(outChars), false);

    buildTree(doc);
    m_outputTree->setCurrentItem(nullptr);
    showOutput();
    m_outputTree->collapseAll();

    if (m_outputStack->currentIndex() != 0)
        m_outputStack->setCurrentIndex(0);
}

void JsonTool::onToggleView()
{
    int current = m_outputStack->currentIndex();
    int next = (current == 0) ? 1 : 0;
    m_outputStack->setCurrentIndex(next);

    if (next == 0) {
        m_toggleBtn->setText(QStringLiteral("切换视图"));
    } else {
        m_outputText->setPlainText(m_lastFormattedJson);
        m_toggleBtn->setText(QStringLiteral("切换视图"));
    }
}

void JsonTool::onCopyResult()
{
    QString text;
    if (m_outputStack->currentIndex() == 0) {
        text = m_lastFormattedJson;
    } else {
        text = m_outputText->toPlainText();
    }

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

void JsonTool::onTreeSelectionChanged()
{
    QTreeWidgetItem *item = m_outputTree->currentItem();
    if (!item || m_doc.isNull()) {
        m_exportBtn->setEnabled(false);
        return;
    }

    const QStringList path = item->data(0, Qt::UserRole).toStringList();
    m_exportBtn->setEnabled(valueByPath(path).isArray());
}

void JsonTool::onExportExcel()
{
    QTreeWidgetItem *item = m_outputTree->currentItem();
    if (!item || m_doc.isNull())
        return;

    const QStringList path = item->data(0, Qt::UserRole).toStringList();
    const QJsonValue value = valueByPath(path);
    if (!value.isArray())
        return;

    const QJsonArray array = value.toArray();
    if (array.isEmpty()) {
        QMessageBox::warning(m_outputTree, QStringLiteral("提示"),
            QStringLiteral("选中的数组为空，无法导出。"));
        return;
    }

    QStringList headers;
    QList<QList<XlsxWriter::Cell>> rows;

    bool allObjects = true;
    for (int i = 0; i < array.size(); ++i) {
        if (!array.at(i).isObject()) {
            allObjects = false;
            break;
        }
    }

    if (allObjects) {
        QList<QJsonObject> objects;
        objects.reserve(array.size());
        for (int i = 0; i < array.size(); ++i)
            objects.append(array.at(i).toObject());

        for (const QJsonObject &obj : objects) {
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (!headers.contains(it.key()))
                    headers.append(it.key());
            }
        }

        rows.reserve(objects.size());
        for (const QJsonObject &obj : objects) {
            QList<XlsxWriter::Cell> row;
            row.reserve(headers.size());
            for (const QString &header : headers)
                row.append(cellFromJsonValue(obj.value(header)));
            rows.append(row);
        }
    } else {
        headers.append(QStringLiteral("value"));
        rows.reserve(array.size());
        for (int i = 0; i < array.size(); ++i) {
            QList<XlsxWriter::Cell> row;
            row.append(cellFromJsonValue(array.at(i)));
            rows.append(row);
        }
    }

    QString fileBase = path.isEmpty() ? QStringLiteral("data") : path.last();
    fileBase.replace(QLatin1Char('/'), QLatin1Char('_'));
    fileBase.replace(QLatin1Char('\\'), QLatin1Char('_'));
    QString defaultName = fileBase + QStringLiteral(".xlsx");
    QString filePath = QFileDialog::getSaveFileName(m_outputTree,
        QStringLiteral("导出 Excel"), defaultName,
        QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty())
        return;
    if (!filePath.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive))
        filePath += QStringLiteral(".xlsx");

    QString error;
    if (!XlsxWriter::writeSheet(filePath, headers, rows, &error)) {
        QMessageBox::warning(m_outputTree, QStringLiteral("导出失败"),
            QStringLiteral("写入文件失败：%1").arg(error));
        return;
    }

    updateStatus(QStringLiteral("已导出 %1 行 × %2 列 → %3")
        .arg(rows.size())
        .arg(headers.size())
        .arg(filePath), false);
}

void JsonTool::onClear()
{
    m_inputEdit->clear();
    m_outputTree->clear();
    m_outputText->clear();
    m_statusLabel->clear();
    m_lastFormattedJson.clear();
    m_doc = QJsonDocument();
    m_toggleBtn->setEnabled(false);
    m_copyBtn->setEnabled(false);
    m_exportBtn->setEnabled(false);
    m_outputTree->setCurrentItem(nullptr);
    if (m_outputStack->currentIndex() != 0)
        m_outputStack->setCurrentIndex(0);
}

void JsonTool::onInputChanged()
{
    if (m_inputEdit->toPlainText().trimmed().isEmpty()) {
        m_outputTree->clear();
        m_outputText->clear();
        m_statusLabel->clear();
        m_lastFormattedJson.clear();
        m_doc = QJsonDocument();
        m_toggleBtn->setEnabled(false);
        m_copyBtn->setEnabled(false);
        m_exportBtn->setEnabled(false);
        m_outputTree->setCurrentItem(nullptr);
    }
}
