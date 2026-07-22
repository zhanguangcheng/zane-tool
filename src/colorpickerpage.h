#ifndef COLORPICKERPAGE_H
#define COLORPICKERPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QList>
#include <QColor>

class ColorPicker;

class ColorPickerPage : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPickerPage(QWidget *parent = nullptr);
    ~ColorPickerPage();

private slots:
    void onStartPick();
    void onColorPicked(const QColor &color);
    void onPickCancelled();
    void onCopyHex();
    void onCopyRgb();
    void onHistoryColorClicked(int index);

private:
    void setupUi();
    void updateColorDisplay();
    void updateColorHistory();

    ColorPicker *m_colorPicker;

    QPushButton *m_pickColorBtn;
    QLabel *m_colorSwatch;
    QLabel *m_hexLabel;
    QLabel *m_hexValue;
    QLabel *m_rgbLabel;
    QLabel *m_rgbValue;
    QLabel *m_hslLabel;
    QLabel *m_hslValue;
    QPushButton *m_copyHexBtn;
    QPushButton *m_copyRgbBtn;
    QWidget *m_historyContainer;
    QHBoxLayout *m_historyLayout;

    QColor m_pickedColor;
    QList<QColor> m_colorHistory;
};

#endif // COLORPICKERPAGE_H
