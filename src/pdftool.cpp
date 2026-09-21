#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QRadioButton>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>

#include "pdftool.h"
#include "utils.h"

namespace {

const char *kPdfFilter = "PDF 文件 (*.pdf)";

QHBoxLayout *labeledRow(const QString &text, QWidget *field, QWidget *parent, int labelWidth = 96)
{
    QHBoxLayout *row = new QHBoxLayout();
    QLabel *label = new QLabel(text, parent);
    label->setMinimumWidth(labelWidth);
    row->addWidget(label);
    row->addWidget(field, 1);
    return row;
}

QString readablePdfDate(const QString &raw)
{
    // PDF 日期格式: D:YYYYMMDDHHmmSS(+|-)HH'mm' 或 D:YYYYMMDDHHmmSSZ
    QRegularExpression re(QStringLiteral("D:(\\d{4})(\\d{2})?(\\d{2})?(\\d{2})?(\\d{2})?(\\d{2})?"));
    auto m = re.match(raw);
    if (!m.hasMatch())
        return raw;
    const QString y = m.captured(1);
    const QString mo = m.captured(2).isEmpty() ? QStringLiteral("01") : m.captured(2);
    const QString d = m.captured(3).isEmpty() ? QStringLiteral("01") : m.captured(3);
    const QString h = m.captured(4).isEmpty() ? QStringLiteral("00") : m.captured(4);
    const QString mi = m.captured(5).isEmpty() ? QStringLiteral("00") : m.captured(5);
    const QString s = m.captured(6).isEmpty() ? QStringLiteral("00") : m.captured(6);
    return QStringLiteral("%1-%2-%3 %4:%5:%6").arg(y, mo, d, h, mi, s);
}

} // namespace

PdfTool::PdfTool(const QString &qpdfPath, QObject *parent)
    : QObject(parent)
    , m_qpdfPath(qpdfPath)
    , m_available(QFileInfo::exists(qpdfPath))
{
}

QWidget *PdfTool::createPage()
{
    m_page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_page);
    layout->setSpacing(8);

    if (!m_available) {
        QLabel *warn = new QLabel(
            QStringLiteral("未找到 qpdf.exe，PDF 功能不可用。请将 qpdf.exe 与 qpdf30.dll 放到程序同级目录。"),
            m_page);
        warn->setWordWrap(true);
        warn->setStyleSheet(QStringLiteral(
            "background:#f8d7da;color:#842029;border:1px solid #f5c2c7;border-radius:4px;padding:8px;"));
        layout->addWidget(warn);
    }

    m_tabs = new QTabWidget(m_page);
    buildMergeTab(m_tabs);
    buildSplitTab(m_tabs);
    buildOrganizeTab(m_tabs);
    buildEncryptTab(m_tabs);
    buildOptimizeTab(m_tabs);
    buildInfoTab(m_tabs);
    layout->addWidget(m_tabs, 1);

    m_statusLabel = new QLabel(QStringLiteral("就绪"), m_page);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#6c757d;"));
    layout->addWidget(m_statusLabel);

    m_log = new QTextEdit(m_page);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(80);
    m_log->setStyleSheet(QStringLiteral(
        "QTextEdit { font-family:'Consolas','Courier New',monospace; font-size:12px;"
        " border:1px solid #ced4da; border-radius:6px; padding:6px; background:#ffffff; color:#212529; }"));
    layout->addWidget(m_log);

    m_stopBtn = new QPushButton(QStringLiteral("停止"), m_page);
    m_stopBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &PdfTool::onStop);
    QHBoxLayout *stopRow = new QHBoxLayout();
    stopRow->addStretch();
    stopRow->addWidget(m_stopBtn);
    layout->addLayout(stopRow);

    setBusy(false);
    return m_page;
}

void PdfTool::buildMergeTab(QTabWidget *tabs)
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel(QStringLiteral("待合并的 PDF（从上到下依次拼接，可拖动调整顺序）:"), page));
    m_mergeList = new QListWidget(page);
    m_mergeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_mergeList->setDragDropMode(QAbstractItemView::InternalMove);
    m_mergeList->setDefaultDropAction(Qt::MoveAction);
    layout->addWidget(m_mergeList, 1);

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QStringLiteral("添加文件"), page);
    QPushButton *removeBtn = new QPushButton(QStringLiteral("移除选中"), page);
    QPushButton *clearBtn = new QPushButton(QStringLiteral("清空"), page);
    QPushButton *upBtn = new QPushButton(QStringLiteral("上移"), page);
    QPushButton *downBtn = new QPushButton(QStringLiteral("下移"), page);
    btnRow->addWidget(addBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addWidget(clearBtn);
    btnRow->addWidget(upBtn);
    btnRow->addWidget(downBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(addBtn, &QPushButton::clicked, this, &PdfTool::onMergeAdd);
    connect(removeBtn, &QPushButton::clicked, this, &PdfTool::onMergeRemove);
    connect(clearBtn, &QPushButton::clicked, this, &PdfTool::onMergeClear);
    connect(upBtn, &QPushButton::clicked, this, &PdfTool::onMergeMoveUp);
    connect(downBtn, &QPushButton::clicked, this, &PdfTool::onMergeMoveDown);

    m_mergePassword = new QLineEdit(page);
    m_mergePassword->setEchoMode(QLineEdit::Password);
    m_mergePassword->setPlaceholderText(QStringLiteral("可留空；若源文件已加密则填写打开密码"));
    layout->addLayout(labeledRow(QStringLiteral("打开密码:"), m_mergePassword, page));

    m_mergeOut = new QLineEdit(page);
    m_mergeOut->setPlaceholderText(QStringLiteral("留空则输出到首个文件同目录 <名称>_merged.pdf"));
    QPushButton *outBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *outRow = labeledRow(QStringLiteral("输出文件:"), m_mergeOut, page);
    outRow->addWidget(outBtn);
    layout->addLayout(outRow);
    connect(outBtn, &QPushButton::clicked, this, [this]() {
        pickSavePdf(m_mergeOut, m_mergeList->count() > 0
                        ? defaultOutPath(m_mergeList->item(0)->text(), QStringLiteral("_merged"))
                        : QStringLiteral("merged.pdf"));
    });

    QPushButton *runBtn = new QPushButton(QStringLiteral("合并 PDF"), page);
    QHBoxLayout *runRow = new QHBoxLayout();
    runRow->addStretch();
    runRow->addWidget(runBtn);
    layout->addLayout(runRow);
    connect(runBtn, &QPushButton::clicked, this, &PdfTool::onMergeRun);
    registerActionButton(runBtn);

    tabs->addTab(page, QStringLiteral("合并"));
}

void PdfTool::buildSplitTab(QTabWidget *tabs)
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    m_splitIn = new QLineEdit(page);
    QPushButton *inBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *inRow = labeledRow(QStringLiteral("源 PDF:"), m_splitIn, page);
    inRow->addWidget(inBtn);
    layout->addLayout(inRow);
    connect(inBtn, &QPushButton::clicked, this, &PdfTool::onSplitBrowseIn);

    m_splitMode = new QComboBox(page);
    m_splitMode->addItem(QStringLiteral("提取指定页到单个文件"));
    m_splitMode->addItem(QStringLiteral("每页拆分为一个文件"));
    m_splitMode->addItem(QStringLiteral("每 N 页拆分为一个文件"));
    layout->addLayout(labeledRow(QStringLiteral("拆分模式:"), m_splitMode, page));

    m_splitRange = new QLineEdit(page);
    m_splitRange->setPlaceholderText(QStringLiteral("例: 1-3,5,7-z（z 表示最后一页）"));
    layout->addLayout(labeledRow(QStringLiteral("页码范围:"), m_splitRange, page));

    m_splitEvery = new QSpinBox(page);
    m_splitEvery->setRange(1, 1000);
    m_splitEvery->setValue(2);
    layout->addLayout(labeledRow(QStringLiteral("每 N 页:"), m_splitEvery, page));

    m_splitPassword = new QLineEdit(page);
    m_splitPassword->setEchoMode(QLineEdit::Password);
    m_splitPassword->setPlaceholderText(QStringLiteral("可留空；若源文件已加密则填写"));
    layout->addLayout(labeledRow(QStringLiteral("打开密码:"), m_splitPassword, page));

    m_splitOut = new QLineEdit(page);
    QPushButton *outBtn = new QPushButton(QStringLiteral("浏览"), page);
    m_splitOutLabel = new QLabel(QStringLiteral("输出文件:"), page);
    m_splitOutLabel->setMinimumWidth(96);
    QHBoxLayout *outRow = new QHBoxLayout();
    outRow->addWidget(m_splitOutLabel);
    outRow->addWidget(m_splitOut, 1);
    outRow->addWidget(outBtn);
    layout->addLayout(outRow);
    connect(outBtn, &QPushButton::clicked, this, &PdfTool::onSplitBrowseOut);
    connect(m_splitMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PdfTool::onSplitModeChanged);

    QPushButton *runBtn = new QPushButton(QStringLiteral("开始拆分"), page);
    QHBoxLayout *runRow = new QHBoxLayout();
    runRow->addStretch();
    runRow->addWidget(runBtn);
    layout->addLayout(runRow);
    connect(runBtn, &QPushButton::clicked, this, &PdfTool::onSplitRun);
    registerActionButton(runBtn);

    tabs->addTab(page, QStringLiteral("拆分 / 提取"));
    onSplitModeChanged();
}

void PdfTool::buildOrganizeTab(QTabWidget *tabs)
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    m_orgIn = new QLineEdit(page);
    QPushButton *inBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *inRow = labeledRow(QStringLiteral("源 PDF:"), m_orgIn, page);
    inRow->addWidget(inBtn);
    layout->addLayout(inRow);
    connect(inBtn, &QPushButton::clicked, this, &PdfTool::onOrgBrowseIn);

    m_orgOrder = new QLineEdit(page);
    m_orgOrder->setPlaceholderText(QStringLiteral("例: 3,1,2,5-z  （删除 4；留空表示保持原页序）"));
    layout->addLayout(labeledRow(QStringLiteral("页序表达式:"), m_orgOrder, page));

    m_orgRotate = new QCheckBox(QStringLiteral("旋转页面"), page);
    m_orgAngle = new QComboBox(page);
    m_orgAngle->addItem(QStringLiteral("+90（顺时针 90°）"), QStringLiteral("+90"));
    m_orgAngle->addItem(QStringLiteral("+180（180°）"), QStringLiteral("+180"));
    m_orgAngle->addItem(QStringLiteral("-90（逆时针 90°）"), QStringLiteral("-90"));
    m_orgAngle->addItem(QStringLiteral("90（设为 90°）"), QStringLiteral("90"));
    m_orgAngle->addItem(QStringLiteral("180（设为 180°）"), QStringLiteral("180"));
    m_orgAngle->addItem(QStringLiteral("270（设为 270°）"), QStringLiteral("270"));
    QHBoxLayout *rotRow = new QHBoxLayout();
    rotRow->addWidget(m_orgRotate);
    rotRow->addWidget(m_orgAngle);
    rotRow->addStretch();
    layout->addLayout(rotRow);

    m_orgRotRange = new QLineEdit(page);
    m_orgRotRange->setPlaceholderText(QStringLiteral("留空 = 全部页面；例: 1,3-5"));
    layout->addLayout(labeledRow(QStringLiteral("旋转页码范围:"), m_orgRotRange, page));

    m_orgPassword = new QLineEdit(page);
    m_orgPassword->setEchoMode(QLineEdit::Password);
    m_orgPassword->setPlaceholderText(QStringLiteral("可留空；若源文件已加密则填写"));
    layout->addLayout(labeledRow(QStringLiteral("打开密码:"), m_orgPassword, page));

    m_orgOut = new QLineEdit(page);
    m_orgOut->setPlaceholderText(QStringLiteral("留空则输出到源文件同目录 <名称>_organized.pdf"));
    QPushButton *outBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *outRow = labeledRow(QStringLiteral("输出文件:"), m_orgOut, page);
    outRow->addWidget(outBtn);
    layout->addLayout(outRow);
    connect(outBtn, &QPushButton::clicked, this, &PdfTool::onOrgBrowseOut);

    QLabel *hint = new QLabel(
        QStringLiteral("提示：页序表达式可删除、重排页面（如 3,1,2 删除其余页）；旋转为相对原方向，"
                       "扫描件建议用“+90/+180/-90”。"),
        page);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#6c757d;"));
    layout->addWidget(hint);

    QPushButton *runBtn = new QPushButton(QStringLiteral("执行编排"), page);
    QHBoxLayout *runRow = new QHBoxLayout();
    runRow->addStretch();
    runRow->addWidget(runBtn);
    layout->addLayout(runRow);
    connect(runBtn, &QPushButton::clicked, this, &PdfTool::onOrgRun);
    registerActionButton(runBtn);

    tabs->addTab(page, QStringLiteral("页面编排"));
}

void PdfTool::buildEncryptTab(QTabWidget *tabs)
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QHBoxLayout *modeRow = new QHBoxLayout();
    m_encModeEncrypt = new QRadioButton(QStringLiteral("加密"), page);
    m_encModeDecrypt = new QRadioButton(QStringLiteral("解密"), page);
    m_encModeEncrypt->setChecked(true);
    modeRow->addWidget(m_encModeEncrypt);
    modeRow->addWidget(m_encModeDecrypt);
    modeRow->addStretch();
    layout->addLayout(modeRow);
    connect(m_encModeEncrypt, &QRadioButton::toggled, this, &PdfTool::onEncModeChanged);

    m_encIn = new QLineEdit(page);
    QPushButton *inBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *inRow = labeledRow(QStringLiteral("源 PDF:"), m_encIn, page);
    inRow->addWidget(inBtn);
    layout->addLayout(inRow);
    connect(inBtn, &QPushButton::clicked, this, &PdfTool::onEncBrowseIn);

    m_encUser = new QLineEdit(page);
    m_encUser->setEchoMode(QLineEdit::Password);
    m_encUser->setPlaceholderText(QStringLiteral("打开文件所需密码，可留空（留空则无需密码即可打开）"));
    layout->addLayout(labeledRow(QStringLiteral("用户密码:"), m_encUser, page));

    m_encOwner = new QLineEdit(page);
    m_encOwner->setEchoMode(QLineEdit::Password);
    m_encOwner->setPlaceholderText(QStringLiteral("用于修改权限，建议设置"));
    layout->addLayout(labeledRow(QStringLiteral("所有者密码:"), m_encOwner, page));

    m_encPassword = new QLineEdit(page);
    m_encPassword->setEchoMode(QLineEdit::Password);
    m_encPassword->setPlaceholderText(QStringLiteral("解密时输入源文件的打开密码"));
    layout->addLayout(labeledRow(QStringLiteral("打开密码:"), m_encPassword, page));

    QHBoxLayout *permRow = new QHBoxLayout();
    m_encAllowPrint = new QCheckBox(QStringLiteral("允许打印"), page);
    m_encAllowCopy = new QCheckBox(QStringLiteral("允许复制文本"), page);
    m_encAllowModify = new QCheckBox(QStringLiteral("允许修改"), page);
    permRow->addWidget(new QLabel(QStringLiteral("权限:"), page));
    permRow->addWidget(m_encAllowPrint);
    permRow->addWidget(m_encAllowCopy);
    permRow->addWidget(m_encAllowModify);
    permRow->addStretch();
    layout->addLayout(permRow);

    m_encHint = new QLabel(page);
    m_encHint->setWordWrap(true);
    m_encHint->setStyleSheet(QStringLiteral("color:#6c757d;"));
    layout->addWidget(m_encHint);

    m_encOut = new QLineEdit(page);
    m_encOut->setPlaceholderText(QStringLiteral("留空则输出到源文件同目录"));
    QPushButton *outBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *outRow = labeledRow(QStringLiteral("输出文件:"), m_encOut, page);
    outRow->addWidget(outBtn);
    layout->addLayout(outRow);
    connect(outBtn, &QPushButton::clicked, this, &PdfTool::onEncBrowseOut);

    QPushButton *runBtn = new QPushButton(QStringLiteral("执行"), page);
    QHBoxLayout *runRow = new QHBoxLayout();
    runRow->addStretch();
    runRow->addWidget(runBtn);
    layout->addLayout(runRow);
    connect(runBtn, &QPushButton::clicked, this, &PdfTool::onEncRun);
    registerActionButton(runBtn);

    tabs->addTab(page, QStringLiteral("加密 / 解密"));
    onEncModeChanged();
}

void PdfTool::buildOptimizeTab(QTabWidget *tabs)
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    m_optIn = new QLineEdit(page);
    QPushButton *inBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *inRow = labeledRow(QStringLiteral("源 PDF:"), m_optIn, page);
    inRow->addWidget(inBtn);
    layout->addLayout(inRow);
    connect(inBtn, &QPushButton::clicked, this, &PdfTool::onOptBrowseIn);

    m_optLinearize = new QCheckBox(QStringLiteral("线性化（Web 快速打开）"), page);
    m_optLinearize->setChecked(true);
    m_optObjectStreams = new QCheckBox(QStringLiteral("生成对象流（减小体积）"), page);
    m_optObjectStreams->setChecked(true);
    m_optRecompress = new QCheckBox(QStringLiteral("重压缩数据流"), page);
    m_optRecompress->setChecked(true);
    m_optImages = new QCheckBox(QStringLiteral("优化图片（可能影响画质）"), page);
    layout->addWidget(m_optLinearize);
    layout->addWidget(m_optObjectStreams);
    layout->addWidget(m_optRecompress);
    layout->addWidget(m_optImages);

    m_optLevel = new QSpinBox(page);
    m_optLevel->setRange(1, 9);
    m_optLevel->setValue(9);
    layout->addLayout(labeledRow(QStringLiteral("压缩级别:"), m_optLevel, page));

    m_optPassword = new QLineEdit(page);
    m_optPassword->setEchoMode(QLineEdit::Password);
    m_optPassword->setPlaceholderText(QStringLiteral("可留空；若源文件已加密则填写"));
    layout->addLayout(labeledRow(QStringLiteral("打开密码:"), m_optPassword, page));

    m_optOut = new QLineEdit(page);
    m_optOut->setPlaceholderText(QStringLiteral("留空则输出到源文件同目录 <名称>_optimized.pdf"));
    QPushButton *outBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *outRow = labeledRow(QStringLiteral("输出文件:"), m_optOut, page);
    outRow->addWidget(outBtn);
    layout->addLayout(outRow);
    connect(outBtn, &QPushButton::clicked, this, &PdfTool::onOptBrowseOut);

    m_optResult = new QLabel(QStringLiteral("提示：qpdf 为无损结构优化，不会降低图片分辨率。"), page);
    m_optResult->setWordWrap(true);
    m_optResult->setStyleSheet(QStringLiteral("color:#6c757d;"));
    layout->addWidget(m_optResult);

    QPushButton *runBtn = new QPushButton(QStringLiteral("开始优化"), page);
    QHBoxLayout *runRow = new QHBoxLayout();
    runRow->addStretch();
    runRow->addWidget(runBtn);
    layout->addLayout(runRow);
    connect(runBtn, &QPushButton::clicked, this, &PdfTool::onOptRun);
    registerActionButton(runBtn);

    tabs->addTab(page, QStringLiteral("压缩 / 优化"));
}

void PdfTool::buildInfoTab(QTabWidget *tabs)
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    m_infoIn = new QLineEdit(page);
    QPushButton *inBtn = new QPushButton(QStringLiteral("浏览"), page);
    QHBoxLayout *inRow = labeledRow(QStringLiteral("源 PDF:"), m_infoIn, page);
    inRow->addWidget(inBtn);
    layout->addLayout(inRow);
    connect(inBtn, &QPushButton::clicked, this, &PdfTool::onInfoBrowse);

    m_infoPassword = new QLineEdit(page);
    m_infoPassword->setEchoMode(QLineEdit::Password);
    m_infoPassword->setPlaceholderText(QStringLiteral("可留空；若文件已加密则填写"));
    layout->addLayout(labeledRow(QStringLiteral("打开密码:"), m_infoPassword, page));

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *readBtn = new QPushButton(QStringLiteral("读取信息"), page);
    QPushButton *copyBtn = new QPushButton(QStringLiteral("复制"), page);
    btnRow->addWidget(readBtn);
    btnRow->addWidget(copyBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);
    connect(readBtn, &QPushButton::clicked, this, &PdfTool::onInfoRead);
    connect(copyBtn, &QPushButton::clicked, this, &PdfTool::onInfoCopy);
    registerActionButton(readBtn);

    m_infoOut = new QTextEdit(page);
    m_infoOut->setReadOnly(true);
    m_infoOut->setStyleSheet(QStringLiteral(
        "QTextEdit { font-family:'Consolas','Courier New',monospace; font-size:12px;"
        " border:1px solid #ced4da; border-radius:6px; padding:10px; background:#ffffff; color:#212529; }"));
    layout->addWidget(m_infoOut, 1);

    tabs->addTab(page, QStringLiteral("信息 / 元数据"));
}

// ==================== 公共辅助 ====================

void PdfTool::registerActionButton(QPushButton *btn)
{
    m_actionButtons.append(btn);
}

void PdfTool::setBusy(bool busy)
{
    m_running = busy;
    for (QPushButton *btn : m_actionButtons)
        btn->setEnabled(!busy && m_available);
    if (m_stopBtn)
        m_stopBtn->setEnabled(busy);
}

void PdfTool::log(const QString &text)
{
    if (m_log && !text.isEmpty())
        m_log->append(text);
}

void PdfTool::setStatus(const QString &text, bool error)
{
    if (!m_statusLabel)
        return;
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(error ? QStringLiteral("color:#dc3545;")
                                       : QStringLiteral("color:#6c757d;"));
}

void PdfTool::runQpdf(const QStringList &args, const QString &title,
                      const std::function<void(bool, const QString &, const QString &)> &onDone)
{
    if (m_running)
        return;
    if (!m_available) {
        showResult(QStringLiteral("不可用"), QStringLiteral("未找到 qpdf.exe"), true);
        return;
    }

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    m_stopping = false;
    setBusy(true);
    setStatus(title + QStringLiteral("…"));
    log(QStringLiteral("> qpdf ") + maskPassword(args));

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, onDone, title](int code, QProcess::ExitStatus status) {
                const QString out = QString::fromUtf8(m_process->readAllStandardOutput());
                const QString err = QString::fromUtf8(m_process->readAllStandardError());
                const bool ok = (status == QProcess::NormalExit && code == 0);
                setBusy(false);
                if (m_stopping) {
                    m_stopping = false;
                    setStatus(title + QStringLiteral(" 已停止"), true);
                    return;
                }
                if (!out.trimmed().isEmpty())
                    log(out.trimmed());
                if (!err.trimmed().isEmpty())
                    log(err.trimmed());
                if (onDone)
                    onDone(ok, out, err);
                if (ok)
                    setStatus(title + QStringLiteral(" 完成"));
                else
                    setStatus(title + QStringLiteral(" 失败（退出码 %1）").arg(code), true);
            });

    connect(m_process, &QProcess::errorOccurred, this,
            [this, onDone, title](QProcess::ProcessError) {
                const QString err = m_process->errorString();
                setBusy(false);
                log(err);
                if (onDone)
                    onDone(false, QString(), err);
                setStatus(title + QStringLiteral(" 失败"), true);
            });

    m_process->start(m_qpdfPath, args);
}

bool PdfTool::runQpdfSync(const QStringList &args, QString *out, QString *err, int timeoutMs) const
{
    QProcess proc;
    proc.start(m_qpdfPath, args);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(1000);
        if (err)
            *err = QStringLiteral("执行超时");
        return false;
    }
    if (out)
        *out = QString::fromUtf8(proc.readAllStandardOutput());
    if (err)
        *err = QString::fromUtf8(proc.readAllStandardError());
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

QString PdfTool::pickOpenPdf(QLineEdit *edit, const QString &title)
{
    const QString path = QFileDialog::getOpenFileName(m_page, title, edit->text(),
                                                      QString::fromLatin1(kPdfFilter));
    if (!path.isEmpty())
        edit->setText(path);
    return path;
}

QString PdfTool::pickSavePdf(QLineEdit *edit, const QString &defaultName)
{
    QString start = edit->text().trimmed();
    if (start.isEmpty())
        start = defaultName;
    const QString path = QFileDialog::getSaveFileName(m_page, QStringLiteral("选择输出文件"), start,
                                                      QString::fromLatin1(kPdfFilter));
    if (!path.isEmpty())
        edit->setText(path);
    return path;
}

QString PdfTool::pickDirectory(QLineEdit *edit, const QString &title)
{
    const QString dir = QFileDialog::getExistingDirectory(m_page, title, edit->text());
    if (!dir.isEmpty())
        edit->setText(dir);
    return dir;
}

QString PdfTool::defaultOutPath(const QString &input, const QString &suffix)
{
    const QFileInfo fi(input);
    return fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName() + suffix + QStringLiteral(".pdf");
}

QString PdfTool::safeOutput(const QString &input, const QString &out)
{
    if (input.isEmpty() || out.isEmpty())
        return out;
    if (QFileInfo(input).absoluteFilePath().compare(QFileInfo(out).absoluteFilePath(),
                                                    Qt::CaseInsensitive) != 0)
        return out;
    const QFileInfo fi(out);
    return fi.absolutePath() + QStringLiteral("/") + fi.completeBaseName() + QStringLiteral("_out.pdf");
}

QString PdfTool::maskPassword(const QStringList &args)
{
    QStringList masked;
    bool inEncrypt = false;
    for (const QString &arg : args) {
        if (arg.startsWith(QStringLiteral("--password="))) {
            masked << QStringLiteral("--password=***");
        } else if (arg == QStringLiteral("--encrypt")) {
            inEncrypt = true;
            masked << arg;
        } else if (inEncrypt) {
            if (arg == QStringLiteral("--")) {
                inEncrypt = false;
                masked << arg;
            } else if (arg.startsWith(QStringLiteral("--"))
                       || arg == QStringLiteral("40") || arg == QStringLiteral("128")
                       || arg == QStringLiteral("256")) {
                masked << arg;
            } else {
                masked << QStringLiteral("***");
            }
        } else {
            masked << arg;
        }
    }
    return masked.join(QLatin1Char(' '));
}

void PdfTool::showResult(const QString &title, const QString &text, bool error)
{
    if (error)
        QMessageBox::warning(m_page, title, text);
    else
        QMessageBox::information(m_page, title, text);
}

// ==================== 合并 ====================

void PdfTool::addFiles(const QStringList &paths)
{
    if (!m_mergeList)
        return;
    for (const QString &path : paths) {
        if (!path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
            continue;
        bool exists = false;
        for (int i = 0; i < m_mergeList->count(); ++i) {
            if (m_mergeList->item(i)->text() == path) {
                exists = true;
                break;
            }
        }
        if (!exists)
            m_mergeList->addItem(path);
    }
}

void PdfTool::onMergeAdd()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        m_page, QStringLiteral("选择 PDF 文件"), QString(), QString::fromLatin1(kPdfFilter));
    addFiles(paths);
}

void PdfTool::onMergeRemove()
{
    const QList<QListWidgetItem *> items = m_mergeList->selectedItems();
    for (QListWidgetItem *item : items)
        delete item;
}

void PdfTool::onMergeClear()
{
    m_mergeList->clear();
}

void PdfTool::onMergeMoveUp()
{
    const int row = m_mergeList->currentRow();
    if (row <= 0)
        return;
    QListWidgetItem *item = m_mergeList->takeItem(row);
    m_mergeList->insertItem(row - 1, item);
    m_mergeList->setCurrentRow(row - 1);
}

void PdfTool::onMergeMoveDown()
{
    const int row = m_mergeList->currentRow();
    if (row < 0 || row >= m_mergeList->count() - 1)
        return;
    QListWidgetItem *item = m_mergeList->takeItem(row);
    m_mergeList->insertItem(row + 1, item);
    m_mergeList->setCurrentRow(row + 1);
}

void PdfTool::onMergeRun()
{
    if (m_running)
        return;
    if (m_mergeList->count() == 0) {
        showResult(QStringLiteral("提示"), QStringLiteral("请先添加至少一个 PDF 文件"), true);
        return;
    }
    QStringList files;
    for (int i = 0; i < m_mergeList->count(); ++i) {
        const QString path = m_mergeList->item(i)->text();
        if (!QFileInfo::exists(path)) {
            showResult(QStringLiteral("错误"), QStringLiteral("文件不存在:\n%1").arg(path), true);
            return;
        }
        files << path;
    }

    QString out = m_mergeOut->text().trimmed();
    if (out.isEmpty()) {
        out = defaultOutPath(files.first(), QStringLiteral("_merged"));
        m_mergeOut->setText(out);
    }
    if (!out.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        out += QStringLiteral(".pdf");
    out = safeOutput(files.first(), out);
    if (!QDir().mkpath(QFileInfo(out).absolutePath())) {
        showResult(QStringLiteral("错误"), QStringLiteral("无法创建输出目录"), true);
        return;
    }

    const QString pw = m_mergePassword->text();
    QStringList args;
    args << QStringLiteral("--empty") << QStringLiteral("--pages");
    for (const QString &file : files) {
        args << file;
        if (!pw.isEmpty())
            args << (QStringLiteral("--password=") + pw);
    }
    args << QStringLiteral("--") << out;

    runQpdf(args, QStringLiteral("合并 PDF"),
            [this, out](bool ok, const QString &, const QString &) {
                if (ok) {
                    Utils::logToFile(QStringLiteral("[PDF] merged -> %1").arg(out));
                    showResult(QStringLiteral("合并成功"),
                               QStringLiteral("输出文件:\n%1").arg(QDir::toNativeSeparators(out)), false);
                }
            });
}

// ==================== 拆分 / 提取 ====================

void PdfTool::onSplitBrowseIn()
{
    pickOpenPdf(m_splitIn, QStringLiteral("选择源 PDF"));
}

void PdfTool::onSplitBrowseOut()
{
    if (m_splitMode->currentIndex() == 0) {
        const QString src = m_splitIn->text().trimmed();
        pickSavePdf(m_splitOut, src.isEmpty() ? QStringLiteral("pages.pdf")
                                              : defaultOutPath(src, QStringLiteral("_pages")));
    } else {
        pickDirectory(m_splitOut, QStringLiteral("选择输出目录"));
    }
}

void PdfTool::onSplitModeChanged()
{
    const int mode = m_splitMode->currentIndex();
    m_splitRange->setEnabled(mode == 0);
    m_splitEvery->setEnabled(mode == 2);
    m_splitOutLabel->setText(mode == 0 ? QStringLiteral("输出文件:")
                                       : QStringLiteral("输出目录:"));
    m_splitOut->setPlaceholderText(mode == 0
        ? QStringLiteral("留空则输出到源文件同目录 <名称>_pages.pdf")
        : QStringLiteral("留空则输出到源文件同目录"));
}

void PdfTool::onSplitRun()
{
    if (m_running)
        return;
    const QString src = m_splitIn->text().trimmed();
    if (src.isEmpty() || !QFileInfo::exists(src)) {
        showResult(QStringLiteral("提示"), QStringLiteral("请选择有效的源 PDF 文件"), true);
        return;
    }

    const int mode = m_splitMode->currentIndex();
    const QString pw = m_splitPassword->text();
    QStringList args;
    args << src;
    if (!pw.isEmpty())
        args << (QStringLiteral("--password=") + pw);

    if (mode == 0) {
        const QString range = m_splitRange->text().trimmed();
        if (range.isEmpty()) {
            showResult(QStringLiteral("提示"), QStringLiteral("请填写页码范围，例如 1-3,5"), true);
            return;
        }
        QString out = m_splitOut->text().trimmed();
        if (out.isEmpty())
            out = defaultOutPath(src, QStringLiteral("_pages"));
        if (!out.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
            out += QStringLiteral(".pdf");
        out = safeOutput(src, out);
        if (!QDir().mkpath(QFileInfo(out).absolutePath())) {
            showResult(QStringLiteral("错误"), QStringLiteral("无法创建输出目录"), true);
            return;
        }
        args << QStringLiteral("--pages") << QStringLiteral(".") << range
             << QStringLiteral("--") << out;
        runQpdf(args, QStringLiteral("提取页面"),
                [this, out](bool ok, const QString &, const QString &) {
                    if (ok)
                        showResult(QStringLiteral("提取成功"),
                                   QStringLiteral("输出文件:\n%1").arg(QDir::toNativeSeparators(out)), false);
                });
    } else {
        QString dir = m_splitOut->text().trimmed();
        if (dir.isEmpty())
            dir = QFileInfo(src).absolutePath();
        if (!QDir().mkpath(dir)) {
            showResult(QStringLiteral("错误"), QStringLiteral("无法创建输出目录"), true);
            return;
        }
        const QString base = QFileInfo(src).completeBaseName() + QStringLiteral("_split");
        const QString outPath = dir + QStringLiteral("/") + base + QStringLiteral(".pdf");
        if (mode == 1)
            args << QStringLiteral("--split-pages");
        else
            args << (QStringLiteral("--split-pages=") + QString::number(m_splitEvery->value()));
        args << outPath;

        runQpdf(args, QStringLiteral("拆分 PDF"),
                [this, dir, base](bool ok, const QString &, const QString &) {
                    if (!ok)
                        return;
                    QDir d(dir);
                    const QStringList files = d.entryList({base + QStringLiteral("*.pdf")},
                                                          QDir::Files, QDir::Name);
                    QStringList paths;
                    for (const QString &f : files)
                        paths << QDir::toNativeSeparators(dir + QStringLiteral("/") + f);
                    showResult(QStringLiteral("拆分成功"),
                               QStringLiteral("共生成 %1 个文件:\n%2")
                                   .arg(paths.size())
                                   .arg(paths.join(QLatin1Char('\n'))), false);
                });
    }
}

// ==================== 页面编排 ====================

void PdfTool::onOrgBrowseIn()
{
    pickOpenPdf(m_orgIn, QStringLiteral("选择源 PDF"));
}

void PdfTool::onOrgBrowseOut()
{
    const QString src = m_orgIn->text().trimmed();
    pickSavePdf(m_orgOut, src.isEmpty() ? QStringLiteral("organized.pdf")
                                        : defaultOutPath(src, QStringLiteral("_organized")));
}

void PdfTool::onOrgRun()
{
    if (m_running)
        return;
    const QString src = m_orgIn->text().trimmed();
    if (src.isEmpty() || !QFileInfo::exists(src)) {
        showResult(QStringLiteral("提示"), QStringLiteral("请选择有效的源 PDF 文件"), true);
        return;
    }
    const QString order = m_orgOrder->text().trimmed();
    const bool doRotate = m_orgRotate->isChecked();
    if (order.isEmpty() && !doRotate) {
        showResult(QStringLiteral("提示"),
                   QStringLiteral("请填写页序表达式，或勾选旋转页面"), true);
        return;
    }

    QString out = m_orgOut->text().trimmed();
    if (out.isEmpty())
        out = defaultOutPath(src, QStringLiteral("_organized"));
    if (!out.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        out += QStringLiteral(".pdf");
    out = safeOutput(src, out);
    if (!QDir().mkpath(QFileInfo(out).absolutePath())) {
        showResult(QStringLiteral("错误"), QStringLiteral("无法创建输出目录"), true);
        return;
    }

    const QString pw = m_orgPassword->text();
    QStringList args;
    args << src;
    if (!pw.isEmpty())
        args << (QStringLiteral("--password=") + pw);
    if (!order.isEmpty())
        args << QStringLiteral("--pages") << QStringLiteral(".") << order << QStringLiteral("--");
    if (doRotate) {
        QString rotate = QStringLiteral("--rotate=") + m_orgAngle->currentData().toString();
        const QString range = m_orgRotRange->text().trimmed();
        if (!range.isEmpty())
            rotate += QLatin1Char(':') + range;
        args << rotate;
    }
    args << out;

    runQpdf(args, QStringLiteral("页面编排"),
            [this, out](bool ok, const QString &, const QString &) {
                if (ok)
                    showResult(QStringLiteral("处理成功"),
                               QStringLiteral("输出文件:\n%1").arg(QDir::toNativeSeparators(out)), false);
            });
}

// ==================== 加密 / 解密 ====================

void PdfTool::onEncBrowseIn()
{
    pickOpenPdf(m_encIn, QStringLiteral("选择源 PDF"));
}

void PdfTool::onEncBrowseOut()
{
    const QString src = m_encIn->text().trimmed();
    const bool encrypt = m_encModeEncrypt->isChecked();
    pickSavePdf(m_encOut, src.isEmpty() ? QStringLiteral("output.pdf")
                                        : defaultOutPath(src, encrypt ? QStringLiteral("_encrypted")
                                                                      : QStringLiteral("_decrypted")));
}

void PdfTool::onEncModeChanged()
{
    const bool encrypt = m_encModeEncrypt->isChecked();
    m_encUser->setEnabled(encrypt);
    m_encOwner->setEnabled(encrypt);
    m_encAllowPrint->setEnabled(encrypt);
    m_encAllowCopy->setEnabled(encrypt);
    m_encAllowModify->setEnabled(encrypt);
    m_encPassword->setEnabled(!encrypt);
    if (encrypt) {
        m_encHint->setText(QStringLiteral(
            "使用 AES-256 加密。用户密码留空则打开文件无需密码；所有者密码用于限制权限。"));
    } else {
        m_encHint->setText(QStringLiteral(
            "解密：输入源文件的打开密码，输出为无加密的 PDF。"));
    }
}

void PdfTool::onEncRun()
{
    if (m_running)
        return;
    const QString src = m_encIn->text().trimmed();
    if (src.isEmpty() || !QFileInfo::exists(src)) {
        showResult(QStringLiteral("提示"), QStringLiteral("请选择有效的源 PDF 文件"), true);
        return;
    }
    const bool encrypt = m_encModeEncrypt->isChecked();

    QString out = m_encOut->text().trimmed();
    if (out.isEmpty())
        out = defaultOutPath(src, encrypt ? QStringLiteral("_encrypted") : QStringLiteral("_decrypted"));
    if (!out.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        out += QStringLiteral(".pdf");
    out = safeOutput(src, out);
    if (!QDir().mkpath(QFileInfo(out).absolutePath())) {
        showResult(QStringLiteral("错误"), QStringLiteral("无法创建输出目录"), true);
        return;
    }

    QStringList args;
    if (encrypt) {
        args << src << QStringLiteral("--encrypt")
             << m_encUser->text() << m_encOwner->text() << QStringLiteral("256")
             << (m_encAllowPrint->isChecked() ? QStringLiteral("--print=full")
                                              : QStringLiteral("--print=none"))
             << (m_encAllowModify->isChecked() ? QStringLiteral("--modify=all")
                                               : QStringLiteral("--modify=none"))
             << (m_encAllowCopy->isChecked() ? QStringLiteral("--extract=y")
                                             : QStringLiteral("--extract=n"))
             << QStringLiteral("--accessibility=y")
             << QStringLiteral("--") << out;
    } else {
        const QString pw = m_encPassword->text();
        args << (QStringLiteral("--password=") + pw)
             << QStringLiteral("--decrypt") << src << out;
    }

    runQpdf(args, encrypt ? QStringLiteral("加密 PDF") : QStringLiteral("解密 PDF"),
            [this, out, encrypt](bool ok, const QString &, const QString &) {
                if (ok)
                    showResult(encrypt ? QStringLiteral("加密成功") : QStringLiteral("解密成功"),
                               QStringLiteral("输出文件:\n%1").arg(QDir::toNativeSeparators(out)), false);
            });
}

// ==================== 压缩 / 优化 ====================

void PdfTool::onOptBrowseIn()
{
    pickOpenPdf(m_optIn, QStringLiteral("选择源 PDF"));
}

void PdfTool::onOptBrowseOut()
{
    const QString src = m_optIn->text().trimmed();
    pickSavePdf(m_optOut, src.isEmpty() ? QStringLiteral("optimized.pdf")
                                        : defaultOutPath(src, QStringLiteral("_optimized")));
}

void PdfTool::onOptRun()
{
    if (m_running)
        return;
    const QString src = m_optIn->text().trimmed();
    if (src.isEmpty() || !QFileInfo::exists(src)) {
        showResult(QStringLiteral("提示"), QStringLiteral("请选择有效的源 PDF 文件"), true);
        return;
    }

    QString out = m_optOut->text().trimmed();
    if (out.isEmpty())
        out = defaultOutPath(src, QStringLiteral("_optimized"));
    if (!out.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        out += QStringLiteral(".pdf");
    out = safeOutput(src, out);
    if (!QDir().mkpath(QFileInfo(out).absolutePath())) {
        showResult(QStringLiteral("错误"), QStringLiteral("无法创建输出目录"), true);
        return;
    }

    const QString pw = m_optPassword->text();
    QStringList args;
    args << src;
    if (!pw.isEmpty())
        args << (QStringLiteral("--password=") + pw);
    if (m_optLinearize->isChecked())
        args << QStringLiteral("--linearize");
    if (m_optObjectStreams->isChecked())
        args << QStringLiteral("--object-streams=generate");
    if (m_optRecompress->isChecked()) {
        args << QStringLiteral("--compress-streams=y")
             << QStringLiteral("--recompress-flate")
             << (QStringLiteral("--compression-level=") + QString::number(m_optLevel->value()));
    }
    if (m_optImages->isChecked())
        args << QStringLiteral("--optimize-images");
    args << out;

    runQpdf(args, QStringLiteral("压缩 / 优化"),
            [this, src, out](bool ok, const QString &, const QString &) {
                if (!ok)
                    return;
                const qint64 inSize = QFileInfo(src).size();
                const qint64 outSize = QFileInfo(out).size();
                const double ratio = inSize > 0 ? (1.0 - static_cast<double>(outSize) / inSize) * 100.0 : 0.0;
                const QString summary = QStringLiteral("原始: %1  →  优化后: %2  （%3）")
                    .arg(Utils::formatFileSize(inSize), Utils::formatFileSize(outSize),
                         ratio >= 0 ? QStringLiteral("减小 %1%").arg(QString::number(ratio, 'f', 1))
                                    : QStringLiteral("增大 %1%").arg(QString::number(-ratio, 'f', 1)));
                m_optResult->setText(summary);
                m_optResult->setStyleSheet(QStringLiteral("color:#198754;"));
                Utils::logToFile(QStringLiteral("[PDF] optimized %1 -> %2").arg(src, out));
                showResult(QStringLiteral("优化完成"),
                           QStringLiteral("%1\n\n输出文件:\n%2")
                               .arg(summary, QDir::toNativeSeparators(out)), false);
            });
}

// ==================== 信息 / 元数据 ====================

void PdfTool::onInfoBrowse()
{
    pickOpenPdf(m_infoIn, QStringLiteral("选择 PDF 文件"));
}

void PdfTool::onInfoCopy()
{
    QApplication::clipboard()->setText(m_infoOut->toPlainText());
    setStatus(QStringLiteral("信息已复制到剪贴板"));
}

void PdfTool::onInfoRead()
{
    if (m_running)
        return;
    const QString src = m_infoIn->text().trimmed();
    if (src.isEmpty() || !QFileInfo::exists(src)) {
        showResult(QStringLiteral("提示"), QStringLiteral("请选择有效的 PDF 文件"), true);
        return;
    }

    setBusy(true);
    if (m_stopBtn)
        m_stopBtn->setEnabled(false);
    setStatus(QStringLiteral("读取信息…"));

    const QString pw = m_infoPassword->text();
    QStringList baseArgs;
    baseArgs << src;
    if (!pw.isEmpty())
        baseArgs << (QStringLiteral("--password=") + pw);

    QStringList lines;
    lines << QStringLiteral("文件:     ") + QDir::toNativeSeparators(src);
    lines << QStringLiteral("大小:     ") + Utils::formatFileSize(QFileInfo(src).size());

    // PDF 版本（读取文件头）
    QFile file(src);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray head = file.read(1024);
        file.close();
        QRegularExpression re(QStringLiteral("%PDF-(\\d\\.\\d)"));
        auto m = re.match(QString::fromLatin1(head));
        if (m.hasMatch())
            lines << QStringLiteral("PDF 版本: ") + m.captured(1);
    }

    // 页数
    QString out, err;
    {
        QStringList args = baseArgs;
        args << QStringLiteral("--show-npages");
        if (runQpdfSync(args, &out, &err))
            lines << QStringLiteral("页数:     ") + out.trimmed();
        else
            lines << QStringLiteral("页数:     (读取失败) ") + err.trimmed();
    }

    // 加密状态
    {
        QStringList args = baseArgs;
        args << QStringLiteral("--show-encryption");
        if (runQpdfSync(args, &out, &err)) {
            const QString info = out.trimmed();
            lines << QStringLiteral("加密:     ")
                  + (info.contains(QStringLiteral("not encrypted"))
                         ? QStringLiteral("未加密")
                         : QStringLiteral("已加密\n") + info);
        }
    }

    // 线性化
    {
        QStringList args = baseArgs;
        args << QStringLiteral("--show-linearization");
        if (runQpdfSync(args, &out, &err))
            lines << QStringLiteral("线性化:   ") + (out.contains(QStringLiteral("not linearized"))
                                                         ? QStringLiteral("否")
                                                         : QStringLiteral("是"));
    }

    // 元数据（/Info 字典）
    lines << QString() << QStringLiteral("— 元数据 —");
    QString trailer;
    {
        QStringList args = baseArgs;
        args << QStringLiteral("--show-object=trailer");
        if (runQpdfSync(args, &trailer, &err)) {
            QRegularExpression re(QStringLiteral("/Info\\s+(\\d+)\\s+(\\d+)\\s+R"));
            auto m = re.match(trailer);
            if (!m.hasMatch()) {
                lines << QStringLiteral("(无元数据)");
            } else {
                const QString objRef = m.captured(1) + QLatin1Char(',') + m.captured(2);
                QStringList args2 = baseArgs;
                args2 << QStringLiteral("--json=1") << QStringLiteral("--json-key=objects")
                      << (QStringLiteral("--json-object=") + objRef);
                QString json;
                if (runQpdfSync(args2, &json, &err)) {
                    const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
                    const QJsonObject objects = root.value(QStringLiteral("objects")).toObject();
                    QJsonObject info = objects.value(objects.keys().value(0)).toObject();
                    const QStringList keys = {
                        QStringLiteral("/Title"), QStringLiteral("/Author"), QStringLiteral("/Subject"),
                        QStringLiteral("/Keywords"), QStringLiteral("/Creator"), QStringLiteral("/Producer"),
                        QStringLiteral("/CreationDate"), QStringLiteral("/ModDate")
                    };
                    const QStringList names = {
                        QStringLiteral("标题"), QStringLiteral("作者"), QStringLiteral("主题"),
                        QStringLiteral("关键词"), QStringLiteral("创建程序"), QStringLiteral("生成程序"),
                        QStringLiteral("创建时间"), QStringLiteral("修改时间")
                    };
                    bool any = false;
                    for (int i = 0; i < keys.size(); ++i) {
                        QString value = info.value(keys.at(i)).toString();
                        if (value.isEmpty())
                            continue;
                        any = true;
                        if (keys.at(i).contains(QStringLiteral("Date")))
                            value = readablePdfDate(value);
                        lines << QStringLiteral("%1: %2").arg(names.at(i), value);
                    }
                    if (!any)
                        lines << QStringLiteral("(无元数据)");
                } else {
                    lines << QStringLiteral("(元数据读取失败)");
                }
            }
        } else {
            lines << QStringLiteral("(元数据读取失败)");
        }
    }

    m_infoOut->setPlainText(lines.join(QLatin1Char('\n')));
    setBusy(false);
    setStatus(QStringLiteral("信息读取完成"));
}

void PdfTool::onStop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    log(QStringLiteral("正在停止 qpdf…"));
    m_stopping = true;
    m_process->kill();
    if (!m_process->waitForFinished(3000))
        m_process->terminate();
    setBusy(false);
    setStatus(QStringLiteral("已停止"), true);
}
