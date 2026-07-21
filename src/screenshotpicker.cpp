#include "screenshotpicker.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

ScreenshotPicker::ScreenshotPicker(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setMouseTracking(true);
}

void ScreenshotPicker::begin()
{
    QPoint cursorPos = QCursor::pos();
    QScreen *screen = QGuiApplication::screenAt(cursorPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    QRect geo = screen->geometry();
    setGeometry(geo);

    m_screenCapture = screen->grabWindow(0);
    m_dpr = m_screenCapture.devicePixelRatio();
    if (m_dpr <= 0.0) m_dpr = 1.0;

    m_selecting = false;
    m_hasSelection = false;

    show();
}

void ScreenshotPicker::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QApplication::setOverrideCursor(Qt::CrossCursor);
    grabMouse();
    grabKeyboard();
}

void ScreenshotPicker::hideEvent(QHideEvent *event)
{
    releaseKeyboard();
    releaseMouse();
    QApplication::restoreOverrideCursor();
    QWidget::hideEvent(event);
}

QRect ScreenshotPicker::normalizedSelection() const
{
    int x1 = qMin(m_startPoint.x(), m_endPoint.x());
    int y1 = qMin(m_startPoint.y(), m_endPoint.y());
    int x2 = qMax(m_startPoint.x(), m_endPoint.x());
    int y2 = qMax(m_startPoint.y(), m_endPoint.y());
    return QRect(x1, y1, x2 - x1, y2 - y1);
}

void ScreenshotPicker::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    p.drawPixmap(0, 0, m_screenCapture);
    p.fillRect(rect(), QColor(0, 0, 0, 90));

    if (m_selecting || m_hasSelection) {
        QRect sel = normalizedSelection();

        if (sel.width() > 0 && sel.height() > 0) {
            p.setClipRect(sel);
            p.drawPixmap(0, 0, m_screenCapture);
            p.setClipping(false);

            p.setPen(QPen(QColor("#4a90d9"), 2, Qt::SolidLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(sel);

            int handleSize = 6;
            p.setPen(QPen(QColor("#4a90d9"), 2));
            p.setBrush(QColor("#ffffff"));
            const QPoint corners[] = {
                sel.topLeft(), sel.topRight(), sel.bottomLeft(), sel.bottomRight()
            };
            for (const QPoint &c : corners) {
                p.drawRect(QRect(c.x() - handleSize / 2, c.y() - handleSize / 2,
                                 handleSize, handleSize));
            }

            qreal dpr = m_dpr;
            int w = qRound(sel.width() * dpr);
            int h = qRound(sel.height() * dpr);
            QString sizeText = QStringLiteral("%1 \u00D7 %2").arg(w).arg(h);

            QFont font(QStringLiteral("Microsoft YaHei"), 10);
            font.setBold(true);
            p.setFont(font);
            p.setPen(Qt::white);
            p.setBrush(QColor(0, 0, 0, 160));

            int labelX = sel.right() - 120;
            int labelY = sel.bottom() + 8;
            if (labelY + 30 > height())
                labelY = sel.top() - 30;
            if (labelX < 0)
                labelX = sel.left() + 4;

            QRect labelRect(labelX - 6, labelY, 120, 24);
            p.drawRoundedRect(labelRect, 4, 4);
            p.drawText(labelRect, Qt::AlignCenter, sizeText);
        }
    } else {
        p.setPen(QColor(255, 255, 255, 180));
        QFont font(QStringLiteral("Microsoft YaHei"), 14);
        font.setBold(true);
        p.setFont(font);
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("\u62D6\u62FD\u9F20\u6807\u9009\u62E9\u622A\u56FE\u533A\u57DF\uFF0CESC \u53D6\u6D88"));
    }
}

void ScreenshotPicker::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_endPoint = event->pos();
        m_selecting = true;
        m_hasSelection = false;
    } else if (event->button() == Qt::RightButton) {
        emit cancelled();
        hide();
    }
}

void ScreenshotPicker::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        m_endPoint = event->pos();
        m_hasSelection = true;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void ScreenshotPicker::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        m_endPoint = event->pos();

        QRect sel = normalizedSelection();
        if (sel.width() > 4 && sel.height() > 4) {
            m_hasSelection = true;
            update();

            QRect physicalSel(sel.left() * m_dpr, sel.top() * m_dpr,
                              sel.width() * m_dpr, sel.height() * m_dpr);
            QPixmap captured = m_screenCapture.copy(physicalSel);
            QPoint globalPos = geometry().topLeft() + sel.topLeft();
            emit screenshotCaptured(captured, globalPos);
            hide();
        } else {
            m_hasSelection = false;
            update();
        }
    }
}

void ScreenshotPicker::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_hasSelection) {
            m_hasSelection = false;
            m_selecting = false;
            update();
        } else {
            emit cancelled();
            hide();
        }
    } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
               && m_hasSelection) {
        QRect sel = normalizedSelection();
        if (sel.width() > 4 && sel.height() > 4) {
            QRect physicalSel(sel.left() * m_dpr, sel.top() * m_dpr,
                              sel.width() * m_dpr, sel.height() * m_dpr);
            QPixmap captured = m_screenCapture.copy(physicalSel);
            QPoint globalPos = geometry().topLeft() + sel.topLeft();
            emit screenshotCaptured(captured, globalPos);
            hide();
        }
    }
}
