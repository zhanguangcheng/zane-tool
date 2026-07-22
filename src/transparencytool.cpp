#include <QTimer>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <windows.h>

#include "transparencytool.h"
#include "windowpicker.h"

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

TransparencyTool::TransparencyTool(QWidget *mainWidget, QObject *parent)
    : QObject(parent)
    , m_mainWidget(mainWidget)
    , m_windowPicker(new WindowPicker(nullptr))
    , m_transparencyWindowList(nullptr)
    , m_transparencyRefreshBtn(nullptr)
    , m_transparencyPickBtn(nullptr)
    , m_transparencySlider(nullptr)
    , m_transparencyValueLabel(nullptr)
    , m_transparencyStatusLabel(nullptr)
    , m_transparencyTargetHwnd(nullptr)
    , m_transparencyOriginalExStyle(0)
{
    connect(m_windowPicker, &WindowPicker::windowPicked,
            this, &TransparencyTool::onWindowPicked);
    connect(m_windowPicker, &WindowPicker::cancelled,
            this, &TransparencyTool::onPickCancelled);
}

QWidget *TransparencyTool::createPage()
{
    QWidget *page = new QWidget(m_mainWidget);
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
    connect(m_transparencyRefreshBtn, &QPushButton::clicked, this, &TransparencyTool::onTransparencyRefresh);

    m_transparencyPickBtn = new QPushButton(QStringLiteral("选取窗口"), listGroup);
    m_transparencyPickBtn->setFixedHeight(36);
    m_transparencyPickBtn->setFixedWidth(140);
    m_transparencyPickBtn->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    connect(m_transparencyPickBtn, &QPushButton::clicked, this, &TransparencyTool::onTransparencyPickWindow);

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
            this, &TransparencyTool::onTransparencySelectionChanged);

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
            this, &TransparencyTool::onTransparencySliderChanged);

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

void TransparencyTool::onPickCancelled()
{
    HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    ShowWindow(hwnd, SW_SHOW);
    m_mainWidget->activateWindow();
}

void TransparencyTool::onWindowPicked(HWND hwnd)
{
    HWND selfHwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    ShowWindow(selfHwnd, SW_SHOW);
    m_mainWidget->activateWindow();

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

void TransparencyTool::onTransparencyRefresh()
{
    m_transparencyWindowList->clear();

    EnumWindowsContext ctx;
    ctx.hwnds = new QList<HWND>();
    ctx.exclude = reinterpret_cast<HWND>(m_mainWidget->winId());

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

    HWND selfHwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
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

void TransparencyTool::onTransparencySelectionChanged()
{
    QListWidgetItem *item = m_transparencyWindowList->currentItem();
    if (!item) {
        QWidget *opacityGroup = m_transparencySlider->parentWidget();
        if (opacityGroup)
            opacityGroup->setEnabled(false);
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(item->data(Qt::UserRole).value<quintptr>());
    HWND selfHwnd = reinterpret_cast<HWND>(m_mainWidget->winId());

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

void TransparencyTool::onTransparencySliderChanged(int value)
{
    m_transparencyValueLabel->setText(QStringLiteral("%1%").arg(value));

    if (!m_transparencyTargetHwnd || !IsWindow(m_transparencyTargetHwnd)
        || m_transparencyTargetHwnd == reinterpret_cast<HWND>(m_mainWidget->winId())) {
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

void TransparencyTool::onTransparencyPickWindow()
{
    HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    ShowWindow(hwnd, SW_HIDE);
    QTimer::singleShot(200, this, [this]() {
        m_windowPicker->begin();
    });
}
