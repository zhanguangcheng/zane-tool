# AGENTS.md

## 项目

Zane Tool — 基于 Qt6 Widgets 的桌面应用（C++17，仅 Win32，MinGW GCC 13.1），通过子进程调用 `ffmpeg.exe` 实现图片和视频的批量压缩、缩放和格式转换。

## 构建与运行

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/c++.exe"

cmake --build build --target ZaneTool

# 构建后部署 Qt 运行时 DLL：
windeployqt build\ZaneTool.exe --no-translations --no-opengl-sw
```

无测试、无 linter、无 CI 配置。

## 架构

```
main.cpp          → QApplication + 全局 QSS（内嵌原始字符串）
MainWindow        → UI（标签页、控件）+ 批量队列编排
ImageProcessor    → 静态：构建图片 ffmpeg 参数 + 输出路径
VideoProcessor    → 静态：构建视频 ffmpeg 参数 + 输出路径
FFmpegProcess     → QProcess 封装（start/cancel/isRunning），含 finished/errorOccurred 信号
Utils             → 静态：文件大小格式化、格式检测、日志
```

- **单进程模型**：每个标签页同时运行一个 ffmpeg 子进程。`QList<ImageTask>` / `QList<VideoTask>` 串行队列。
- **进度**：文件级别 `(currentIndex / total) × 100`，非基于时长。
- **取消流程**：设置取消标志 → 终止 QProcess → 等待最多 3s → 恢复 UI。
- **日志**：`Utils::logToFile()` 写入相对于运行时 exe 目录的 `logs/app.log`。

## 关键实现细节

- `ffmpeg.exe` 与 exe 放在同级目录；`main.cpp` 在启动时检查，若缺失则弹出致命错误对话框。
- 全局 QSS 以原始字符串字面量形式写在 `src/main.cpp:16-139`。所有控件样式均在此处。
- `ImageTask::outputName` 和 `VideoTask::outputName` **从未被 UI 设置**（始终为空）；`buildOutputPath` 回退使用 `fi.completeBaseName()`。
- 视频格式键 `"mp4_hevc"` 映射到 `.mp4` 容器，编码器为 `libx265` — 与普通 `"mp4"` 容器扩展名相同。
- JPEG 扩展名全局统一为 `"jpg"`；MKV 输入默认输出为 MP4。
- 图片质量滑块（0–100）通过 `qualityToQScale` 反向映射到 ffmpeg `-q:v`（31–2）。视频 AVI 格式将 CRF 单独映射到 `-q:v`（1–31）。
- 窗口图标：运行时通过 `resources.qrc` 使用 SVG；Windows 资源管理器通过 `app.rc` 使用 ICO。
- `installer/ZaneTool/` 是 NSIS 打包临时目录（已 gitignore）。NSIS 脚本在 `installer/installer.nsi`，输出到 `dist/`。
- `build/` 和 `logs/` 目录已 gitignore。
