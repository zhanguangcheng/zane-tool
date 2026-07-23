#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
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
    , m_updatingFromRgb(false)
    , m_updatingFromHex(false)
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

    QGroupBox *convGroup = new QGroupBox(QStringLiteral("颜色转换"), this);
    QVBoxLayout *convLayout = new QVBoxLayout(convGroup);
    convLayout->setSpacing(10);

    QHBoxLayout *rgbRow = new QHBoxLayout();
    QLabel *convRLabel = new QLabel(QStringLiteral("R:"), convGroup);
    convRLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-weight: bold;"));
    m_convRSpin = new QSpinBox(convGroup);
    m_convRSpin->setRange(0, 255);
    m_convRSpin->setValue(255);
    m_convRSpin->setFixedWidth(64);

    QLabel *convGLabel = new QLabel(QStringLiteral("G:"), convGroup);
    convGLabel->setStyleSheet(QStringLiteral("color: #198754; font-weight: bold;"));
    m_convGSpin = new QSpinBox(convGroup);
    m_convGSpin->setRange(0, 255);
    m_convGSpin->setValue(255);
    m_convGSpin->setFixedWidth(64);

    QLabel *convBLabel = new QLabel(QStringLiteral("B:"), convGroup);
    convBLabel->setStyleSheet(QStringLiteral("color: #0d6efd; font-weight: bold;"));
    m_convBSpin = new QSpinBox(convGroup);
    m_convBSpin->setRange(0, 255);
    m_convBSpin->setValue(255);
    m_convBSpin->setFixedWidth(64);

    QLabel *rgbArrow = new QLabel(QStringLiteral("  \u2192  "), convGroup);
    rgbArrow->setStyleSheet(QStringLiteral("font-size: 16px; color: #6c757d;"));

    m_convHexResult = new QLabel(QStringLiteral("#FFFFFF"), convGroup);
    QFont monoFont(QStringLiteral("Consolas"), 11);
    m_convHexResult->setFont(monoFont);
    m_convHexResult->setStyleSheet(QStringLiteral("color: #212529; background: #f1f3f5; padding: 2px 8px; border-radius: 3px;"));

    m_convCopyHexBtn = new QPushButton(QStringLiteral("复制 HEX"), convGroup);
    m_convCopyHexBtn->setFixedWidth(110);

    rgbRow->addWidget(convRLabel);
    rgbRow->addWidget(m_convRSpin);
    rgbRow->addSpacing(6);
    rgbRow->addWidget(convGLabel);
    rgbRow->addWidget(m_convGSpin);
    rgbRow->addSpacing(6);
    rgbRow->addWidget(convBLabel);
    rgbRow->addWidget(m_convBSpin);
    rgbRow->addSpacing(8);
    rgbRow->addWidget(rgbArrow);
    rgbRow->addWidget(m_convHexResult);
    rgbRow->addWidget(m_convCopyHexBtn);
    rgbRow->addStretch();

    QHBoxLayout *hexRow = new QHBoxLayout();
    QLabel *hexPrefix = new QLabel(QStringLiteral("#"), convGroup);
    hexPrefix->setStyleSheet(QStringLiteral("font-size: 14px; color: #6c757d;"));
    m_convHexInput = new QLineEdit(convGroup);
    m_convHexInput->setPlaceholderText(QStringLiteral("输入 HEX 值"));
    m_convHexInput->setFixedWidth(140);
    m_convHexInput->setFont(monoFont);

    QLabel *hexArrow = new QLabel(QStringLiteral("  \u2192  "), convGroup);
    hexArrow->setStyleSheet(QStringLiteral("font-size: 16px; color: #6c757d;"));

    m_convRgbResult = new QLabel(QStringLiteral("rgb(255, 255, 255)"), convGroup);
    m_convRgbResult->setFont(monoFont);
    m_convRgbResult->setStyleSheet(QStringLiteral("color: #212529; background: #f1f3f5; padding: 2px 8px; border-radius: 3px;"));

    m_convCopyRgbBtn = new QPushButton(QStringLiteral("复制 RGB"), convGroup);
    m_convCopyRgbBtn->setFixedWidth(110);

    hexRow->addWidget(hexPrefix);
    hexRow->addWidget(m_convHexInput);
    hexRow->addSpacing(8);
    hexRow->addWidget(hexArrow);
    hexRow->addWidget(m_convRgbResult);
    hexRow->addWidget(m_convCopyRgbBtn);
    hexRow->addStretch();

    m_convColorPreview = new QLabel(convGroup);
    m_convColorPreview->setFixedSize(120, 36);
    m_convColorPreview->setAlignment(Qt::AlignCenter);
    m_convColorPreview->setStyleSheet(QStringLiteral(
        "background-color: #FFFFFF; border: 2px solid #ced4da; border-radius: 6px;"
        "color: #212529;"));

    QHBoxLayout *previewRow = new QHBoxLayout();
    previewRow->addStretch();
    previewRow->addWidget(m_convColorPreview);
    previewRow->addStretch();

    convLayout->addLayout(rgbRow);
    convLayout->addLayout(hexRow);
    convLayout->addLayout(previewRow);

    mainLayout->addWidget(convGroup);

    connect(m_convRSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerPage::onConvRgbChanged);
    connect(m_convGSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerPage::onConvRgbChanged);
    connect(m_convBSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerPage::onConvRgbChanged);
    connect(m_convHexInput, &QLineEdit::textEdited, this, &ColorPickerPage::onConvHexChanged);
    connect(m_convCopyHexBtn, &QPushButton::clicked, this, &ColorPickerPage::onCopyConvHex);
    connect(m_convCopyRgbBtn, &QPushButton::clicked, this, &ColorPickerPage::onCopyConvRgb);

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

void ColorPickerPage::onConvRgbChanged()
{
    if (m_updatingFromHex) return;
    m_updatingFromRgb = true;

    int r = m_convRSpin->value();
    int g = m_convGSpin->value();
    int b = m_convBSpin->value();

    QString hex = QStringLiteral("#%1%2%3")
        .arg(r, 2, 16, QLatin1Char('0'))
        .arg(g, 2, 16, QLatin1Char('0'))
        .arg(b, 2, 16, QLatin1Char('0'))
        .toUpper();

    m_convHexResult->setText(hex);
    m_convHexInput->setText(hex.mid(1));
    m_convRgbResult->setText(QStringLiteral("rgb(%1, %2, %3)").arg(r).arg(g).arg(b));

    m_convColorPreview->setStyleSheet(QStringLiteral(
        "background-color: %1; border: 2px solid #adb5bd; border-radius: 6px;"
        "color: %2;"
    ).arg(hex).arg((r + g + b) / 3 > 128 ? QStringLiteral("#212529") : QStringLiteral("#f8f9fa")));

    m_updatingFromRgb = false;
}

void ColorPickerPage::onConvHexChanged()
{
    if (m_updatingFromRgb) return;
    QString text = m_convHexInput->text().simplified();
    if (text.startsWith(QLatin1Char('#')))
        text = text.mid(1);

    if (text.isEmpty()) {
        m_convHexResult->setText(QStringLiteral("-"));
        m_convRgbResult->setText(QStringLiteral("-"));
        m_convColorPreview->setStyleSheet(QStringLiteral(
            "background-color: #e9ecef; border: 2px solid #ced4da; border-radius: 6px;"
            "color: #212529;"));
        return;
    }

    int len = text.length();
    QString hex;
    if (len == 3) {
        hex = QStringLiteral("#%1%1%2%2%3%3")
            .arg(text[0]).arg(text[1]).arg(text[2]);
    } else if (len == 6) {
        hex = QStringLiteral("#") + text;
    } else {
        return;
    }

    bool ok;
    int rgb = hex.mid(1).toInt(&ok, 16);
    if (!ok) return;

    int r = (rgb >> 16) & 0xFF;
    int g = (rgb >> 8) & 0xFF;
    int b = rgb & 0xFF;

    m_updatingFromHex = true;

    m_convRSpin->setValue(r);
    m_convGSpin->setValue(g);
    m_convBSpin->setValue(b);

    hex = hex.toUpper();
    m_convHexResult->setText(hex);
    m_convRgbResult->setText(QStringLiteral("rgb(%1, %2, %3)").arg(r).arg(g).arg(b));

    m_convColorPreview->setStyleSheet(QStringLiteral(
        "background-color: %1; border: 2px solid #adb5bd; border-radius: 6px;"
        "color: %2;"
    ).arg(hex).arg((r + g + b) / 3 > 128 ? QStringLiteral("#212529") : QStringLiteral("#f8f9fa")));

    m_updatingFromHex = false;
}

void ColorPickerPage::onCopyConvHex()
{
    QString text = m_convHexResult->text();
    if (text == QStringLiteral("-")) return;
    QApplication::clipboard()->setText(text);
    QString original = m_convCopyHexBtn->text();
    m_convCopyHexBtn->setText(QStringLiteral("已复制"));
    m_convCopyHexBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_convCopyHexBtn->setText(original);
        m_convCopyHexBtn->setEnabled(true);
    });
}

void ColorPickerPage::onCopyConvRgb()
{
    QString text = m_convRgbResult->text();
    if (text == QStringLiteral("-")) return;
    QApplication::clipboard()->setText(text);
    QString original = m_convCopyRgbBtn->text();
    m_convCopyRgbBtn->setText(QStringLiteral("已复制"));
    m_convCopyRgbBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_convCopyRgbBtn->setText(original);
        m_convCopyRgbBtn->setEnabled(true);
    });
}
