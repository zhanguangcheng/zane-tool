#ifndef QRCODETOOL_H
#define QRCODETOOL_H

#include <QObject>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QPixmap>

class ScreenshotTool;

class QrCodeTool : public QObject
{
    Q_OBJECT
public:
    explicit QrCodeTool(ScreenshotTool *screenshotTool, QObject *parent = nullptr);

    QWidget *createPage();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onGenerate();
    void onSaveImage();
    void onCopyImage();
    void onSelectImage();
    void onScreenCapture();
    void onCopyResult();
    void onOpenLink();

private:
    void processQrDecodeImage(const QImage &image, const QString &sourceDesc);

    ScreenshotTool *m_screenshotTool;

    QTextEdit *m_genInput;
    QComboBox *m_genEccCombo;
    QSpinBox *m_genScaleSpin;
    QLabel *m_genPreview;
    QLabel *m_genStatusLabel;
    QPushButton *m_genSaveBtn;
    QPushButton *m_genCopyBtn;
    QPixmap m_genPixmap;

    QLineEdit *m_decFilePath;
    QPushButton *m_decSelectBtn;
    QPushButton *m_decScreenBtn;
    QLabel *m_decDropZone;
    QLabel *m_decPreview;
    QTextEdit *m_decOutput;
    QLabel *m_decInfoLabel;
    QPushButton *m_decCopyBtn;
    QPushButton *m_decOpenBtn;
};

#endif // QRCODETOOL_H
