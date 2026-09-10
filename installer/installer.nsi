; Zane Tool NSIS Installer Script

!define PRODUCT_NAME "Zane Tool"
!define PRODUCT_VERSION "1.0.6"
!define PRODUCT_PUBLISHER "Zane"
!define PRODUCT_WEB_SITE ""

!define PRODUCT_DIR_REGKEY "Software\${PRODUCT_NAME}"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

Unicode true
SetCompressor /SOLID lzma

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "..\dist\ZaneTool-${PRODUCT_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\ZaneTool"
RequestExecutionLevel admin

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "StrFunc.nsh"
${Using:StrFunc} StrStr

!define MUI_ABORTWARNING
!define MUI_ICON "..\src\resources\app-icon.ico"
!define MUI_UNICON "..\src\resources\app-icon.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\ZaneTool.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

Section "Install"
    SetOutPath "$INSTDIR"

    ; 检测旧版本是否正在运行，若在运行则询问用户后关闭，避免文件被占用
    nsExec::ExecToStack 'tasklist /FI "IMAGENAME eq ZaneTool.exe" /FO CSV /NH'
    Pop $0
    Pop $1
    ${StrStr} $2 $1 "ZaneTool.exe"
    ${If} $2 != ""
        MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "检测到 ZaneTool 正在运行。$\r$\n$\r$\n升级安装前需要先关闭它，是否继续？" IDYES +2
        Abort
        nsExec::ExecToLog 'taskkill /F /T /IM ZaneTool.exe'
        nsExec::ExecToLog 'taskkill /F /T /IM ffmpeg.exe'
        nsExec::ExecToLog 'taskkill /F /T /IM aria2c.exe'
        nsExec::ExecToLog 'taskkill /F /T /IM mkcert.exe'
        Sleep 500
    ${EndIf}

    File /r "ZaneTool\*"

    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\ZaneTool.exe"
    CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\ZaneTool.exe"

    WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\ZaneTool.exe"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoRepair" 1

    WriteUninstaller "$INSTDIR\uninst.exe"
SectionEnd

Section "Uninstall"
    ; 先关闭正在运行的程序，避免文件被占用导致残留
    nsExec::ExecToLog 'taskkill /F /T /IM ZaneTool.exe'
    nsExec::ExecToLog 'taskkill /F /T /IM ffmpeg.exe'
    nsExec::ExecToLog 'taskkill /F /T /IM aria2c.exe'
    nsExec::ExecToLog 'taskkill /F /T /IM mkcert.exe'
    Sleep 500

    Delete "$INSTDIR\uninst.exe"
    RMDir /r "$INSTDIR"

    Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
    RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

    DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
    DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
SectionEnd
