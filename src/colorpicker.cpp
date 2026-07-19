#include "colorpicker.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

ColorPicker::ColorPicker(QWidget *parent)
    : QWidget(parent)
    , m_currentColor(Qt::black)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setMouseTracking(true);
}

QRect ColorPicker::buildTotalDesktopRect() const
{
    QRect total;
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        total = total.united(s->geometry());
    }
    return total;
}

void ColorPicker::begin()
{
    QRect totalRect = buildTotalDesktopRect();
    setGeometry(totalRect);
    m_screenCapture = QGuiApplication::primaryScreen()->grabWindow(0);
    show();
}

void ColorPicker::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    QApplication::setOverrideCursor(Qt::CrossCursor);
    grabMouse();
    grabKeyboard();

    m_cursorPos = QCursor::pos() - geometry().topLeft();
    updateColorAtCursor();
    update();
}

void ColorPicker::hideEvent(QHideEvent *event)
{
    releaseKeyboard();
    releaseMouse();
    QApplication::restoreOverrideCursor();
    QWidget::hideEvent(event);
}

void ColorPicker::updateColorAtCursor()
{
    if (!m_screenCapture.isNull()) {
        qreal dpr = m_screenCapture.devicePixelRatio();
        int px = qBound(0, qRound(m_cursorPos.x() * dpr), m_screenCapture.width() - 1);
        int py = qBound(0, qRound(m_cursorPos.y() * dpr), m_screenCapture.height() - 1);
        m_currentColor = m_screenCapture.toImage().pixelColor(px, py);
    }
}

void ColorPicker::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.drawPixmap(0, 0, m_screenCapture);

    painter.fillRect(rect(), QColor(0, 0, 0, 50));

    int zoomW = m_zoomPixels * m_zoomFactor;
    int zoomH = m_zoomPixels * m_zoomFactor;

    int zoomX = m_cursorPos.x() + 30;
    int zoomY = m_cursorPos.y() + 30;
    if (zoomX + zoomW + 10 > width())
        zoomX = m_cursorPos.x() - zoomW - 30;
    if (zoomY + zoomH + 10 > height())
        zoomY = m_cursorPos.y() - zoomH - 30;

    qreal dpr = m_screenCapture.devicePixelRatio();
    int halfPixels = m_zoomPixels / 2;
    int srcX = qRound((m_cursorPos.x() - halfPixels) * dpr);
    int srcY = qRound((m_cursorPos.y() - halfPixels) * dpr);
    int srcW = qRound(m_zoomPixels * dpr);
    int srcH = qRound(m_zoomPixels * dpr);

    int phyW = m_screenCapture.width();
    int phyH = m_screenCapture.height();
    if (srcX < 0) { srcW += srcX; srcX = 0; }
    if (srcY < 0) { srcH += srcY; srcY = 0; }
    if (srcX + srcW > phyW) srcW = phyW - srcX;
    if (srcY + srcH > phyH) srcH = phyH - srcY;

    painter.drawPixmap(QRect(zoomX, zoomY, zoomW, zoomH),
                       m_screenCapture,
                       QRect(srcX, srcY, srcW, srcH));

    painter.setPen(QPen(Qt::white, 3));
    painter.drawRect(zoomX - 1, zoomY - 1, zoomW + 2, zoomH + 2);

    painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
    for (int i = 1; i < m_zoomPixels; i++) {
        int linePos = zoomX + i * m_zoomFactor;
        painter.drawLine(linePos, zoomY, linePos, zoomY + zoomH);
    }
    for (int j = 1; j < m_zoomPixels; j++) {
        int linePos = zoomY + j * m_zoomFactor;
        painter.drawLine(zoomX, linePos, zoomX + zoomW, linePos);
    }

    int centerX = zoomX + halfPixels * m_zoomFactor;
    int centerY = zoomY + halfPixels * m_zoomFactor;
    painter.setPen(QPen(Qt::red, 2));
    painter.drawRect(centerX, centerY, m_zoomFactor, m_zoomFactor);

    painter.setPen(QPen(Qt::white, 1));
    painter.drawLine(m_cursorPos.x() - 12, m_cursorPos.y(),
                     m_cursorPos.x() - 4,  m_cursorPos.y());
    painter.drawLine(m_cursorPos.x() + 4,  m_cursorPos.y(),
                     m_cursorPos.x() + 12, m_cursorPos.y());
    painter.drawLine(m_cursorPos.x(), m_cursorPos.y() - 12,
                     m_cursorPos.x(), m_cursorPos.y() - 4);
    painter.drawLine(m_cursorPos.x(), m_cursorPos.y() + 4,
                     m_cursorPos.x(), m_cursorPos.y() + 12);
    painter.drawEllipse(m_cursorPos, 10, 10);

    int panelW = 230;
    int panelH = 110;
    int panelX = width() - panelW - 20;
    int panelY = height() - panelH - 20;

    if (m_cursorPos.x() > panelX - 40 && m_cursorPos.y() > panelY - 40) {
        panelY = 20;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 30, 30, 210));
    painter.drawRoundedRect(panelX, panelY, panelW, panelH, 8, 8);

    painter.setBrush(m_currentColor);
    painter.drawRect(panelX + 12, panelY + 12, 86, 86);
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(panelX + 12, panelY + 12, 86, 86);

    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(12);
    font.setBold(true);
    painter.setFont(font);

    QString hex = m_currentColor.name().toUpper();
    painter.drawText(panelX + 110, panelY + 30, hex);

    font.setPointSize(9);
    font.setBold(false);
    painter.setFont(font);
    painter.setPen(QColor(210, 210, 210));

    QString rgbText = QStringLiteral("RGB: %1, %2, %3")
        .arg(m_currentColor.red())
        .arg(m_currentColor.green())
        .arg(m_currentColor.blue());
    painter.drawText(panelX + 110, panelY + 56, rgbText);

    int hslH = m_currentColor.hslHue();
    int hslS = m_currentColor.hslSaturation() * 100 / 255;
    int hslL = m_currentColor.lightness() * 100 / 255;
    QString hslText = QStringLiteral("HSL: %1°, %2%, %3%")
        .arg(hslH >= 0 ? hslH : 0)
        .arg(hslS)
        .arg(hslL);
    painter.drawText(panelX + 110, panelY + 78, hslText);

    painter.setPen(QColor(160, 160, 160));
    font.setPointSize(8);
    painter.setFont(font);
    painter.drawText(panelX + 110, panelY + 98,
                     QStringLiteral("左键取色 | ESC / 右键取消"));
}

void ColorPicker::mouseMoveEvent(QMouseEvent *event)
{
    QRect totalRect = buildTotalDesktopRect();
    m_cursorPos = QCursor::pos() - totalRect.topLeft();
    updateColorAtCursor();
    update();
    QWidget::mouseMoveEvent(event);
}

void ColorPicker::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit colorPicked(m_currentColor);
        hide();
    } else if (event->button() == Qt::RightButton) {
        emit cancelled();
        hide();
    }
}

void ColorPicker::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
}

void ColorPicker::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit cancelled();
        hide();
    }
}
