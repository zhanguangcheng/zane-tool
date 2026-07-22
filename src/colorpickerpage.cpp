#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

#include <windows.h>

#include "colorpickerpage.h"
#include "colorpicker.h"

ColorPickerPage::ColorPickerPage(QWidget *parent)
    : QWidget(parent)
    , m_pickedColor(Qt::white)
    , m_colorPicker(new ColorPicker(nullptr))
{
    setupUi();

    connect(m_colorPicker, &ColorPicker::colorPicked, this, &ColorPickerPage::onColorPicked);
    connect(m_colorPicker, &ColorPicker::cancelled, this, &ColorPickerPage::onPickCancelled);
}

ColorPickerPage::~ColorPickerPage()
{
    delete m_colorPicker;
}

void ColorPickerPage::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *group = new QGroupBox(QStringLiteral("屏幕取色"), this);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(14);

    m_pickColorBtn = new QPushButton(QStringLiteral("开始取色"), group);
    m_pickColorBtn->setFixedHeight(48);
    m_pickColorBtn->setFixedWidth(200);
    m_pickColorBtn->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    connect(m_pickColorBtn, &QPushButton::clicked, this, &ColorPickerPage::onStartPick);

    QHBoxLayout *colorRow = new QHBoxLayout();
    m_colorSwatch = new QLabel(group);
    m_colorSwatch->setFixedSize(100, 80);
    m_colorSwatch->setStyleSheet(QStringLiteral(
        "background-color: #e9ecef; border: 2px solid #ced4da; border-radius: 6px;"));

    QGridLayout *infoLayout = new QGridLayout();
    infoLayout->setSpacing(6);
    infoLayout->setColumnStretch(2, 0);
    infoLayout->setColumnStretch(3, 1);

    QFont infoFont;
    infoFont.setPointSize(11);
    m_hexLabel = new QLabel(QStringLiteral("HEX:"), group);
    m_hexLabel->setFont(infoFont);
    m_hexLabel->setStyleSheet(QStringLiteral("color: #212529;"));
    m_hexValue = new QLabel(group);
    m_hexValue->setFont(infoFont);
    m_hexValue->setStyleSheet(QStringLiteral("color: #212529;"));
    m_copyHexBtn = new QPushButton(QStringLiteral("复制 HEX"), group);
    m_copyHexBtn->setEnabled(false);
    m_copyHexBtn->setFixedWidth(110);
    infoLayout->addWidget(m_hexLabel, 0, 0);
    infoLayout->addWidget(m_hexValue, 0, 1);
    infoLayout->addWidget(m_copyHexBtn, 0, 2);

    infoFont.setPointSize(9);
    m_rgbLabel = new QLabel(QStringLiteral("RGB:"), group);
    m_rgbLabel->setFont(infoFont);
    m_rgbLabel->setStyleSheet(QStringLiteral("color: #6c757d;"));
    m_rgbValue = new QLabel(group);
    m_rgbValue->setFont(infoFont);
    m_rgbValue->setStyleSheet(QStringLiteral("color: #6c757d;"));
    m_copyRgbBtn = new QPushButton(QStringLiteral("复制 RGB"), group);
    m_copyRgbBtn->setEnabled(false);
    m_copyRgbBtn->setFixedWidth(110);
    infoLayout->addWidget(m_rgbLabel, 1, 0);
    infoLayout->addWidget(m_rgbValue, 1, 1);
    infoLayout->addWidget(m_copyRgbBtn, 1, 2);

    m_hslLabel = new QLabel(QStringLiteral("HSL:"), group);
    m_hslLabel->setFont(infoFont);
    m_hslLabel->setStyleSheet(QStringLiteral("color: #6c757d;"));
    m_hslValue = new QLabel(group);
    m_hslValue->setFont(infoFont);
    m_hslValue->setStyleSheet(QStringLiteral("color: #6c757d;"));
    infoLayout->addWidget(m_hslLabel, 2, 0);
    infoLayout->addWidget(m_hslValue, 2, 1);

    connect(m_copyHexBtn, &QPushButton::clicked, this, &ColorPickerPage::onCopyHex);
    connect(m_copyRgbBtn, &QPushButton::clicked, this, &ColorPickerPage::onCopyRgb);

    colorRow->addWidget(m_colorSwatch);
    colorRow->addLayout(infoLayout);
    colorRow->addStretch();

    QLabel *historyLabel = new QLabel(QStringLiteral("最近颜色"), group);
    historyLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-weight: bold; margin-top: 4px;"));

    m_historyContainer = new QWidget(group);
    m_historyLayout = new QHBoxLayout(m_historyContainer);
    m_historyLayout->setContentsMargins(0, 0, 0, 0);
    m_historyLayout->setSpacing(6);
    m_historyLayout->addStretch();

    groupLayout->addWidget(m_pickColorBtn, 0, Qt::AlignCenter);
    groupLayout->addSpacing(8);
    groupLayout->addLayout(colorRow);
    groupLayout->addWidget(historyLabel);
    groupLayout->addWidget(m_historyContainer);
    groupLayout->addStretch();

    mainLayout->addWidget(group);
    mainLayout->addStretch();
}

void ColorPickerPage::onStartPick()
{
    QWidget *w = window();
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    ShowWindow(hwnd, SW_HIDE);
    QTimer::singleShot(200, this, [this]() {
        m_colorPicker->begin();
    });
}

void ColorPickerPage::onColorPicked(const QColor &color)
{
    m_pickedColor = color;

    m_colorHistory.removeAll(color);
    m_colorHistory.prepend(color);
    while (m_colorHistory.size() > 10)
        m_colorHistory.removeLast();

    QWidget *w = window();
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    ShowWindow(hwnd, SW_SHOW);
    w->activateWindow();
    updateColorDisplay();
    updateColorHistory();
}

void ColorPickerPage::onPickCancelled()
{
    QWidget *w = window();
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    ShowWindow(hwnd, SW_SHOW);
    w->activateWindow();
}

void ColorPickerPage::onCopyHex()
{
    QApplication::clipboard()->setText(m_pickedColor.name().toUpper());
    QString original = m_copyHexBtn->text();
    m_copyHexBtn->setText(QStringLiteral("已复制 HEX"));
    m_copyHexBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyHexBtn->setText(original);
        m_copyHexBtn->setEnabled(true);
    });
}

void ColorPickerPage::onCopyRgb()
{
    QString rgb = QStringLiteral("rgb(%1, %2, %3)")
        .arg(m_pickedColor.red())
        .arg(m_pickedColor.green())
        .arg(m_pickedColor.blue());
    QApplication::clipboard()->setText(rgb);
    QString original = m_copyRgbBtn->text();
    m_copyRgbBtn->setText(QStringLiteral("已复制 RGB"));
    m_copyRgbBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyRgbBtn->setText(original);
        m_copyRgbBtn->setEnabled(true);
    });
}

void ColorPickerPage::onHistoryColorClicked(int index)
{
    if (index < 0 || index >= m_colorHistory.size()) return;
    m_pickedColor = m_colorHistory[index];
    updateColorDisplay();
}

void ColorPickerPage::updateColorDisplay()
{
    QString colorHex = m_pickedColor.name().toUpper();
    m_colorSwatch->setStyleSheet(QStringLiteral(
        "background-color: %1; border: 2px solid #adb5bd; border-radius: 6px;").arg(colorHex));
    m_hexValue->setText(colorHex);
    m_rgbValue->setText(QStringLiteral("%1, %2, %3")
        .arg(m_pickedColor.red())
        .arg(m_pickedColor.green())
        .arg(m_pickedColor.blue()));

    int hue = m_pickedColor.hslHue();
    int sat = m_pickedColor.hslSaturation() * 100 / 255;
    int light = m_pickedColor.lightness() * 100 / 255;
    m_hslValue->setText(QStringLiteral("%1°, %2%, %3%")
        .arg(hue >= 0 ? hue : 0)
        .arg(sat)
        .arg(light));

    m_copyHexBtn->setEnabled(true);
    m_copyRgbBtn->setEnabled(true);
}

void ColorPickerPage::updateColorHistory()
{
    QLayoutItem *item;
    while ((item = m_historyLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            delete item->widget();
        delete item;
    }

    for (int i = 0; i < m_colorHistory.size(); i++) {
        QPushButton *btn = new QPushButton(m_historyContainer);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(m_colorHistory[i].name().toUpper());
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; border: 2px solid #dee2e6; border-radius: 4px; }"
            "QPushButton:hover { border-color: #0d6efd; }"
        ).arg(m_colorHistory[i].name()));

        int idx = i;
        connect(btn, &QPushButton::clicked, this, [this, idx]() {
            onHistoryColorClicked(idx);
        });

        m_historyLayout->addWidget(btn);
    }
    m_historyLayout->addStretch();
}
