#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <QWidget>
#include <QColor>
#include <QPixmap>
#include <QImage>
#include <QPoint>

class ColorPicker : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPicker(QWidget *parent = nullptr);
    void begin();

signals:
    void colorPicked(const QColor &color);
    void cancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateColorAtCursor();

    QPixmap m_screenCapture;
    QImage m_screenImage;
    qreal m_dpr = 1.0;
    QColor m_currentColor;
    QPoint m_cursorPos;
    int m_zoomFactor = 12;
    int m_zoomPixels = 11;
};

#endif // COLORPICKER_H
