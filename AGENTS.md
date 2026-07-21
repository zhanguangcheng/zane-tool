# AGENTS.md

## 项目

Zane Tool — 基于 Qt6 Widgets 的桌面应用（C++17，仅 Win32，MinGW GCC 13.1），通过子进程调用 `ffmpeg.exe` 实现图片/视频/音频批量处理，同时包含屏幕取色、截图贴图、窗口透明、计时器、Base64 编码、时间戳转换、Cron 解析、JWT 解析、随机字符串生成、二维码工具、批量下载等工具。

## 构建与运行

```powershell
C:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH="C:/qt/6.8.3/mingw_64" `
  -DCMAKE_C_COMPILER="C:/qt/Tools/mingw1310_64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="C:/qt/Tools/mingw1310_64/bin/c++.exe" `
  -DCMAKE_RC_COMPILER="C:/qt/Tools/mingw1310_64/bin/windres.exe"

C:\Qt\Tools\CMake_64\bin\cmake.exe --build build --target ZaneTool

# 构建后部署 Qt 运行时 DLL：
C:\qt\6.8.3\mingw_64\bin\windeployqt.exe build\ZaneTool.exe --no-translations --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --no-quick-import --skip-plugin-types generic

# 删除不需要的插件（网络信息监控非 Widgets 应用必需）：
Remove-Item -LiteralPath "build\networkinformation" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "build\tls\qcertonlybackend.dll" -Force -ErrorAction SilentlyContinue
```

无测试、无 linter、无 CI 配置。

## 架构

```
main.cpp          → QApplication + 全局 QSS（内嵌原始字符串）+ ffmpeg/aria2 存在性检查
MainWindow        → 侧边栏 + QStackedWidget（14 页）+ 批量队列编排 + 所有工具 UI/逻辑
ImageProcessor    → 静态：构建图片 ffmpeg 参数 + 输出路径
VideoProcessor    → 静态：构建视频 ffmpeg 参数 + 输出路径
AudioProcessor    → 静态：构建音频 ffmpeg 参数 + 输出路径
FFmpegProcess     → QProcess 封装（start/cancel/isRunning），含 finished/errorOccurred 信号
ColorPicker       → 全屏取色器 Widget，信号 colorPicked(QColor) / cancelled()
WindowPicker      → 窗口选择器 Widget（全局钩子），信号 windowPicked(HWND) / cancelled()
ScreenshotPicker  → 区域截图 Widget，信号 screenshotCaptured(QPixmap, QPoint) / cancelled()
PinWindow         → 置顶贴图窗口 Widget（独立 QWidget，无 parent）
StopwatchTimer    → 秒表引擎 QObject（QTimer 10ms 轮询 + QElapsedTimer）
Utils             → 静态：文件大小格式化、格式检测、日志
```

- **单进程模型**：图片/视频/音频三个标签页各维护一个 ffmpeg 子进程。`QList<ImageTask>` / `QList<VideoTask>` / `QList<AudioTask>` 串行队列。
- **进度**：文件级别 `(currentIndex / total) × 100`，非基于时长。
- **取消流程**：设置取消标志 → 终止 QProcess → 等待最多 3s → 恢复 UI。
- **日志**：`Utils::logToFile()` 写入相对于运行时 exe 目录的 `logs/app.log`。

## 侧边栏与标签页

```
媒体工具
  图片处理 (0)    — createImageTab()
  视频处理 (1)    — createVideoTab()
  音频处理 (2)    — createAudioTab()
系统工具
  屏幕取色 (3)    — createColorPickerPage()
  截图贴图 (4)    — createStickyNotePage()
  窗口透明 (5)    — createTransparencyPage()
  计时器   (6)    — createTimerPage()
开发工具
  图片转Base64 (7)  — createBase64Page()
  时间戳转换   (8)  — createTimestampPage()
  定时任务     (9)  — createCronPage()
  JWT 解析    (10)  — createJwtPage()
  随机字符串   (12)  — createRandomStringPage()
  二维码工具  (13)  — createQrCodePage()
网络工具
  批量下载    (11)  — createDownloadPage()
```

侧边栏使用 `QListWidget#sidebar`，项目通过 `Qt::UserRole` 存储页面索引。选中切换 `QStackedWidget` 页面。

## 关键实现细节

- `ffmpeg.exe` 与 exe 放在同级目录；`main.cpp` 在启动时检查，若缺失则弹出致命错误对话框。
- 全局 QSS 以原始字符串字面量形式写在 `src/main.cpp:16-139`。所有控件样式均在此处。
- `ImageTask::outputName` 和 `VideoTask::outputName` 和 `AudioTask::outputName` **从未被 UI 设置**（始终为空）；`buildOutputPath` 回退使用 `fi.completeBaseName()`。
- 视频格式键 `"mp4_hevc"` 映射到 `.mp4` 容器，编码器为 `libx265` — 与普通 `"mp4"` 容器扩展名相同。
- JPEG 扩展名全局统一为 `"jpg"`；MKV 输入默认输出为 MP4。
- 图片质量滑块（0–100）通过 `qualityToQScale` 反向映射到 ffmpeg `-q:v`（31–2）。视频 AVI 格式将 CRF 单独映射到 `-q:v`（1–31）。
- 窗口图标：运行时通过 `resources.qrc` 使用 SVG；Windows 资源管理器通过 `app.rc` 使用 ICO。
- `installer/ZaneTool/` 是 NSIS 打包临时目录（已 gitignore）。NSIS 脚本在 `installer/installer.nsi`，输出到 `dist/`。
- `build/` 和 `logs/` 目录已 gitignore。
- 二维码：生成用 nayuki QR-Code-generator（`src/third_party/qrcodegen.{hpp,cpp}`，MIT），识别用 Quirc（`src/third_party/quirc.{h,c}` + `decode.c`/`identify.c`/`version_db.c`，ISC），均 vendor 进源码树，无外部依赖。CMake 需启用 `LANGUAGES C CXX`（Quirc 是纯 C）。屏幕识别复用 `m_screenshotPicker`，通过 `m_screenshotForQr` 标志在 `onScreenshotCaptured` 中分流到 `processQrDecodeImage()`。

## 复制到剪贴板模式

多处置复制按钮（取色、Base64、时间戳）遵循统一模式：

```cpp
QApplication::clipboard()->setText(text);
QString original = btn->text();
btn->setText("已复制");
btn->setEnabled(false);
QTimer::singleShot(1500, [btn, original]() {
    btn->setText(original);
    btn->setEnabled(true);
});
```

## 时间戳转换细节

- `m_timestampTimer` (200ms QTimer) 实时刷新当前时间戳和本地时间
- 时间戳输入使用 `QRegularExpressionValidator("-?\\d+")` 仅允许数字和负号
- `QButtonGroup` 管理秒/毫秒单选切换，自动触发重新转换
- `QDateTimeEdit` 带日历弹窗 (calendarPopup)，`dateTimeChanged` 信号实时更新秒/毫秒输出
- 时间戳→日期转换：`QDateTime::fromSecsSinceEpoch()` / `fromMSecsSinceEpoch()`
- 日期→时间戳转换：`QDateTime::toSecsSinceEpoch()` / `toMSecsSinceEpoch()`

## 批量下载细节

- `aria2c.exe` 与 exe 同级目录，`main.cpp` 启动时与 ffmpeg 一起检查
- `MainWindow` 构造函数新增 `aria2Path` 参数
- 用户通过 `QTextEdit` 输入 URL，每行一个，可在 URL 后加空格 + 自定义文件名
  - 格式：`http://example.com/file.zip` 或 `http://example.com/data 自定义名.zip`
  - 自定义文件名通过 aria2c `-i` 格式的 `out=` 选项实现
- 启动下载时：
  1. 解析 URL 列表 → `m_downloadEntries`
  2. 写入临时文件 `%TEMP%/zane_download_urls.txt`（aria2c `-i` 格式）
  3. 预填充 `QTableWidget`（文件名/进度条/速度/ETA/状态）
  4. 通过 `QProcess` 启动 aria2c，参数含 `--max-concurrent-downloads`, `--max-connection-per-server`, `--max-overall-download-limit`, `--allow-overwrite`, `--console-log-level=notice`, `--summary-interval=1`
- 进度解析：通过 `QProcess::readyReadStandardOutput` 信号实时读取 stdout
  - 正则 `\[#(\d+)\s+\S+/(\S+)\((\d+)%\).*SPD:(\S+)(?:\s*ETA:(\S*))?\]` 匹配进度行
  - GID 数字与 `m_downloadEntries` 索引一一对应（aria2c 顺序分配 GID）
  - 每行进度更新对应行的 `QProgressBar`（cellWidget）、速度、ETA、状态
  - 百分比=100% 时标记为"已完成"
- "Downloading:" 行用于将等待中的条目切换为"下载中"
- 取消流程：设置 `m_downloadCancelling` → `QProcess::kill()` → `waitForFinished(3000)` → 清理临时文件
- 完成后：exitCode=0 则标记剩余条目为"已完成"（兼容 aria2c 可能未输出 100%），≠0 则标"失败"
- 进度条是每行嵌入的 `QProgressBar`（`QTableWidget::setCellWidget`），总进度条是各行的平均值
- 输出目录默认为 `./downloads/`，自动创建
- `setDownloadUiEnabled(false/true)` 控制输入控件和按钮的启用/禁用切换

## Cron 解析细节

- Cron 表达式输入使用等宽字体 (`Consolas`)，placeholder 提示 `分 时 日 月 周` 格式
- 预设下拉菜单：自定义 / 每1分钟 / 每5分钟 / 每15分钟 / 每30分钟 / 每小时 / 每天零点 / 每周一零点 / 每月1号零点 / 工作日每小时
- 选择预设自动填充表达式并触发解析
- 字段解析：分钟/小时/日期/月份/星期 分别高亮显示当前值；若表达式无效则标红错误信息
- 未来执行时间表 (`QTableWidget`)：显示序号 / 执行时间 / 相对时间三列
- 执行次数可选 5/10/20/50 次（默认 10）
- `m_cronTimer` (30s QTimer) 自动刷新相对时间列（显示"X 分钟后"等）
- 复制全部按钮：将所有执行时间按行复制到剪贴板

## JWT 解析细节

- JWT 输入区域 (`QTextEdit`) 带 Consolas 等宽字体
- 解析时按 `.` 分隔 header / payload / signature 三部分
- Base64url 解码（`-`→`+`，`_`→`/`，补齐 `=`），再 Base64 解码为 JSON
- 结果以 `QTabWidget` 三标签页展示：Header | Payload | Signature
- Payload 自动识别时间戳字段 (`iat`/`exp`/`nbf`)，转换为可读日期时间追加显示
- 复制当前 / 复制全部 按钮（1.5s "已复制" 反馈）
- 清除按钮清空输入和三栏结果

## 随机字符串细节

- 字符集选择：A-Z 大写字母 / a-z 小写字母 / 0-9 数字 / 特殊符号（`!@#$%^&*()-_=+[]{};:'\",.<>?/\\|\`~`）
- 排除字符输入框：从字符集中排除指定字符后再生成
- 长度选择：1–256（默认 16）
- 数量选择：1–1000（默认 10，实际使用 `QSpinBox` `setRange(1, 1000)`）
- 使用 `QRandomGenerator::global()` 生成随机结果
- 结果以 `QTextEdit` (Consolas 14px) 显示，每行一个
- 复制全部按钮（1.5s "已复制" 反馈）
- 若未选择任何字符类型，显示提示"请至少选择一种字符类型"
