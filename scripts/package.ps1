# Zane Tool 打包脚本
# 用法: .\scripts\package.ps1

$ErrorActionPreference = "Stop"

$BuildDir = "build"
$InstallerDir = "installer\ZaneTool"
$DistDir = "dist"
$QtDir = "C:\qt\6.8.3\mingw_64"
$CMake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
$Toolchain = "mingw1310_64"
$NSIS = "makensis.exe"

Write-Host "=== 1/5 编译 ===" -ForegroundColor Cyan
$env:DCMAKE_PREFIX_PATH = $QtDir
& $CMake -S . -B $BuildDir -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DCMAKE_C_COMPILER="C:/qt/Tools/$Toolchain/bin/gcc.exe" `
    -DCMAKE_CXX_COMPILER="C:/qt/Tools/$Toolchain/bin/c++.exe" `
    -DCMAKE_RC_COMPILER="C:/qt/Tools/$Toolchain/bin/windres.exe"

& $CMake --build $BuildDir --target ZaneTool

Write-Host "=== 2/5 windeployqt ===" -ForegroundColor Cyan
& "$QtDir\bin\windeployqt.exe" "$BuildDir\ZaneTool.exe" --no-translations --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --no-quick-import --skip-plugin-types generic

Write-Host "=== 3/5 清理不必要插件 ===" -ForegroundColor Cyan
Remove-Item -LiteralPath "$BuildDir\networkinformation" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$BuildDir\tls\qcertonlybackend.dll" -Force -ErrorAction SilentlyContinue

Write-Host "=== 4/5 同步到安装目录 ===" -ForegroundColor Cyan
Remove-Item -LiteralPath $InstallerDir -Recurse -Force -ErrorAction SilentlyContinue
robocopy $BuildDir $InstallerDir /MIR /XD CMakeFiles ZaneTool_autogen /XF *.cmake Makefile CMakeCache.txt /NDL /NJH /NJS
Remove-Item -LiteralPath "$InstallerDir\ZaneTool_autogen" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$InstallerDir\CMakeFiles" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Force "$InstallerDir\CMakeCache.txt","$InstallerDir\Makefile","$InstallerDir\cmake_install.cmake" -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$InstallerDir\certs" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$InstallerDir\logs" -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "=== 5/5 生成安装包 ===" -ForegroundColor Cyan
if (-not (Test-Path $DistDir)) { New-Item -ItemType Directory -Path $DistDir | Out-Null }
& $NSIS installer\installer.nsi

$exe = Get-ChildItem "$DistDir\ZaneTool-*-setup.exe" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($exe) {
    Write-Host "`n打包完成: $($exe.FullName) ($('{0:N2} MB' -f ($exe.Length / 1MB)))" -ForegroundColor Green
} else {
    Write-Warning "未找到生成的安装包"
}
