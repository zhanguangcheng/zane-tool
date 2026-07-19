#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>

#include <windows.h>

#include <QDragEnterEvent>
#include <QDropEvent>

#include "imageprocessor.h"
#include "videoprocessor.h"
#include "audioprocessor.h"

class FFmpegProcess;
class ColorPicker;
class WindowPicker;

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

    void onAudioAddFiles();
    void onAudioRemoveSelected();
    void onAudioClearFiles();
    void onAudioOutputBrowse();
    void onAudioStart();
    void onAudioCancel();

    void onImageSelectionChanged();
    void onVideoSelectionChanged();
    void onAudioSelectionChanged();

    void onStartPickColor();
    void onColorPicked(const QColor &color);
    void onPickCancelled();
    void onCopyHex();
    void onCopyRgb();
    void onHistoryColorClicked(int index);

    void onTransparencyRefresh();
    void onTransparencySelectionChanged();
    void onTransparencySliderChanged(int value);
    void onTransparencyPickWindow();
    void onWindowPicked(HWND hwnd);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setupUi();
    void setupSidebar();
    QWidget *createImageTab();
    QWidget *createVideoTab();
    QWidget *createAudioTab();

    QWidget *createColorPickerPage();
    QWidget *createStickyNotePage();
    QWidget *createTransparencyPage();
    QWidget *createTimerPage();
    QWidget *createBase64Page();
    QWidget *createTimestampPage();
    QWidget *createCronPage();
    QWidget *createJwtPage();
    QWidget *createDownloadPage();

    void processNextImage();
    void processNextVideo();
    void processNextAudio();
    void showBatchSummary(const QString &type, int total, int success, int failed,
                          qint64 sizeBefore, qint64 sizeAfter, const QStringList &failedFiles);
    void showAbout();

    void setImageUiEnabled(bool enabled);
    void setVideoUiEnabled(bool enabled);
    void setAudioUiEnabled(bool enabled);
    void updateImageResolutionPreview();
    void updateVideoInfoPreview();
    void updateAudioInfoPreview();
    void updateColorDisplay();
    void updateColorHistory();

    QString m_ffmpegPath;
    QListWidget *m_sidebar;
    QStackedWidget *m_stackedWidget;

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

    // ---- Audio tab widgets ----
    QListWidget *m_audioFileList;
    QPushButton *m_audioAddBtn;
    QPushButton *m_audioRemoveBtn;
    QPushButton *m_audioClearBtn;
    QComboBox *m_audioFormatCombo;
    QComboBox *m_audioBitrateCombo;
    QComboBox *m_audioSampleRateCombo;
    QComboBox *m_audioChannelsCombo;
    QLineEdit *m_audioOutputDir;
    QPushButton *m_audioOutputBrowse;
    QProgressBar *m_audioProgressBar;
    QLabel *m_audioStatusLabel;
    QPushButton *m_audioStartBtn;
    QPushButton *m_audioCancelBtn;
    QLabel *m_audioInfoPreview;

    // ---- ColorPicker tab widgets ----
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
    ColorPicker *m_colorPicker;

    // ---- Transparency tab widgets ----
    QListWidget *m_transparencyWindowList;
    QPushButton *m_transparencyRefreshBtn;
    QPushButton *m_transparencyPickBtn;
    QSlider *m_transparencySlider;
    QLabel *m_transparencyValueLabel;
    QLabel *m_transparencyStatusLabel;
    HWND m_transparencyTargetHwnd;
    LONG_PTR m_transparencyOriginalExStyle;
    WindowPicker *m_windowPicker;

    // ---- Task queues ----
    QList<ImageTask> m_imageTaskQueue;
    QList<VideoTask> m_videoTaskQueue;
    QList<AudioTask> m_audioTaskQueue;
    int m_imageCurrentIndex;
    int m_videoCurrentIndex;
    int m_audioCurrentIndex;
    bool m_imageCancelling;
    bool m_videoCancelling;
    bool m_audioCancelling;

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

    qint64 m_audioSizeBefore;
    qint64 m_audioSizeAfter;
    int m_audioSuccessCount;
    int m_audioFailedCount;
    QStringList m_audioFailedFiles;

    FFmpegProcess *m_ffmpegImage;
    FFmpegProcess *m_ffmpegVideo;
    FFmpegProcess *m_ffmpegAudio;
};

#endif // MAINWINDOW_H
