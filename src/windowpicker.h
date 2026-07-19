#ifndef WINDOWPICKER_H
#define WINDOWPICKER_H

#include <QWidget>
#include <QPoint>
#include <QTimer>

#include <windows.h>

class WindowPicker : public QWidget
{
    Q_OBJECT

public:
    explicit WindowPicker(QWidget *parent = nullptr);
    ~WindowPicker() override;
    void begin();

signals:
    void windowPicked(HWND hwnd);
    void cancelled();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void updateCurrentWindow();
    void handleLeftClick();
    QRect totalDesktopRect() const;

    QPoint m_cursorPos;
    HWND m_currentHwnd;
    HWND m_overlayHwnd;
    QTimer *m_trackTimer;
};

#endif // WINDOWPICKER_H
