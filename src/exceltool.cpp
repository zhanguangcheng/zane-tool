#include "exceltool.h"

#include <QApplication>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QTabWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QColor>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVector>
#include <algorithm>

#include "xlsxreader.h"

ExcelTool::ExcelTool(QObject *parent)
    : QObject(parent)
    , m_tabWidget(nullptr)
    , m_headerTable(nullptr)
    , m_dataTable(nullptr)
    , m_clearTable(nullptr)
    , m_fileList(nullptr)
    , m_outputDirEdit(nullptr)
    , m_statusLabel(nullptr)
{
}

QList<int> ExcelTool::parseClearColumns(const QString &input, bool *allOk)
{
    QList<int> cols;
    *allOk = true;

    const QStringList tokens = input.split(QRegularExpression(QStringLiteral("[,\\s;，；]")), Qt::SkipEmptyParts);
    for (const QString &tok : tokens) {
        if (tok.contains(QLatin1Char(':'))) {
            const QStringList range = tok.split(QLatin1Char(':'));
            if (range.size() != 2) {
                *allOk = false;
                continue;
            }
            int a = Xlsx::columnFromLetter(range.at(0));
            int b = Xlsx::columnFromLetter(range.at(1));
            if (a <= 0 || b <= 0) {
                *allOk = false;
                continue;
            }
            if (a > b)
                std::swap(a, b);
            for (int c = a; c <= b; ++c)
                cols.append(c);
        } else {
            const int c = Xlsx::columnFromLetter(tok);
            if (c <= 0) {
                *allOk = false;
                continue;
            }
            cols.append(c);
        }
    }

    std::sort(cols.begin(), cols.end());
    cols.erase(std::unique(cols.begin(), cols.end()), cols.end());
    return cols;
}

bool ExcelTool::parseExtraConditions(const QString &text,
                                     QList<QList<ClearCondition>> *groups,
                                     QString *err)
{
    groups->clear();
    if (err)
        err->clear();
    if (text.isEmpty())
        return true;

    const QStringList groupStrs = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &gs : groupStrs) {
        QList<ClearCondition> group;
        const QStringList atomStrs = gs.split(QLatin1Char('|'), Qt::SkipEmptyParts);
        for (const QString &asis : atomStrs) {
            const QString as = asis.trimmed();
            if (as.isEmpty())
                continue;
            const int ne = as.indexOf(QLatin1String("!="));
            const int eq = as.indexOf(QLatin1Char('='));
            int opPos = -1;
            bool notEq = false;
            if (ne != -1) {
                opPos = ne;
                notEq = true;
            } else if (eq != -1) {
                opPos = eq;
                notEq = false;
            } else {
                if (err)
                    *err = QStringLiteral("「%1」缺少 = 或 !=").arg(as);
                return false;
            }
            const QString colName = as.left(opPos).trimmed();
            if (Xlsx::columnFromLetter(colName) == 0) {
                if (err)
                    *err = QStringLiteral("「%1」的列 %2 无效").arg(as, colName);
                return false;
            }
            const QString value = as.mid(opPos + (notEq ? 2 : 1));
            ClearCondition cond;
            cond.column = colName.toUpper();
            if (value.isEmpty()) {
                cond.op = notEq ? ClearCondition::Op::NotEmpty : ClearCondition::Op::Empty;
            } else {
                cond.op = notEq ? ClearCondition::Op::Ne : ClearCondition::Op::Eq;
                cond.value = value;
            }
            group.append(cond);
        }
        if (!group.isEmpty())
            groups->append(group);
    }
    return true;
}

QString ExcelTool::serializeExtraConditions(const QList<QList<ClearCondition>> &groups)
{
    QStringList groupStrs;
    for (const auto &group : groups) {
        QStringList atomStrs;
        for (const ClearCondition &c : group) {
            QString atom = c.column;
            switch (c.op) {
            case ClearCondition::Op::Eq:
                atom += QLatin1Char('=') + c.value;
                break;
            case ClearCondition::Op::Ne:
                atom += QLatin1String("!=") + c.value;
                break;
            case ClearCondition::Op::Empty:
                atom += QLatin1Char('=');
                break;
            case ClearCondition::Op::NotEmpty:
                atom += QLatin1String("!=");
                break;
            }
            atomStrs.append(atom);
        }
        groupStrs.append(atomStrs.join(QLatin1Char('|')));
    }
    return groupStrs.join(QLatin1Char(';'));
}

bool ExcelTool::evalExtraCondition(const Xlsx::Sheet &sheet, int row, const ClearCondition &cond)
{
    const int col = Xlsx::columnFromLetter(cond.column);
    if (col <= 0)
        return false;
    switch (cond.op) {
    case ClearCondition::Op::Empty:
        return Xlsx::cellText(sheet, row, col).trimmed().isEmpty();
    case ClearCondition::Op::NotEmpty:
        return !Xlsx::cellText(sheet, row, col).trimmed().isEmpty();
    case ClearCondition::Op::Eq:
        return Xlsx::cellText(sheet, row, col) == cond.value;
    case ClearCondition::Op::Ne:
        return Xlsx::cellText(sheet, row, col) != cond.value;
    }
    return false;
}

void ExcelTool::updateStatus(const QString &text, bool isError)
{
    m_statusLabel->setText(text);
    if (isError) {
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-size: 13px;"));
    } else {
        m_statusLabel->setStyleSheet(QStringLiteral("color: #198754; font-size: 13px;"));
    }
}

QTableWidget *ExcelTool::activeRuleTable() const
{
    if (!m_tabWidget)
        return nullptr;
    return qobject_cast<QTableWidget *>(m_tabWidget->currentWidget());
}

QWidget *ExcelTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ================= 处理规则 =================
    QGroupBox *ruleGroup = new QGroupBox(QStringLiteral("处理规则"), page);
    QVBoxLayout *ruleLayout = new QVBoxLayout(ruleGroup);
    ruleLayout->setSpacing(10);

    QLabel *hint = new QLabel(QStringLiteral(
        "按顺序执行：①表头精确替换 ②数据子串替换（含匹配即替换） ③条件清空/删除行（命中条件后清空指定列或删除整行并上移）。仅处理每个文件的第一个工作表。"),
        ruleGroup);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 12px;"));
    ruleLayout->addWidget(hint);

    auto makeTable = [](const QStringList &headers) {
        QTableWidget *table = new QTableWidget(0, headers.size());
        table->setHorizontalHeaderLabels(headers);
        table->verticalHeader()->setVisible(false);
        table->setAlternatingRowColors(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        table->setEditTriggers(QAbstractItemView::AllEditTriggers);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setHighlightSections(false);
        table->setMinimumHeight(120);
        return table;
    };

    m_headerTable = makeTable({QStringLiteral("查找"), QStringLiteral("替换为")});
    m_dataTable = makeTable({QStringLiteral("查找"), QStringLiteral("替换为")});
    m_clearTable = makeTable({QStringLiteral("动作"), QStringLiteral("条件列"), QStringLiteral("等于值"), QStringLiteral("附加条件"), QStringLiteral("清空列")});
    m_clearTable->horizontalHeaderItem(0)->setToolTip(QStringLiteral("清空列：命中后清空指定列；删除行：命中后删除整行并上移（表头保留）"));
    m_clearTable->horizontalHeaderItem(1)->setToolTip(QStringLiteral("列字母，如 A"));
    m_clearTable->horizontalHeaderItem(3)->setToolTip(QStringLiteral(
        "选填。与主条件同时满足（并且）。|=或者，组间分号=并且；"
        "原子格式：列=值(等于)、列!=值(不等于)、列=(为空)、列!=(非空)。\n"
        "例：B=|C=|D= 表示 B/C/D 任一为空"));
    m_clearTable->horizontalHeaderItem(4)->setToolTip(QStringLiteral("仅动作为「清空列」时使用。逗号分隔多个列，如 D,E,F；也支持范围 D:F"));
    m_clearTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_clearTable->setColumnWidth(0, 100);

    m_tabWidget = new QTabWidget(ruleGroup);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->addTab(m_headerTable, QStringLiteral("表头替换"));
    m_tabWidget->addTab(m_dataTable, QStringLiteral("数据替换"));
    m_tabWidget->addTab(m_clearTable, QStringLiteral("条件清空"));
    ruleLayout->addWidget(m_tabWidget, 1);

    QHBoxLayout *ruleBtns = new QHBoxLayout();
    ruleBtns->setSpacing(8);

    QPushButton *addBtn = new QPushButton(QStringLiteral("添加规则"), ruleGroup);
    addBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QPushButton::clicked, this, &ExcelTool::onAddRule);

    QPushButton *delBtn = new QPushButton(QStringLiteral("删除选中行"), ruleGroup);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "border-radius: 4px; font-size: 13px; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(delBtn, &QPushButton::clicked, this, &ExcelTool::onRemoveRules);

    QPushButton *importBtn = new QPushButton(QStringLiteral("导入规则"), ruleGroup);
    importBtn->setCursor(Qt::PointingHandCursor);
    connect(importBtn, &QPushButton::clicked, this, &ExcelTool::onImportRules);

    QPushButton *exportBtn = new QPushButton(QStringLiteral("导出规则"), ruleGroup);
    exportBtn->setCursor(Qt::PointingHandCursor);
    connect(exportBtn, &QPushButton::clicked, this, &ExcelTool::onExportRules);

    QPushButton *clearRulesBtn = new QPushButton(QStringLiteral("清空规则"), ruleGroup);
    clearRulesBtn->setCursor(Qt::PointingHandCursor);
    clearRulesBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "border-radius: 4px; font-size: 13px; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(clearRulesBtn, &QPushButton::clicked, this, &ExcelTool::onClearRules);

    QPushButton *helpBtn = new QPushButton(QStringLiteral("语法帮助"), ruleGroup);
    helpBtn->setCursor(Qt::PointingHandCursor);
    connect(helpBtn, &QPushButton::clicked, this, &ExcelTool::onShowSyntaxHelp);

    ruleBtns->addWidget(addBtn);
    ruleBtns->addWidget(delBtn);
    ruleBtns->addWidget(importBtn);
    ruleBtns->addWidget(exportBtn);
    ruleBtns->addWidget(clearRulesBtn);
    ruleBtns->addWidget(helpBtn);
    ruleBtns->addStretch();
    ruleLayout->addLayout(ruleBtns);

    // ================= 批处理文件 =================
    QGroupBox *fileGroup = new QGroupBox(QStringLiteral("批处理文件"), page);
    QVBoxLayout *fileLayout = new QVBoxLayout(fileGroup);
    fileLayout->setSpacing(10);

    QHBoxLayout *fileBtns = new QHBoxLayout();
    fileBtns->setSpacing(8);

    QPushButton *addFileBtn = new QPushButton(QStringLiteral("添加文件..."), fileGroup);
    addFileBtn->setCursor(Qt::PointingHandCursor);
    connect(addFileBtn, &QPushButton::clicked, this, &ExcelTool::onAddFiles);

    QPushButton *removeFileBtn = new QPushButton(QStringLiteral("移除选中"), fileGroup);
    removeFileBtn->setCursor(Qt::PointingHandCursor);
    removeFileBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "border-radius: 4px; font-size: 13px; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(removeFileBtn, &QPushButton::clicked, this, &ExcelTool::onRemoveFiles);

    QPushButton *clearFileBtn = new QPushButton(QStringLiteral("清空列表"), fileGroup);
    clearFileBtn->setCursor(Qt::PointingHandCursor);
    clearFileBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "border-radius: 4px; font-size: 13px; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(clearFileBtn, &QPushButton::clicked, this, &ExcelTool::onClearFiles);

    fileBtns->addWidget(addFileBtn);
    fileBtns->addWidget(removeFileBtn);
    fileBtns->addWidget(clearFileBtn);
    fileBtns->addStretch();
    fileLayout->addLayout(fileBtns);

    m_fileList = new QListWidget(fileGroup);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setMinimumHeight(110);
    fileLayout->addWidget(m_fileList);

    QHBoxLayout *dirRow = new QHBoxLayout();
    dirRow->setSpacing(8);

    QLabel *dirLabel = new QLabel(QStringLiteral("输出目录:"), fileGroup);
    m_outputDirEdit = new QLineEdit(fileGroup);
    m_outputDirEdit->setPlaceholderText(QStringLiteral("留空则保存到各文件的同目录"));
    m_outputDirEdit->setClearButtonEnabled(true);

    QPushButton *chooseDirBtn = new QPushButton(QStringLiteral("选择..."), fileGroup);
    chooseDirBtn->setCursor(Qt::PointingHandCursor);
    connect(chooseDirBtn, &QPushButton::clicked, this, &ExcelTool::onChooseOutputDir);

    dirRow->addWidget(dirLabel);
    dirRow->addWidget(m_outputDirEdit, 1);
    dirRow->addWidget(chooseDirBtn);
    fileLayout->addLayout(dirRow);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(16);

    m_statusLabel = new QLabel(fileGroup);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));
    m_statusLabel->setWordWrap(true);

    QPushButton *processBtn = new QPushButton(QStringLiteral("开始处理"), fileGroup);
    processBtn->setFixedHeight(36);
    processBtn->setCursor(Qt::PointingHandCursor);
    processBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #198754; color: #fff; border: none; "
        "border-radius: 6px; font-size: 14px; font-weight: bold; padding: 0 24px; }"
        "QPushButton:hover { background-color: #157347; }"));
    connect(processBtn, &QPushButton::clicked, this, &ExcelTool::onProcess);

    bottomRow->addWidget(m_statusLabel, 1);
    bottomRow->addWidget(processBtn);
    fileLayout->addLayout(bottomRow);

    mainLayout->addWidget(ruleGroup);
    mainLayout->addWidget(fileGroup);

    return page;
}

void ExcelTool::onAddRule()
{
    QTableWidget *table = activeRuleTable();
    if (!table)
        return;
    const int row = table->rowCount();
    table->insertRow(row);
    if (table == m_clearTable) {
        QComboBox *cb = new QComboBox();
        cb->addItems({QStringLiteral("清空列"), QStringLiteral("删除行")});
        cb->setCursor(Qt::PointingHandCursor);
        table->setCellWidget(row, 0, cb);
        for (int c = 1; c < table->columnCount(); ++c) {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            table->setItem(row, c, item);
        }
        table->editItem(table->item(row, 1));
        return;
    }
    for (int c = 0; c < table->columnCount(); ++c) {
        QTableWidgetItem *item = new QTableWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        table->setItem(row, c, item);
    }
    table->editItem(table->item(row, 0));
}

void ExcelTool::onRemoveRules()
{
    QTableWidget *table = activeRuleTable();
    if (!table)
        return;
    QList<int> rows;
    for (const auto *item : table->selectedItems()) {
        if (!rows.contains(item->row()))
            rows.append(item->row());
    }
    if (rows.isEmpty())
        return;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows)
        table->removeRow(r);
}

void ExcelTool::readRules(QList<HeaderReplace> &headers,
                          QList<DataReplace> &data,
                          QList<ClearRule> &clears,
                          QString &warnings) const
{
    headers.clear();
    data.clear();
    clears.clear();
    warnings.clear();

    for (int r = 0; r < m_headerTable->rowCount(); ++r) {
        const QString find = m_headerTable->item(r, 0) ? m_headerTable->item(r, 0)->text().trimmed() : QString();
        if (find.isEmpty())
            continue;
        const QString replace = m_headerTable->item(r, 1) ? m_headerTable->item(r, 1)->text() : QString();
        headers.append(HeaderReplace{find, replace});
    }

    for (int r = 0; r < m_dataTable->rowCount(); ++r) {
        const QString find = m_dataTable->item(r, 0) ? m_dataTable->item(r, 0)->text().trimmed() : QString();
        if (find.isEmpty())
            continue;
        const QString replace = m_dataTable->item(r, 1) ? m_dataTable->item(r, 1)->text() : QString();
        data.append(DataReplace{find, replace});
    }

    for (int r = 0; r < m_clearTable->rowCount(); ++r) {
        ClearRule::Action action = ClearRule::Action::Clear;
        if (const auto *cb = qobject_cast<QComboBox *>(m_clearTable->cellWidget(r, 0))) {
            if (cb->currentIndex() == 1)
                action = ClearRule::Action::DeleteRow;
        }
        const QString col = m_clearTable->item(r, 1) ? m_clearTable->item(r, 1)->text().trimmed() : QString();
        if (col.isEmpty())
            continue;
        const QString value = m_clearTable->item(r, 2) ? m_clearTable->item(r, 2)->text() : QString();
        const QString extraStr = m_clearTable->item(r, 3) ? m_clearTable->item(r, 3)->text().trimmed() : QString();
        const QString clearStr = m_clearTable->item(r, 4) ? m_clearTable->item(r, 4)->text().trimmed() : QString();

        if (action == ClearRule::Action::Clear && clearStr.isEmpty()) {
            warnings += QStringLiteral("条件列 %1 未指定清空列，已跳过\n").arg(col);
            continue;
        }
        if (Xlsx::columnFromLetter(col) == 0) {
            warnings += QStringLiteral("条件列 %1 不是有效列名，已跳过\n").arg(col);
            continue;
        }
        QStringList clearLetters;
        if (action == ClearRule::Action::Clear) {
            bool ok = false;
            const QList<int> cols = parseClearColumns(clearStr, &ok);
            if (!ok || cols.isEmpty()) {
                warnings += QStringLiteral("条件「%1=%2」的清空列 %3 无效，已跳过\n").arg(col, value, clearStr);
                continue;
            }
            for (int c : cols)
                clearLetters.append(Xlsx::columnLetter(c));
        }
        QList<QList<ClearCondition>> extraGroups;
        QString extraErr;
        if (!extraStr.isEmpty() && !parseExtraConditions(extraStr, &extraGroups, &extraErr)) {
            warnings += QStringLiteral("条件「%1=%2」的附加条件 %3 无效（%4），已跳过\n")
                .arg(col, value, extraStr, extraErr);
            continue;
        }
        clears.append(ClearRule{action, col.toUpper(), value, extraGroups, clearLetters});
    }
}

void ExcelTool::loadRulesToTables(const QList<HeaderReplace> &headers,
                                  const QList<DataReplace> &data,
                                  const QList<ClearRule> &clears)
{
    auto fill2 = [](QTableWidget *table, const QStringList &cols) {
        table->setRowCount(0);
        for (const QString &row : cols) {
            const int r = table->rowCount();
            table->insertRow(r);
            QString find = row.section(QLatin1Char('\t'), 0, 0);
            QString rep = row.section(QLatin1Char('\t'), 1, 1);
            QTableWidgetItem *fi = new QTableWidgetItem(find);
            QTableWidgetItem *ri = new QTableWidgetItem(rep);
            fi->setFlags(fi->flags() | Qt::ItemIsEditable);
            ri->setFlags(ri->flags() | Qt::ItemIsEditable);
            table->setItem(r, 0, fi);
            table->setItem(r, 1, ri);
        }
    };

    QStringList hRows;
    for (const HeaderReplace &hr : headers)
        hRows.append(hr.find + QLatin1Char('\t') + hr.replace);
    fill2(m_headerTable, hRows);

    QStringList dRows;
    for (const DataReplace &dr : data)
        dRows.append(dr.find + QLatin1Char('\t') + dr.replace);
    fill2(m_dataTable, dRows);

    m_clearTable->setRowCount(0);
    for (const ClearRule &cr : clears) {
        const int r = m_clearTable->rowCount();
        m_clearTable->insertRow(r);
        QComboBox *cb = new QComboBox();
        cb->addItems({QStringLiteral("清空列"), QStringLiteral("删除行")});
        cb->setCursor(Qt::PointingHandCursor);
        cb->setCurrentIndex(cr.action == ClearRule::Action::DeleteRow ? 1 : 0);
        m_clearTable->setCellWidget(r, 0, cb);
        QTableWidgetItem *ci = new QTableWidgetItem(cr.column);
        QTableWidgetItem *vi = new QTableWidgetItem(cr.value);
        QTableWidgetItem *ei = new QTableWidgetItem(serializeExtraConditions(cr.extraGroups));
        QTableWidgetItem *li = new QTableWidgetItem(cr.clearColumns.join(QLatin1String(",")));
        for (QTableWidgetItem *item : {ci, vi, ei, li})
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_clearTable->setItem(r, 1, ci);
        m_clearTable->setItem(r, 2, vi);
        m_clearTable->setItem(r, 3, ei);
        m_clearTable->setItem(r, 4, li);
    }
}

void ExcelTool::onExportRules()
{
    QList<HeaderReplace> headers;
    QList<DataReplace> data;
    QList<ClearRule> clears;
    QString warnings;
    readRules(headers, data, clears, warnings);

    QJsonArray headerArr;
    for (const HeaderReplace &hr : headers) {
        QJsonObject o;
        o.insert(QStringLiteral("find"), hr.find);
        o.insert(QStringLiteral("replace"), hr.replace);
        headerArr.append(o);
    }
    QJsonArray dataArr;
    for (const DataReplace &dr : data) {
        QJsonObject o;
        o.insert(QStringLiteral("find"), dr.find);
        o.insert(QStringLiteral("replace"), dr.replace);
        dataArr.append(o);
    }
    QJsonArray clearArr;
    for (const ClearRule &cr : clears) {
        QJsonObject o;
        o.insert(QStringLiteral("column"), cr.column);
        o.insert(QStringLiteral("value"), cr.value);
        o.insert(QStringLiteral("action"),
                 cr.action == ClearRule::Action::DeleteRow ? QStringLiteral("delete") : QStringLiteral("clear"));
        if (!cr.extraGroups.isEmpty()) {
            QJsonArray extraList;
            for (const auto &group : cr.extraGroups) {
                QJsonArray groupList;
                for (const ClearCondition &cond : group) {
                    QJsonObject c;
                    c.insert(QStringLiteral("column"), cond.column);
                    QString opStr;
                    switch (cond.op) {
                    case ClearCondition::Op::Eq: opStr = QStringLiteral("eq"); break;
                    case ClearCondition::Op::Ne: opStr = QStringLiteral("ne"); break;
                    case ClearCondition::Op::Empty: opStr = QStringLiteral("empty"); break;
                    case ClearCondition::Op::NotEmpty: opStr = QStringLiteral("notempty"); break;
                    }
                    c.insert(QStringLiteral("op"), opStr);
                    if (cond.op == ClearCondition::Op::Eq || cond.op == ClearCondition::Op::Ne)
                        c.insert(QStringLiteral("value"), cond.value);
                    groupList.append(c);
                }
                extraList.append(groupList);
            }
            o.insert(QStringLiteral("extra"), extraList);
        }
        QJsonArray clearList;
        for (const QString &c : cr.clearColumns)
            clearList.append(c);
        o.insert(QStringLiteral("clear"), clearList);
        clearArr.append(o);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("headerReplace"), headerArr);
    root.insert(QStringLiteral("dataReplace"), dataArr);
    root.insert(QStringLiteral("clearRules"), clearArr);

    const QString filePath = QFileDialog::getSaveFileName(m_fileList,
        QStringLiteral("导出规则"), QStringLiteral("excel_rules.json"),
        QStringLiteral("规则文件 (*.json)"));
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(m_fileList, QStringLiteral("导出失败"),
            QStringLiteral("无法写入文件：%1").arg(file.errorString()));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    updateStatus(QStringLiteral("规则已导出 → %1").arg(filePath), false);
}

void ExcelTool::onImportRules()
{
    const QString filePath = QFileDialog::getOpenFileName(m_fileList,
        QStringLiteral("导入规则"), QString(),
        QStringLiteral("规则文件 (*.json)"));
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(m_fileList, QStringLiteral("导入失败"),
            QStringLiteral("无法读取文件：%1").arg(file.errorString()));
        return;
    }
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &perr);
    file.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(m_fileList, QStringLiteral("导入失败"),
            QStringLiteral("规则文件不是有效的 JSON。"));
        return;
    }

    const QJsonObject root = doc.object();
    QList<HeaderReplace> headers;
    QList<DataReplace> data;
    QList<ClearRule> clears;

    const QJsonArray headerArr = root.value(QStringLiteral("headerReplace")).toArray();
    for (const QJsonValue &v : headerArr) {
        const QJsonObject o = v.toObject();
        headers.append(HeaderReplace{o.value(QStringLiteral("find")).toString(),
                                     o.value(QStringLiteral("replace")).toString()});
    }
    const QJsonArray dataArr = root.value(QStringLiteral("dataReplace")).toArray();
    for (const QJsonValue &v : dataArr) {
        const QJsonObject o = v.toObject();
        data.append(DataReplace{o.value(QStringLiteral("find")).toString(),
                                o.value(QStringLiteral("replace")).toString()});
    }
    const QJsonArray clearArr = root.value(QStringLiteral("clearRules")).toArray();
    for (const QJsonValue &v : clearArr) {
        const QJsonObject o = v.toObject();
        QStringList clearCols;
        const QJsonArray clearList = o.value(QStringLiteral("clear")).toArray();
        for (const QJsonValue &c : clearList)
            clearCols.append(c.toString());
        QList<QList<ClearCondition>> extraGroups;
        const QJsonArray extraList = o.value(QStringLiteral("extra")).toArray();
        for (const QJsonValue &g : extraList) {
            QList<ClearCondition> group;
            const QJsonArray groupList = g.toArray();
            for (const QJsonValue &cv : groupList) {
                const QJsonObject c = cv.toObject();
                ClearCondition cond;
                cond.column = c.value(QStringLiteral("column")).toString();
                const QString opStr = c.value(QStringLiteral("op")).toString();
                if (opStr == QLatin1String("notempty"))
                    cond.op = ClearCondition::Op::NotEmpty;
                else if (opStr == QLatin1String("empty"))
                    cond.op = ClearCondition::Op::Empty;
                else if (opStr == QLatin1String("ne"))
                    cond.op = ClearCondition::Op::Ne;
                else
                    cond.op = ClearCondition::Op::Eq;
                cond.value = c.value(QStringLiteral("value")).toString();
                group.append(cond);
            }
            if (!group.isEmpty())
                extraGroups.append(group);
        }
        ClearRule cr;
        cr.column = o.value(QStringLiteral("column")).toString();
        cr.value = o.value(QStringLiteral("value")).toString();
        cr.extraGroups = extraGroups;
        cr.clearColumns = clearCols;
        cr.action = (o.value(QStringLiteral("action")).toString() == QLatin1String("delete"))
            ? ClearRule::Action::DeleteRow : ClearRule::Action::Clear;
        clears.append(cr);
    }

    loadRulesToTables(headers, data, clears);
    updateStatus(QStringLiteral("已导入规则：表头 %1 条 / 数据 %2 条 / 清空 %3 条")
        .arg(headers.size()).arg(data.size()).arg(clears.size()), false);
}

void ExcelTool::onClearRules()
{
    m_headerTable->setRowCount(0);
    m_dataTable->setRowCount(0);
    m_clearTable->setRowCount(0);
    updateStatus(QStringLiteral("规则已清空"), false);
}

void ExcelTool::onShowSyntaxHelp()
{
    QMessageBox box(QMessageBox::Information, QStringLiteral("语法帮助"),
        QStringLiteral(
            "<b>附加条件</b>（条件清空表第 4 列，选填）："
            "<span style=\"font-family:Consolas,monospace\">;|=|</span>"
            "<br><br>"
            "<b>原子条件</b>（列名 + <span style=\"font-family:Consolas,monospace\">=</span> 或 "
            "<span style=\"font-family:Consolas,monospace\">!=</span> + 值）：<br>"
            "&nbsp;&nbsp;<span style=\"font-family:Consolas,monospace\">B=值</span>　B 列等于 值<br>"
            "&nbsp;&nbsp;<span style=\"font-family:Consolas,monospace\">B!=值</span>　B 列不等于 值<br>"
            "&nbsp;&nbsp;<span style=\"font-family:Consolas,monospace\">B=</span>&nbsp;&nbsp;&nbsp;　B 列为空（等号后留空）<br>"
            "&nbsp;&nbsp;<span style=\"font-family:Consolas,monospace\">B!=</span>&nbsp;&nbsp;　B 列非空<br><br>"
            "<b>组合规则：</b><br>"
            "&nbsp;&nbsp;<span style=\"font-family:Consolas,monospace\">|　</span>　或者（同组内命中其一即可）<br>"
            "&nbsp;&nbsp;<span style=\"font-family:Consolas,monospace\">;　</span>　并且（各组之间都要满足）<br><br>"
            "<b>示例</b>（条件列 A、等于值 xxx）：<br>"
            "&nbsp;&nbsp;附加条件 <span style=\"font-family:Consolas,monospace\">B=|C=|D=</span>　→ A=xxx 且（B 或 C 或 D 任一为空）<br>"
            "&nbsp;&nbsp;附加条件 <span style=\"font-family:Consolas,monospace\">B=在职;E!=1</span>　→ A=xxx 且 B=在职 且 E≠1<br><br>"
            "<b>清空列</b>格式：<span style=\"font-family:Consolas,monospace\">D,E,F</span> 或 "
            "<span style=\"font-family:Consolas,monospace\">D:F</span>（仅「清空列」动作使用）<br><br>"
            "<b>动作</b>：清空列 = 命中后清空指定列；删除行 = 整行删除并上移（第 1 行表头永不删除）"),
        QMessageBox::NoButton, m_fileList);
    box.addButton(QStringLiteral("知道了"), QMessageBox::AcceptRole);
    box.exec();
}

void ExcelTool::addFiles(const QStringList &paths)
{
    for (const QString &p : paths) {
        bool dup = false;
        for (int i = 0; i < m_fileList->count(); ++i) {
            if (m_fileList->item(i)->data(Qt::UserRole).toString() == p) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            QListWidgetItem *item = new QListWidgetItem(p);
            item->setData(Qt::UserRole, p);
            m_fileList->addItem(item);
        }
    }
    if (m_fileList->count() > 0)
        updateStatus(QStringLiteral("已添加 %1 个文件").arg(m_fileList->count()), false);
}

void ExcelTool::onAddFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(m_fileList,
        QStringLiteral("选择 Excel 文件"), QString(),
        QStringLiteral("Excel 文件 (*.xlsx)"));
    addFiles(paths);
}

void ExcelTool::onRemoveFiles()
{
    QList<int> rows;
    for (const auto *item : m_fileList->selectedItems())
        rows.append(m_fileList->row(item));
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows)
        delete m_fileList->takeItem(r);
    updateStatus(QStringLiteral("当前 %1 个文件").arg(m_fileList->count()), false);
}

void ExcelTool::onClearFiles()
{
    m_fileList->clear();
    updateStatus(QString(), false);
}

void ExcelTool::onChooseOutputDir()
{
    const QString dir = QFileDialog::getExistingDirectory(m_fileList,
        QStringLiteral("选择输出目录"), m_outputDirEdit->text());
    if (!dir.isEmpty())
        m_outputDirEdit->setText(dir);
}

bool ExcelTool::processOne(const QString &src, const QString &outDir,
                           const QList<HeaderReplace> &headers,
                           const QList<DataReplace> &data,
                           const QList<ClearRule> &clears,
                           QString *outPath, QString *err)
{
    Xlsx::Workbook wb;
    QString rerr;
    if (!Xlsx::readWorkbook(src, &wb, &rerr)) {
        if (err)
            *err = QStringLiteral("读取失败：%1").arg(rerr);
        return false;
    }

    Xlsx::Sheet &sheet = wb.sheets[0];

    // 1. 表头精确替换（第 1 行）
    for (int col = 1; col <= sheet.maxCol; ++col) {
        QString text = Xlsx::cellText(sheet, 1, col);
        if (text.isEmpty())
            continue;
        for (const HeaderReplace &hr : headers) {
            if (hr.find.isEmpty())
                continue;
            if (text == hr.find)
                text = hr.replace;
        }
        Xlsx::setCellText(&sheet, 1, col, text);
    }

    // 2. 数据子串替换（第 2 行起，仅文本单元格）
    if (!data.isEmpty()) {
        QVector<int> keys;
        for (auto it = sheet.cells.constBegin(); it != sheet.cells.constEnd(); ++it) {
            if (it->kind == Xlsx::Cell::Text && it.key() / 65536 >= 2)
                keys.append(it.key());
        }
        for (int key : keys) {
            const Xlsx::Cell &cell = sheet.cells.value(key);
            if (cell.kind != Xlsx::Cell::Text)
                continue;
            const int row = key / 65536;
            const int col = key % 65536;
            QString text = cell.text;
            for (const DataReplace &dr : data) {
                if (dr.find.isEmpty())
                    continue;
                if (text.contains(dr.find))
                    text.replace(dr.find, dr.replace);
            }
            if (text != cell.text)
                Xlsx::setCellText(&sheet, row, col, text);
        }
    }

    // 3. 条件清空 / 删除行
    if (!clears.isEmpty()) {
        QList<int> delRows;
        const int ruleMaxRow = sheet.maxRow;
        for (const ClearRule &cr : clears) {
            const int condCol = Xlsx::columnFromLetter(cr.column);
            if (condCol <= 0)
                continue;
            QList<int> clearCols;
            for (const QString &cl : cr.clearColumns) {
                const int c = Xlsx::columnFromLetter(cl);
                if (c > 0)
                    clearCols.append(c);
            }
            for (int row = 2; row <= ruleMaxRow; ++row) {
                if (Xlsx::cellText(sheet, row, condCol) != cr.value)
                    continue;
                bool match = true;
                for (const auto &group : cr.extraGroups) {
                    bool groupOk = false;
                    for (const ClearCondition &cond : group) {
                        if (evalExtraCondition(sheet, row, cond)) {
                            groupOk = true;
                            break;
                        }
                    }
                    if (!groupOk) {
                        match = false;
                        break;
                    }
                }
                if (!match)
                    continue;
                if (cr.action == ClearRule::Action::DeleteRow) {
                    if (!delRows.contains(row))
                        delRows.append(row);
                } else {
                    for (int cc : clearCols)
                        Xlsx::setCellEmpty(&sheet, row, cc);
                }
            }
        }
        if (!delRows.isEmpty())
            Xlsx::deleteRows(&sheet, delRows);
    }

    const QString base = QFileInfo(src).completeBaseName();
    const QString dir = outDir.isEmpty() ? QFileInfo(src).absolutePath() : outDir;
    const QString output = QDir(dir).filePath(base + QStringLiteral("_batch.xlsx"));

    QString werr;
    if (!Xlsx::writeWorkbook(output, wb, &werr)) {
        if (err)
            *err = QStringLiteral("写入失败：%1").arg(werr);
        return false;
    }
    if (outPath)
        *outPath = output;
    return true;
}

void ExcelTool::onProcess()
{
    if (m_processing)
        return;
    if (m_fileList->count() == 0) {
        QMessageBox::warning(m_fileList, QStringLiteral("提示"),
            QStringLiteral("请先添加要处理的 Excel 文件。"));
        return;
    }

    QList<HeaderReplace> headers;
    QList<DataReplace> data;
    QList<ClearRule> clears;
    QString warnings;
    readRules(headers, data, clears, warnings);
    if (headers.isEmpty() && data.isEmpty() && clears.isEmpty()) {
        QMessageBox::warning(m_fileList, QStringLiteral("提示"),
            QStringLiteral("请先配置处理规则。"));
        return;
    }

    const QString outDir = m_outputDirEdit->text().trimmed();

    QMessageBox box(QMessageBox::Question, QStringLiteral("确认处理"),
        QStringLiteral(
            "注意：仅处理每个文件的第一个工作表，原文件不会被修改。\n\n"
            "输出目录：%1\n文件数量：%2 个\n\n确定开始处理？")
            .arg(outDir.isEmpty() ? QStringLiteral("（每个文件的同目录）") : outDir)
            .arg(m_fileList->count()),
        QMessageBox::NoButton, m_fileList);
    QPushButton *okBtn = box.addButton(QStringLiteral("开始处理"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != okBtn)
        return;

    if (!outDir.isEmpty() && !QDir().mkpath(outDir)) {
        QMessageBox::warning(m_fileList, QStringLiteral("提示"),
            QStringLiteral("无法创建输出目录：%1").arg(outDir));
        return;
    }

    m_processing = true;
    QApplication::setOverrideCursor(Qt::WaitCursor);

    int okCount = 0, failCount = 0;
    QStringList outputs;
    QStringList errorLines;

    for (int i = 0; i < m_fileList->count(); ++i) {
        const QString src = m_fileList->item(i)->data(Qt::UserRole).toString();
        if (src.isEmpty()) {
            ++failCount;
            errorLines.append(m_fileList->item(i)->text() + QStringLiteral("：无法获取源文件路径"));
            continue;
        }
        m_statusLabel->setText(QStringLiteral("正在处理: %1").arg(QFileInfo(src).fileName()));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #0d6efd; font-size: 13px;"));
        QApplication::processEvents();

        QString outPath;
        QString err;
        QListWidgetItem *item = m_fileList->item(i);
        const QString base = QFileInfo(src).fileName();
        if (processOne(src, outDir, headers, data, clears, &outPath, &err)) {
            ++okCount;
            outputs.append(outPath);
            item->setText(QStringLiteral("\u2714 ") + base);
            item->setForeground(QColor(QStringLiteral("#198754")));
            item->setToolTip(outPath);
        } else {
            ++failCount;
            errorLines.append(base + QStringLiteral("：") + err);
            item->setText(QStringLiteral("\u2718 ") + base);
            item->setForeground(QColor(QStringLiteral("#dc3545")));
            item->setToolTip(err);
        }
    }

    m_processing = false;
    QApplication::restoreOverrideCursor();

    QString result = QStringLiteral("处理完成\n\n成功 %1 个，失败 %2 个\n").arg(okCount).arg(failCount);
    if (!warnings.isEmpty())
        result += QStringLiteral("\n规则提示：\n%1").arg(warnings.trimmed());
    if (!errorLines.isEmpty())
        result += QStringLiteral("\n失败详情：\n%1").arg(errorLines.join(QLatin1Char('\n')));
    if (!outputs.isEmpty())
        result += QStringLiteral("\n输出文件：\n%1").arg(outputs.join(QLatin1Char('\n')));

    updateStatus(QStringLiteral("处理完成：成功 %1 / 失败 %2").arg(okCount).arg(failCount), failCount > 0);

    QMessageBox::information(m_fileList, QStringLiteral("处理完成"), result);
}