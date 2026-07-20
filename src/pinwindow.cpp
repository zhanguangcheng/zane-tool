#include "pinwindow.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QCloseEvent>

PinWindow::PinWindow(const QPixmap &pixmap, QPoint globalPos)
    : QWidget(nullptr)
    , m_originalPixmap(pixmap)
    , m_scaledPixmap(pixmap)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMouseTracking(true);

    qreal dpr = m_originalPixmap.devicePixelRatio();
    if (dpr <= 0.0) dpr = 1.0;
    int logicalW = qRound(m_originalPixmap.width() / dpr);
    int logicalH = qRound(m_originalPixmap.height() / dpr);

    int w = logicalW + BORDER_WIDTH * 2;
    int h = logicalH + BORDER_WIDTH * 2;
    setMinimumSize(40, 40);
    resize(w, h);

    move(globalPos - QPoint(BORDER_WIDTH, BORDER_WIDTH));

    m_closeBtnRect = QRect(w - BORDER_WIDTH - CLOSE_BTN_SIZE, BORDER_WIDTH,
                           CLOSE_BTN_SIZE, CLOSE_BTN_SIZE);
    show();
}

void PinWindow::rescalePixmap()
{
    qreal dpr = m_originalPixmap.devicePixelRatio();
    if (dpr <= 0.0) dpr = 1.0;

    QSize logicalSize(
        qRound(m_originalPixmap.width() / dpr * m_zoomFactor),
        qRound(m_originalPixmap.height() / dpr * m_zoomFactor));

    QSize physSize(logicalSize * dpr);
    m_scaledPixmap = m_originalPixmap.scaled(
        physSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_scaledPixmap.setDevicePixelRatio(dpr);
}

void PinWindow::updateWindowSize()
{
    qreal dpr = m_originalPixmap.devicePixelRatio();
    if (dpr <= 0.0) dpr = 1.0;
    int logicalW = qRound(m_originalPixmap.width() / dpr * m_zoomFactor);
    int logicalH = qRound(m_originalPixmap.height() / dpr * m_zoomFactor);
    int w = logicalW + BORDER_WIDTH * 2;
    int h = logicalH + BORDER_WIDTH * 2;
    resize(w, h);
    m_closeBtnRect = QRect(w - BORDER_WIDTH - CLOSE_BTN_SIZE, BORDER_WIDTH,
                           CLOSE_BTN_SIZE, CLOSE_BTN_SIZE);
}

void PinWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.fillRect(rect(), QColor("#4a90d9"));

    QRect imageRect(BORDER_WIDTH, BORDER_WIDTH,
                    width() - BORDER_WIDTH * 2, height() - BORDER_WIDTH * 2);
    p.drawPixmap(imageRect, m_scaledPixmap);

    if (m_closeBtnRect.width() > 16) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 100));

        QPainterPath path;
        path.addEllipse(m_closeBtnRect.center(),
                        CLOSE_BTN_SIZE / 2.0, CLOSE_BTN_SIZE / 2.0);
        p.drawPath(path);

        int cx = m_closeBtnRect.center().x();
        int cy = m_closeBtnRect.center().y();
        int s = 5;
        p.setPen(QPen(QColor(255, 255, 255, 200), 2));
        p.drawLine(cx - s, cy - s, cx + s, cy + s);
        p.drawLine(cx + s, cy - s, cx - s, cy + s);
    }

    p.setPen(QPen(QColor(0, 0, 0, 30), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(0, 0, width() - 1, height() - 1);
}

void PinWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_closeBtnRect.contains(event->pos())) {
            close();
            return;
        }
        m_dragging = true;
        m_dragOffset = event->pos();
    }
}

void PinWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        move(pos() + event->pos() - m_dragOffset);
    }

    if (m_closeBtnRect.contains(event->pos())) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::OpenHandCursor);
    }
}

void PinWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_closeBtnRect.contains(event->pos())) {
        QPixmap pix = m_originalPixmap;
        close();
        QApplication::clipboard()->setPixmap(pix);
    }
}

void PinWindow::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    qreal oldZoom = m_zoomFactor;

    if (delta > 0) {
        m_zoomFactor = qMin(m_zoomFactor + ZOOM_STEP, MAX_ZOOM);
    } else if (delta < 0) {
        m_zoomFactor = qMax(m_zoomFactor - ZOOM_STEP, MIN_ZOOM);
    }
    if (qFuzzyCompare(m_zoomFactor, oldZoom))
        return;

    QPoint mouseInWindow = event->position().toPoint();
    qreal dpr = m_originalPixmap.devicePixelRatio();
    if (dpr <= 0.0) dpr = 1.0;
    int oldLogicalW = qRound(m_originalPixmap.width() / dpr * oldZoom);
    int oldLogicalH = qRound(m_originalPixmap.height() / dpr * oldZoom);
    int newLogicalW = qRound(m_originalPixmap.width() / dpr * m_zoomFactor);
    int newLogicalH = qRound(m_originalPixmap.height() / dpr * m_zoomFactor);

    QPoint oldWindowPos = pos();
    QPoint oldContentPos = oldWindowPos + QPoint(BORDER_WIDTH, BORDER_WIDTH);
    QPoint mouseGlobal = oldWindowPos + mouseInWindow;
    QPointF anchorRatio(
        (mouseInWindow.x() - BORDER_WIDTH) / qMax(1.0, qreal(oldLogicalW)),
        (mouseInWindow.y() - BORDER_WIDTH) / qMax(1.0, qreal(oldLogicalH)));

    rescalePixmap();
    updateWindowSize();

    int newContentX = qRound(mouseGlobal.x() - anchorRatio.x() * newLogicalW);
    int newContentY = qRound(mouseGlobal.y() - anchorRatio.y() * newLogicalH);
    move(newContentX - BORDER_WIDTH, newContentY - BORDER_WIDTH);

    update();
}

void PinWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: #ffffff; border: 1px solid #dee2e6; border-radius: 4px; padding: 4px 0; }"
        "QMenu::item { padding: 6px 32px 6px 16px; color: #212529; }"
        "QMenu::item:selected { background-color: #e7f1ff; color: #0d6efd; }"
    ));

    QAction *copyAction = menu.addAction(QStringLiteral("\u590D\u5236\u5230\u526A\u8D34\u677F"));
    QAction *saveAction = menu.addAction(QStringLiteral("\u53E6\u5B58\u4E3A..."));
    QAction *resetZoomAction = menu.addAction(QStringLiteral("\u6062\u590D\u539F\u59CB\u5927\u5C0F"));
    menu.addSeparator();
    QAction *closeAction = menu.addAction(QStringLiteral("\u5173\u95ED"));

    if (qFuzzyCompare(m_zoomFactor, 1.0))
        resetZoomAction->setEnabled(false);

    QAction *chosen = menu.exec(event->globalPos());

    if (chosen == copyAction) {
        QApplication::clipboard()->setPixmap(m_originalPixmap);
    } else if (chosen == saveAction) {
        QString path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("\u4FDD\u5B58\u622A\u56FE"),
            QString(),
            QStringLiteral("PNG \u56FE\u7247 (*.png);;JPG \u56FE\u7247 (*.jpg)"));
        if (!path.isEmpty()) {
            m_originalPixmap.save(path);
        }
    } else if (chosen == resetZoomAction) {
        QPoint oldCenter = geometry().center();
        m_zoomFactor = 1.0;
        rescalePixmap();
        updateWindowSize();
        QRect newGeo = geometry();
        newGeo.moveCenter(oldCenter);
        move(newGeo.topLeft());
        update();
    } else if (chosen == closeAction) {
        close();
    }
}

void PinWindow::closeEvent(QCloseEvent *event)
{
    QWidget::closeEvent(event);
}
