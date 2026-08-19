# AGENTS.md

## 项目

Zane Tool — 基于 Qt6 Widgets 的桌面应用（C++17，仅 Win32，MinGW GCC 13.1），通过子进程调用 `ffmpeg.exe` 实现图片/视频/音频批量处理，同时包含屏幕取色、截图贴图、窗口透明、计时器、Base64 编码、时间戳转换、Cron 解析、JWT 解析、随机字符串生成、二维码工具、批量下载等工具。

## 构建与运行

```powershell
.\scripts\build.ps1
```

CMake 配置 + 编译，生成 `build\ZaneTool.exe`。构建后手动部署 Qt 运行时 DLL：

```powershell
C:\qt\6.8.3\mingw_64\bin\windeployqt.exe build\ZaneTool.exe --no-translations --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --no-quick-import --skip-plugin-types generic

Remove-Item -LiteralPath "build\networkinformation" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "build\tls\qcertonlybackend.dll" -Force -ErrorAction SilentlyContinue
```

## 打包安装包

```powershell
.\scripts\package.ps1
```

脚本自动执行：CMake 编译 → windeployqt 部署 → 清理冗余插件 → 同步文件到 `installer\ZaneTool\` → 调用 `makensis` 生成 NSIS 安装包到 `dist\ZaneTool-<version>-setup.exe`。

NSIS 脚本位于 `installer\installer.nsi`，安装目录为 `$PROGRAMFILES64\ZaneTool`，含开始菜单和桌面快捷方式。

无测试、无 linter、无 CI 配置。

## 架构

```
main.cpp          → QApplication + 全局 QSS（内嵌原始字符串）+ ffmpeg/aria2 存在性检查
MainWindow        → 侧边栏 + QStackedWidget（19 页）+ 批量队列编排 + 所有工具 UI/逻辑
ImageProcessor    → 静态：构建图片 ffmpeg 参数 + 输出路径
VideoProcessor    → 静态：构建视频 ffmpeg 参数 + 输出路径
AudioProcessor    → 静态：构建音频 ffmpeg 参数 + 输出路径
FFmpegProcess     → QProcess 封装（start/cancel/isRunning），含 finished/errorOccurred 信号
ColorPicker       → 全屏取色器 Widget，信号 colorPicked(QColor) / cancelled()
WindowPicker      → 窗口选择器 Widget（全局钩子），信号 windowPicked(HWND) / cancelled()
ScreenshotPicker  → 区域截图 Widget，信号 screenshotCaptured(QPixmap, QPoint) / cancelled()
PinWindow         → 置顶贴图窗口 Widget（独立 QWidget，无 parent）
StopwatchTimer    → 秒表引擎 QObject（QTimer 10ms 轮询 + QElapsedTimer）
CurlTool          → 解析浏览器复制的 curl 命令并通过 QNetworkAccessManager 发送
UpdateTool        → 启动时/关于对话框手动检查更新；3 个 version.txt 源按序降级，手动检查弹窗先提示"检测中"再显示直链下载地址（国内下载=gh-proxy，GitHub 下载=官方 release）
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
  秒表计时 (6)    — createTimerPage()
  计算器   (17)   — createCalcPage()
开发工具
  编码解码   (10)  — createCodecPage()
  JSON 格式化 (11) — createJsonPage()
  图片转Base64 (7) — createBase64Page()
  时间戳转换   (8) — createTimestampPage()
  Cron 解析   (9) — createCronPage()
  随机字符串  (13) — createRandomStringPage()
  二维码工具  (14) — createQrCodePage()
  HTTPS证书  (15) — createCertPage()
  网络请求   (18) — createCurlPage()
网络工具
  文件批量下载 (12) — createDownloadPage()
  本机IP查询  (16) — createIpPage()
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

## JSON 格式化细节

- 开发工具分组，索引 11，`JsonTool` 类（`jsontool.{h,cpp}`）
- 输入解析：`QJsonDocument::fromJson`，解析成功后在 `m_doc` 中保存文档，输出树/文本双视图切换
- 输出树 2 列（Key/Value），对象显示 `{ n }`、数组显示 `[ n ]`，标量按类型着色（字符串绿/数字蓝/布尔橙/null 灰）
- 每个树节点通过 `item->setData(0, Qt::UserRole, path)` 存储 JSONPath：对象键存键名字符串、数组下标存数字字符串，逐层 `valueByPath()` 沿 `m_doc` 解析（数组→索引，对象→键，天然无歧义）
- **导出 Excel**（`m_exportBtn`，绿色按钮）：仅当当前选中节点解析结果为数组时才可用（含根节点是数组的情形）；`currentItemChanged` 驱动启用/禁用
- 导出流程：数组元素全为对象时表头=全部键的首见顺序并集（缺失列补空单元格）；否则单列 `value`；数值→数字单元格、布尔→`t="b"`、字符串→inlineStr、嵌套对象/数组→紧凑 JSON 文本、null/缺失→空单元格
- 公共静态可复用 API：`cellFromJsonValue()`（单元格转换）、`arrayToRows()`（数组→表头/行）、`populateTree()`（向任意 QTreeWidget 填充 JSON 树）、`valueAtPath()`（按 JSONPath 解析节点值）——「网络请求」的 JSON 树与 Excel 导出均复用它
- `XlsxWriter::writeSheet()`（`xlsxwriter.{h,cpp}`）：内置最小 OOXML + STORE（不压缩）ZIP 写入器（CRC32 表 + 本地文件头 + 中央目录 + EOCD，全 LittleEndian，UTF-8 文件名标记位 0x0800），无第三方依赖；表头粗体样式（styles.xml 两个 xf）
- 导出自定义：`QFileDialog::getSaveFileName`，默认文件名为数组路径最后一段（无则 `data.xlsx`），自动补 `.xlsx` 后缀；成功更新状态栏（行列数+路径），失败弹错误框

## 网络请求细节

- 开发工具分组，索引 18，`CurlTool` 类（`curltool.{h,cpp}`），页面由 `createPage()` 产生（懒加载，仿 `JsonTool`）
- 输入区粘贴浏览器复制的 curl 命令（`QTextEdit`），按钮：发送 / 停止 / 解析 / 清空
- **解析器不调用 curl.exe**，而是将 curl 命令翻译为 Qt `QNetworkAccessManager` 请求：
  - Shell 风格分词：自动拼接行尾 `\` 续行，支持单引号（字面量）、双引号（`\"` 等转义）、`$'...'`（ANSI-C 转义，兼容 Firefox）
  - 选项支持：`--url`/位置参数 URL、`-X/--request`、`-H/--header`（同名字段后写覆盖）、`-b/--cookie`（合并为 `Cookie` 头，拼接 `"; "`）、`-d/--data/--data-raw/--data-ascii/--data-binary/--data-urlencode`（多个用 `&` 拼接，`@file` 读文件）、`--json`（强制 `Content-Type: application/json`）、`-G/--get`（body 并入 URL 查询串）、`-L/--location`（跟随 301/302）、`-k/--insecure`（`ignoreSslErrors()`）、`-u/--user`（Basic 认证）、`-A/--user-agent`、`-e/--referer`、`--compressed`（补 `Accept-Encoding: gzip, deflate`）、`-I/--head`
  - 无关选项（`-s -sS -f -o -O -v --max-time/--retry/--connect-timeout/--output` 等）静默忽略并汇总到警告标签
- 方法判定：显式 `-X`/`--request` 优先；有 body 且非 `-G`→POST；`-I`→HEAD；`-G` 有 body→GET；否则 GET。无显式 content-type 且有 body 时按 curl 行为补 `application/x-www-form-urlencoded`
- 执行：`sendCustomRequest(request, verb, body)`（GET 无 body 走 `get()`）；超时 30s；未加 `-L` 默认不跟随重定向（`RedirectPolicyAttribute` 默认 ManualRedirectPolicy）；停止=`reply->abort()`；`QElapsedTimer` 统计耗时
- URL 预览框 `QSizePolicy::Ignored`（水平最小宽度为 0，长 URL 在框内滚动），发送中状态文本只含方法不含 URL —— 避免点发送时长文本把窗口最小宽度撑大
- 响应：状态标签显示 `HTTP 状态码 · 耗时 · 大小`（≥400 标红），`QTabWidget` 分「JSON 树 / 响应体 / 响应头」；勾选「自动格式化 JSON」时将合法 JSON 响应用 `QJsonDocument::Indented` 展示；复制响应体按钮（1.5s "已复制" 反馈）
- **导出 Excel**（`m_exportExcelBtn`）：响应为合法 JSON 时用 `JsonTool::populateTree()`/`valueAtPath()` 填充「JSON 树」并默认选中根节点；「导出 Excel」仅当选中节点为非空数组时可用（`onJsonTreeSelectionChanged` 驱动）；导出复用 `JsonTool::arrayToRows()`，`QFileDialog` 默认文件名取响应 URL 路径末段（无则 `data.xlsx`），自动补 `.xlsx`，成功后状态标签显示 `已导出 n 行 × m 列 → 路径`
- 输入框高度自适应：`CurlTool::eventFilter` 监听页面 `Resize`，页面高度 ≥ 700 时 curl 命令输入框 `setMaximumHeight(200)`，否则 90

## HTTPS 证书细节

- `mkcert.exe` 与 exe 同级目录，`main.cpp` 启动时与 ffmpeg/aria2c 一起检查，缺失则致命退出
- 页面在开发工具分组，索引 14，`createCertPage()`，`MainWindow` 构造函数第三个参数 `mkcertPath`
- CA 状态检测：同步运行 `mkcert -CAROOT`（`waitForFinished(5000)`），检查 `<CAROOT>/rootCA.pem` 是否存在
- 安装/卸载根证书：`ShellExecuteExW` + `runas` 动词弹 UAC 提权运行 `mkcert -install` / `-uninstall`，`SEE_MASK_NOCLOSEPROCESS` + `WaitForSingleObject(60000)` 等待完成后刷新状态；`ERROR_CANCELLED`（用户取消 UAC）静默返回
- 生成证书：`QProcess` + `MergedChannels`，参数 `-cert-file <dir>/<name>.pem -key-file <dir>/<name>-key.pem <域名...>`
  - 域名输入按行/逗号/分号/空白拆分，用 `^[A-Za-z0-9*_.\-:]+$` 做轻量校验，其余交给 mkcert 报错
  - 文件名剔除 `\ / : * ? " < > |` 非法字符，默认 `dev`
  - 输出目录必填（无默认值），选择后自动 `mkpath`
  - 成功判定：exitCode=0 且两个 pem 文件存在且非空
- `m_certRunning` 标志 + `closeEvent` 中 kill 进程，与 ffmpeg/aria2 取消模式一致

