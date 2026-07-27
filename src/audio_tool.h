#ifndef AUDIO_TOOL_H
#define AUDIO_TOOL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>

#include "audioprocessor.h"

class FFmpegProcess;

class AudioTool : public QWidget
{
    Q_OBJECT

public:
    explicit AudioTool(const QString &ffmpegPath, QWidget *parent = nullptr);

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
    void ensureFfmpeg();
    void processNextAudio();
    void setAudioUiEnabled(bool enabled);
    void updateAudioInfoPreview();
    void showBatchSummary();

    QString m_ffmpegPath;

    QListWidget *m_fileList;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_clearBtn;
    QComboBox *m_formatCombo;
    QComboBox *m_bitrateCombo;
    QComboBox *m_sampleRateCombo;
    QComboBox *m_channelsCombo;
    QLineEdit *m_outputDir;
    QPushButton *m_outputBrowse;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_startBtn;
    QPushButton *m_cancelBtn;
    QLabel *m_infoPreview;

    QList<AudioTask> m_taskQueue;
    int m_currentIndex;
    bool m_cancelling;
    qint64 m_sizeBefore;
    qint64 m_sizeAfter;
    int m_successCount;
    int m_failedCount;
    QStringList m_failedFiles;

    FFmpegProcess *m_ffmpeg;
};

#endif // AUDIO_TOOL_H
