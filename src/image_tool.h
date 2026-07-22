#ifndef IMAGE_TOOL_H
#define IMAGE_TOOL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QSpinBox>

#include "imageprocessor.h"

class FFmpegProcess;

class ImageTool : public QWidget
{
    Q_OBJECT

public:
    explicit ImageTool(const QString &ffmpegPath, QWidget *parent = nullptr);

    QListWidget *fileListWidget() const { return m_fileList; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onAddFiles();
    void onRemoveSelected();
    void onClearFiles();
    void onOutputBrowse();
    void onStart();
    void onCancel();
    void onSelectionChanged();

private:
    void setupUi();
    void processNextImage();
    void setImageUiEnabled(bool enabled);
    void updateImageResolutionPreview();
    void showBatchSummary();

    QString m_ffmpegPath;

    QListWidget *m_fileList;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_clearBtn;
    QSlider *m_qualitySlider;
    QLabel *m_qualityLabel;
    QCheckBox *m_scaleCheck;
    QSpinBox *m_scaleWidth;
    QComboBox *m_formatCombo;
    QLineEdit *m_outputDir;
    QPushButton *m_outputBrowse;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_startBtn;
    QPushButton *m_cancelBtn;
    QLabel *m_resPreview;

    QList<ImageTask> m_taskQueue;
    int m_currentIndex;
    bool m_cancelling;
    qint64 m_sizeBefore;
    qint64 m_sizeAfter;
    int m_successCount;
    int m_failedCount;
    QStringList m_failedFiles;

    FFmpegProcess *m_ffmpeg;
};

#endif // IMAGE_TOOL_H
