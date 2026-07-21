# Zane Tool 编译脚本
# 用法: .\scripts\build.ps1

$ErrorActionPreference = "Stop"

$QtDir = "C:\qt\6.8.3\mingw_64"
$CMake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
$Toolchain = "mingw1310_64"

Write-Host "=== CMake 配置 ===" -ForegroundColor Cyan
& $CMake -S . -B build -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DCMAKE_C_COMPILER="C:/qt/Tools/$Toolchain/bin/gcc.exe" `
    -DCMAKE_CXX_COMPILER="C:/qt/Tools/$Toolchain/bin/c++.exe" `
    -DCMAKE_RC_COMPILER="C:/qt/Tools/$Toolchain/bin/windres.exe"

Write-Host "`n=== 编译 ===" -ForegroundColor Cyan
& $CMake --build build --target ZaneTool

Write-Host "`n编译完成: build\ZaneTool.exe" -ForegroundColor Green
