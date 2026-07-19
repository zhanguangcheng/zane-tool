#include "windowpicker.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

#define WM_PICKER_ESC     (WM_USER + 100)
#define WM_PICKER_LEFT    (WM_USER + 101)
#define WM_PICKER_RIGHT   (WM_USER + 102)

static HHOOK g_keyHook = nullptr;
static HHOOK g_mouseHook = nullptr;
static WindowPicker *g_instance = nullptr;

static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN && g_instance) {
        KBDLLHOOKSTRUCT *kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (kb->vkCode == VK_ESCAPE) {
            PostMessageW(reinterpret_cast<HWND>(g_instance->winId()), WM_PICKER_ESC, 0, 0);
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && g_instance) {
        MSLLHOOKSTRUCT *ms = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);
        switch (wParam) {
        case WM_LBUTTONDOWN:
            PostMessageW(reinterpret_cast<HWND>(g_instance->winId()),
                         WM_PICKER_LEFT, ms->pt.x, ms->pt.y);
            return 1;
        case WM_RBUTTONDOWN:
            PostMessageW(reinterpret_cast<HWND>(g_instance->winId()),
                         WM_PICKER_RIGHT, 0, 0);
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

WindowPicker::WindowPicker(QWidget *parent)
    : QWidget(parent)
    , m_currentHwnd(nullptr)
    , m_overlayHwnd(nullptr)
    , m_trackTimer(nullptr)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
}

WindowPicker::~WindowPicker()
{
    if (g_keyHook) {
        UnhookWindowsHookEx(g_keyHook);
        g_keyHook = nullptr;
    }
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
    g_instance = nullptr;
}

QRect WindowPicker::totalDesktopRect() const
{
    QRect total;
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        total = total.united(s->geometry());
    }
    return total;
}

void WindowPicker::begin()
{
    QRect totalRect = totalDesktopRect();
    setGeometry(totalRect);
    show();
}

void WindowPicker::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_overlayHwnd = reinterpret_cast<HWND>(winId());
    g_instance = this;

    QApplication::setOverrideCursor(Qt::CrossCursor);

    g_keyHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc,
                                  GetModuleHandleW(nullptr), 0);
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseProc,
                                    GetModuleHandleW(nullptr), 0);

    m_trackTimer = new QTimer(this);
    connect(m_trackTimer, &QTimer::timeout, this, [this]() {
        m_cursorPos = QCursor::pos();
        updateCurrentWindow();
        update();
    });
    m_trackTimer->start(30);

    m_cursorPos = QCursor::pos();
    updateCurrentWindow();
    update();
}

void WindowPicker::hideEvent(QHideEvent *event)
{
    if (m_trackTimer) {
        m_trackTimer->stop();
        delete m_trackTimer;
        m_trackTimer = nullptr;
    }

    if (g_keyHook) {
        UnhookWindowsHookEx(g_keyHook);
        g_keyHook = nullptr;
    }
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
    g_instance = nullptr;

    QApplication::restoreOverrideCursor();
    QWidget::hideEvent(event);
}

void WindowPicker::updateCurrentWindow()
{
    POINT pt = { m_cursorPos.x(), m_cursorPos.y() };
    HWND hwnd = WindowFromPoint(pt);

    while (hwnd) {
        if (hwnd != m_overlayHwnd && IsWindowVisible(hwnd)) {
            RECT rect;
            if (GetWindowRect(hwnd, &rect) && PtInRect(&rect, pt)) {
                hwnd = GetAncestor(hwnd, GA_ROOT);
                LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                if (!(exStyle & WS_EX_TOOLWINDOW)) {
                    int len = GetWindowTextLengthW(hwnd);
                    if (len > 0) {
                        m_currentHwnd = hwnd;
                        return;
                    }
                }
            }
        }
        hwnd = GetWindow(hwnd, GW_HWNDNEXT);
    }

    m_currentHwnd = nullptr;
}

void WindowPicker::handleLeftClick()
{
    hide();
    QApplication::processEvents();

    POINT pt = { m_cursorPos.x(), m_cursorPos.y() };
    HWND hwnd = WindowFromPoint(pt);
    if (hwnd) {
        hwnd = GetAncestor(hwnd, GA_ROOT);
    }

    if (hwnd && IsWindow(hwnd)) {
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (!(exStyle & WS_EX_TOOLWINDOW)) {
            int len = GetWindowTextLengthW(hwnd);
            if (len > 0) {
                emit windowPicked(hwnd);
                return;
            }
        }
    }
    emit cancelled();
}

void WindowPicker::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    if (m_currentHwnd && IsWindow(m_currentHwnd)) {
        int len = GetWindowTextLengthW(m_currentHwnd);
        if (len > 0) {
            RECT wr;
            if (GetWindowRect(m_currentHwnd, &wr)) {
                wchar_t *buf = new wchar_t[len + 1];
                GetWindowTextW(m_currentHwnd, buf, len + 1);
                QString title = QString::fromWCharArray(buf);
                delete[] buf;

                QRect winRect(wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top);
                QRect deskRect = totalDesktopRect();
                winRect.translate(-deskRect.topLeft());

                QFont font = painter.font();
                font.setPointSize(12);
                font.setBold(true);
                painter.setFont(font);

                QFontMetrics fm(font);
                int textW = fm.horizontalAdvance(title) + 20;
                int textH = fm.height() + 10;

                int textX = winRect.left() + 8;
                int textY = winRect.top() - textH;
                if (textY < 4)
                    textY = winRect.bottom() + 6;

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 200));
                painter.drawRoundedRect(textX, textY, textW, textH, 8, 8);

                painter.setPen(Qt::white);
                painter.drawText(textX + 10, textY + fm.ascent() + 5, title);
            }
        }
    }

    QPoint pos = m_cursorPos - totalDesktopRect().topLeft();

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 120));
    painter.drawEllipse(pos, 28, 28);

    QPen crossPen(Qt::white, 2);
    painter.setPen(crossPen);
    painter.drawLine(pos.x() - 16, pos.y(), pos.x() - 6, pos.y());
    painter.drawLine(pos.x() + 6, pos.y(), pos.x() + 16, pos.y());
    painter.drawLine(pos.x(), pos.y() - 16, pos.x(), pos.y() - 6);
    painter.drawLine(pos.x(), pos.y() + 6, pos.x(), pos.y() + 16);
    painter.drawEllipse(pos, 12, 12);

    QFont hintFont = painter.font();
    hintFont.setPointSize(13);
    hintFont.setBold(true);
    painter.setFont(hintFont);
    QFontMetrics hintFm(hintFont);
    QString hintText = QStringLiteral("左键选择窗口   |   ESC / 右键取消");
    int hintW = hintFm.horizontalAdvance(hintText) + 36;
    int hintH = hintFm.height() + 20;
    int hintX = (width() - hintW) / 2;
    int hintY = height() - hintH - 24;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(13, 110, 253, 220));
    painter.drawRoundedRect(hintX, hintY, hintW, hintH, 10, 10);

    painter.setPen(Qt::white);
    painter.drawText(QRect(hintX, hintY, hintW, hintH), Qt::AlignCenter, hintText);
}

bool WindowPicker::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        switch (msg->message) {
        case WM_PICKER_ESC:
            hide();
            emit cancelled();
            *result = TRUE;
            return true;
        case WM_PICKER_LEFT:
            m_cursorPos = QPoint(static_cast<int>(msg->wParam),
                                 static_cast<int>(msg->lParam));
            handleLeftClick();
            *result = TRUE;
            return true;
        case WM_PICKER_RIGHT:
            hide();
            emit cancelled();
            *result = TRUE;
            return true;
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}
