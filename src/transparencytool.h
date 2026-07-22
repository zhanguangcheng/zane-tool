#ifndef TRANSPARENCYTOOL_H
#define TRANSPARENCYTOOL_H

#include <QObject>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

#include <windows.h>

class WindowPicker;

class TransparencyTool : public QObject
{
    Q_OBJECT
public:
    explicit TransparencyTool(QWidget *mainWidget, QObject *parent = nullptr);

    QWidget *createPage();

private slots:
    void onPickCancelled();
    void onWindowPicked(HWND hwnd);
    void onTransparencyRefresh();
    void onTransparencySelectionChanged();
    void onTransparencySliderChanged(int value);
    void onTransparencyPickWindow();

private:
    QWidget *m_mainWidget;
    WindowPicker *m_windowPicker;

    QListWidget *m_transparencyWindowList;
    QPushButton *m_transparencyRefreshBtn;
    QPushButton *m_transparencyPickBtn;
    QSlider *m_transparencySlider;
    QLabel *m_transparencyValueLabel;
    QLabel *m_transparencyStatusLabel;
    HWND m_transparencyTargetHwnd = nullptr;
    LONG_PTR m_transparencyOriginalExStyle = 0;
};

#endif // TRANSPARENCYTOOL_H
