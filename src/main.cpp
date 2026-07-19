#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QIcon>

#include "mainwindow.h"
#include "utils.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Zane Tool"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/resources/app-icon.svg")));

    const QString qss = QStringLiteral(R"(
        QMainWindow {
            background-color: #f8f9fa;
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid #dee2e6;
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 12px;
            background-color: #ffffff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #212529;
        }
        QPushButton {
            background-color: #0d6efd;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 6px 14px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #0b5ed7;
        }
        QPushButton:pressed {
            background-color: #0a58ca;
        }
        QPushButton:disabled {
            background-color: #6c757d;
            color: #ced4da;
        }
        QPushButton#dangerBtn {
            background-color: #dc3545;
        }
        QPushButton#dangerBtn:hover {
            background-color: #bb2d3b;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #dee2e6;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #0d6efd;
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
        QSlider::sub-page:horizontal {
            background: #0d6efd;
            border-radius: 3px;
        }
        QLineEdit, QSpinBox, QComboBox {
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 4px 8px;
            background-color: #ffffff;
            color: #212529;
            font-size: 13px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #86b7fe;
            outline: none;
        }
        QListWidget {
            border: 1px solid #ced4da;
            border-radius: 4px;
            background-color: #ffffff;
            color: #212529;
            font-size: 13px;
        }
        QListWidget::item:selected {
            background-color: #0d6efd;
            color: #ffffff;
        }
        QProgressBar {
            border: 1px solid #ced4da;
            border-radius: 4px;
            background-color: #e9ecef;
            text-align: center;
            color: #212529;
            font-size: 12px;
        }
        QProgressBar::chunk {
            background-color: #0d6efd;
            border-radius: 3px;
        }
        QListWidget#sidebar {
            background-color: #ffffff;
            border: none;
            border-right: 1px solid #dee2e6;
            font-size: 13px;
            padding: 8px 0;
            outline: none;
        }
        QListWidget#sidebar::item {
            padding: 7px 0px;
            color: #212529;
        }
        QListWidget#sidebar::item:selected {
            background-color: #e7f1ff;
            color: #0d6efd;
        }
        QStackedWidget {
            background-color: #f8f9fa;
        }
        QLabel {
            color: #212529;
            font-size: 13px;
        }
        QCheckBox {
            color: #212529;
            font-size: 13px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
        }
    )");

    app.setStyleSheet(qss);

    Utils::initLogging();

    QString ffmpegPath = QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg.exe");
    if (!QFileInfo::exists(ffmpegPath)) {
        QMessageBox::critical(nullptr, QStringLiteral("错误"),
                              QStringLiteral("未找到 ffmpeg.exe\n\n请确保 ffmpeg.exe 位于程序同级目录。\n\n路径: %1").arg(ffmpegPath));
        return 1;
    }

    MainWindow window(ffmpegPath);
    window.show();

    return app.exec();
}
