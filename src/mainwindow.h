#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>

#include <QDragEnterEvent>
#include <QDropEvent>

#include "imageprocessor.h"
#include "videoprocessor.h"

class FFmpegProcess;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &ffmpegPath, QWidget *parent = nullptr);

private slots:
    void onImageAddFiles();
    void onImageRemoveSelected();
    void onImageClearFiles();
    void onImageOutputBrowse();
    void onImageStart();
    void onImageCancel();

    void onVideoAddFiles();
    void onVideoRemoveSelected();
    void onVideoClearFiles();
    void onVideoOutputBrowse();
    void onVideoStart();
    void onVideoCancel();

    void onImageSelectionChanged();
    void onVideoSelectionChanged();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setupUi();
    QWidget *createImageTab();
    QWidget *createVideoTab();

    void processNextImage();
    void processNextVideo();
    void showBatchSummary(const QString &type, int total, int success, int failed,
                          qint64 sizeBefore, qint64 sizeAfter, const QStringList &failedFiles);
    void showAbout();

    void setImageUiEnabled(bool enabled);
    void setVideoUiEnabled(bool enabled);
    void updateImageResolutionPreview();
    void updateVideoInfoPreview();

    QString m_ffmpegPath;
    QTabWidget *m_tabWidget;

    // ---- Image tab widgets ----
    QListWidget *m_imageFileList;
    QPushButton *m_imageAddBtn;
    QPushButton *m_imageRemoveBtn;
    QPushButton *m_imageClearBtn;
    QSlider *m_imageQualitySlider;
    QLabel *m_imageQualityLabel;
    QCheckBox *m_imageScaleCheck;
    QSpinBox *m_imageScaleWidth;
    QComboBox *m_imageFormatCombo;
    QLineEdit *m_imageOutputDir;
    QPushButton *m_imageOutputBrowse;
    QProgressBar *m_imageProgressBar;
    QLabel *m_imageStatusLabel;
    QPushButton *m_imageStartBtn;
    QPushButton *m_imageCancelBtn;
    QLabel *m_imageResPreview;

    QLabel *m_aboutLabel;

    // ---- Video tab widgets ----
    QListWidget *m_videoFileList;
    QPushButton *m_videoAddBtn;
    QPushButton *m_videoRemoveBtn;
    QPushButton *m_videoClearBtn;
    QComboBox *m_videoFormatCombo;
    QSlider *m_videoCrfSlider;
    QLabel *m_videoCrfLabel;
    QCheckBox *m_videoScaleCheck;
    QComboBox *m_videoPresetRes;
    QLineEdit *m_videoOutputDir;
    QPushButton *m_videoOutputBrowse;
    QProgressBar *m_videoProgressBar;
    QLabel *m_videoStatusLabel;
    QPushButton *m_videoStartBtn;
    QPushButton *m_videoCancelBtn;
    QLabel *m_videoInfoPreview;

    // ---- Task queues ----
    QList<ImageTask> m_imageTaskQueue;
    QList<VideoTask> m_videoTaskQueue;
    int m_imageCurrentIndex;
    int m_videoCurrentIndex;
    bool m_imageCancelling;
    bool m_videoCancelling;

    // ---- Statistics ----
    qint64 m_imageSizeBefore;
    qint64 m_imageSizeAfter;
    int m_imageSuccessCount;
    int m_imageFailedCount;
    QStringList m_imageFailedFiles;

    qint64 m_videoSizeBefore;
    qint64 m_videoSizeAfter;
    int m_videoSuccessCount;
    int m_videoFailedCount;
    QStringList m_videoFailedFiles;

    FFmpegProcess *m_ffmpegImage;
    FFmpegProcess *m_ffmpegVideo;
};

#endif // MAINWINDOW_H
