#ifndef PINWINDOW_H
#define PINWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>

class PinWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PinWindow(const QPixmap &pixmap, QPoint globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void rescalePixmap();
    void updateWindowSize();

    QPixmap m_originalPixmap;
    QPixmap m_scaledPixmap;
    qreal m_zoomFactor = 1.0;
    bool m_dragging = false;
    QPoint m_dragOffset;
    QRect m_closeBtnRect;

    static constexpr int BORDER_WIDTH = 2;
    static constexpr int CLOSE_BTN_SIZE = 24;
    static constexpr qreal MIN_ZOOM = 0.1;
    static constexpr qreal MAX_ZOOM = 4.0;
    static constexpr qreal ZOOM_STEP = 0.1;
};

#endif
