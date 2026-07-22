#ifndef VIDEO_TOOL_H
#define VIDEO_TOOL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QCheckBox>

#include "videoprocessor.h"

class FFmpegProcess;

class VideoTool : public QWidget
{
    Q_OBJECT

public:
    explicit VideoTool(const QString &ffmpegPath, QWidget *parent = nullptr);

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
    void processNextVideo();
    void setVideoUiEnabled(bool enabled);
    void updateVideoInfoPreview();
    void showBatchSummary();

    QString m_ffmpegPath;

    QListWidget *m_fileList;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_clearBtn;
    QComboBox *m_formatCombo;
    QSlider *m_crfSlider;
    QLabel *m_crfLabel;
    QCheckBox *m_scaleCheck;
    QComboBox *m_presetRes;
    QLineEdit *m_outputDir;
    QPushButton *m_outputBrowse;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_startBtn;
    QPushButton *m_cancelBtn;
    QLabel *m_infoPreview;

    QList<VideoTask> m_taskQueue;
    int m_currentIndex;
    bool m_cancelling;
    qint64 m_sizeBefore;
    qint64 m_sizeAfter;
    int m_successCount;
    int m_failedCount;
    QStringList m_failedFiles;

    FFmpegProcess *m_ffmpeg;
};

#endif // VIDEO_TOOL_H
