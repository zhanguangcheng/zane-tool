#include <QApplication>
#include <QTimer>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QKeyEvent>
#include <QDialog>
#include <QShowEvent>
#include <QHideEvent>

#include <windows.h>

#include "screenshottool.h"
#include "screenshotpicker.h"
#include "pinwindow.h"

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

ScreenshotTool::ScreenshotTool(QWidget *mainWidget, QObject *parent)
    : QObject(parent)
    , m_mainWidget(mainWidget)
    , m_picker(new ScreenshotPicker(nullptr))
    , m_hotkeyLabel(nullptr)
{
    connect(m_picker, &ScreenshotPicker::screenshotCaptured,
            this, &ScreenshotTool::onScreenshotCaptured);
    connect(m_picker, &ScreenshotPicker::cancelled,
            this, &ScreenshotTool::onPickerCancelled);
}

QWidget *ScreenshotTool::createPage()
{
    QWidget *page = new QWidget(m_mainWidget);
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
    connect(startBtn, &QPushButton::clicked, this, &ScreenshotTool::startScreenshot);

    QHBoxLayout *hotkeyRow = new QHBoxLayout();
    hotkeyRow->setSpacing(8);
    hotkeyRow->setAlignment(Qt::AlignCenter);

    m_hotkeyLabel = new QLabel(group);
    m_hotkeyLabel->setStyleSheet(QStringLiteral("color: #0d6efd; font-size: 13px; font-weight: bold;"));
    m_hotkeyLabel->setText(
        QStringLiteral("\u5F53\u524D\u5FEB\u6377\u952E\uFF1A%1").arg(hotkeyDisplayText()));

    QPushButton *changeHotkeyBtn = new QPushButton(QStringLiteral("\u4FEE\u6539"), group);
    changeHotkeyBtn->setFixedHeight(28);
    changeHotkeyBtn->setFixedWidth(56);
    changeHotkeyBtn->setCursor(Qt::PointingHandCursor);
    changeHotkeyBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: #0d6efd; border: 1px solid #0d6efd; "
        "  border-radius: 4px; font-size: 12px; padding: 2px 8px; }"
        "QPushButton:hover { background-color: #0d6efd; color: #ffffff; }"));
    connect(changeHotkeyBtn, &QPushButton::clicked, this, &ScreenshotTool::onChangeScreenshotHotkey);

    hotkeyRow->addWidget(m_hotkeyLabel);
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

void ScreenshotTool::startScreenshot()
{
    m_screenshotForQr = false;
    HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    ShowWindow(hwnd, SW_HIDE);
    QTimer::singleShot(200, this, [this]() {
        m_picker->begin();
    });
}

void ScreenshotTool::startScreenshotForQr()
{
    m_screenshotForQr = true;
    HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    ShowWindow(hwnd, SW_HIDE);
    QTimer::singleShot(200, this, [this]() {
        m_picker->begin();
    });
}

void ScreenshotTool::onScreenshotCaptured(const QPixmap &pixmap, QPoint globalPos)
{
    HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    ShowWindow(hwnd, SW_SHOW);
    m_mainWidget->activateWindow();

    if (m_screenshotForQr) {
        m_screenshotForQr = false;
        emit qrScreenshotCaptured(pixmap.toImage());
        return;
    }

    PinWindow *pin = new PinWindow(pixmap, globalPos);
    m_pinnedWindows.append(pin);
    connect(pin, &PinWindow::destroyed, this, [this, pin]() {
        m_pinnedWindows.removeAll(pin);
    });
}

void ScreenshotTool::onPickerCancelled()
{
    HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    ShowWindow(hwnd, SW_SHOW);
    m_mainWidget->activateWindow();
}

void ScreenshotTool::loadConfig()
{
    QString configPath = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings settings(configPath, QSettings::IniFormat);
    m_hotkeyVk = settings.value(QStringLiteral("screenshot/hotkeyVk"), VK_F4).toUInt();
    m_hotkeyModifiers = settings.value(QStringLiteral("screenshot/hotkeyModifiers"), 0).toUInt();
    updateHotkeyLabel();
}

WPARAM ScreenshotTool::hotkeyId() const
{
    return m_hotkeyId;
}

void ScreenshotTool::registerHotkey()
{
    HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    unregisterHotkey();
    RegisterHotKey(hwnd, m_hotkeyId, m_hotkeyModifiers, m_hotkeyVk);
}

void ScreenshotTool::unregisterHotkey()
{
    HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    UnregisterHotKey(hwnd, m_hotkeyId);
}

QString ScreenshotTool::hotkeyDisplayText() const
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

void ScreenshotTool::updateHotkeyLabel()
{
    if (m_hotkeyLabel) {
        m_hotkeyLabel->setText(
            QStringLiteral("\u5F53\u524D\u5FEB\u6377\u952E\uFF1A%1").arg(hotkeyDisplayText()));
    }
}

void ScreenshotTool::onChangeScreenshotHotkey()
{
    HotkeyDialog dlg(m_mainWidget);
    if (dlg.exec() != QDialog::Accepted)
        return;

    UINT newVk = dlg.vk;
    UINT newMod = dlg.modifiers;

    if (newVk == m_hotkeyVk && newMod == m_hotkeyModifiers)
        return;

    m_hotkeyVk = newVk;
    m_hotkeyModifiers = newMod;
    registerHotkey();

    QString configPath = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("screenshot/hotkeyVk"), m_hotkeyVk);
    settings.setValue(QStringLiteral("screenshot/hotkeyModifiers"), m_hotkeyModifiers);

    updateHotkeyLabel();
}
