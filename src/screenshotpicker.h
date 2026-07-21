#ifndef SCREENSHOTPICKER_H
#define SCREENSHOTPICKER_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>

class ScreenshotPicker : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenshotPicker(QWidget *parent = nullptr);
    void begin();

signals:
    void screenshotCaptured(const QPixmap &pixmap, QPoint globalPos);
    void cancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QRect normalizedSelection() const;

    QPixmap m_screenCapture;
    qreal m_dpr = 1.0;
    bool m_selecting = false;
    bool m_hasSelection = false;
    QPoint m_startPoint;
    QPoint m_endPoint;
};

#endif
