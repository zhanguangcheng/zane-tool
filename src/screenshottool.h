#ifndef SCREENSHOTTOOL_H
#define SCREENSHOTTOOL_H

#include <QObject>
#include <QList>
#include <QLabel>

#include <windows.h>

class ScreenshotPicker;
class PinWindow;
class QWidget;

class ScreenshotTool : public QObject
{
    Q_OBJECT
public:
    explicit ScreenshotTool(QWidget *mainWidget, QObject *parent = nullptr);

    QWidget *createPage();
    void startScreenshot();
    void startScreenshotForQr();

    void loadConfig();
    void registerHotkey();
    void unregisterHotkey();
    WPARAM hotkeyId() const;

signals:
    void qrScreenshotCaptured(const QImage &image);

private slots:
    void onScreenshotCaptured(const QPixmap &pixmap, QPoint globalPos);
    void onPickerCancelled();
    void onChangeScreenshotHotkey();

private:
    QString hotkeyDisplayText() const;
    void updateHotkeyLabel();

    QWidget *m_mainWidget;
    ScreenshotPicker *m_picker;
    QList<PinWindow *> m_pinnedWindows;
    QLabel *m_hotkeyLabel;

    WPARAM m_hotkeyId = 1;
    UINT m_hotkeyVk = VK_F4;
    UINT m_hotkeyModifiers = 0;
    bool m_screenshotForQr = false;
};

#endif // SCREENSHOTTOOL_H
