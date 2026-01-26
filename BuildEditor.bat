@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "ENGINE_DIR=%~dp0"
if "%ENGINE_DIR:~-1%"=="\" set "ENGINE_DIR=%ENGINE_DIR:~0,-1%"

:: --- MMMENGINE_DIR 환경변수 설정 추가 ---
echo Setting MMMENGINE_DIR to: "%ENGINE_DIR%"

:: setx는 영구적으로 시스템 환경변수를 등록합니다. 
set "MMMENGINE_DIR=%ENGINE_DIR%"
setx MMMENGINE_DIR "%ENGINE_DIR%" >nul

if errorlevel 1 (
    echo [WARN] Failed to set MMMENGINE_DIR environment variable.
    echo        Try running this batch file as Administrator.
) else (
    echo [SUCCESS] MMMENGINE_DIR has been set.
)

if "%MMMENGINE_DIR%" NEQ "%ENGINE_DIR%" (
    echo [INFO] Detected path change. 
    echo Old: "%MMMENGINE_DIR%"
    echo New: "%ENGINE_DIR%"
    set "MMMENGINE_DIR=%ENGINE_DIR%"
    setx MMMENGINE_DIR "%ENGINE_DIR%" >nul
)
:: ---------------------------------------

set "PLATFORM=x64"
set "SLN=%ENGINE_DIR%\MMMEngine.sln"

set "BUILD_TARGET=MMMEngineEditor"

echo ENGINE_DIR=[%ENGINE_DIR%]
echo SLN=[%SLN%]
echo.

if not exist "%SLN%" goto :no_sln

goto :after_sln_check

:no_sln
echo ERROR: Solution not found:
echo   %SLN%
pause
exit /b 1

:after_sln_check

echo ===============================
echo   MMMEngine Editor Build
echo ===============================
echo.
echo   1) Release (recommended)
echo   2) Debug
echo.
choice /c 12 /n /m "Select build configuration: "

if errorlevel 2 set "CONFIG=Debug"
if errorlevel 1 set "CONFIG=Release"

echo.
echo Building configuration: %CONFIG%
echo.

rem --- find MSBuild ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo ERROR: vswhere not found
  pause
  exit /b 1
)

set "MSBUILD="
for /f "usebackq delims=" %%i in (
  `"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`
) do (
  set "MSBUILD=%%i"
  goto :msbuild_found
)

:msbuild_found
if not exist "%MSBUILD%" (
  echo ERROR: MSBuild not found
  pause
  exit /b 1
)

echo MSBuild:
echo   %MSBUILD%
echo.

"%MSBUILD%" "%SLN%" ^
  /t:%BUILD_TARGET% ^
  /p:Platform=%PLATFORM% ^
  /p:Configuration=%CONFIG% ^
  /m:1 /v:minimal /nologo

if errorlevel 1 (
  echo.
  echo BUILD FAILED
  pause
  exit /b 1
)

rem ============================================================
rem NORMALIZED OUTPUT:
rem ============================================================

set "OUTDIR=%ENGINE_DIR%\%PLATFORM%\%CONFIG%"
set "THIRDPARTY=%ENGINE_DIR%\Common\Bin\%CONFIG%"

:: 1. OUTDIR 존재 여부 확인
:: if 문 내부에서 경로 확장을 피하기 위해 GOTO 문으로 분리합니다.
if exist "%OUTDIR%" goto :outdir_exists
echo ERROR: Build output folder not found: "%OUTDIR%"
pause
exit /b 1

:outdir_exists
echo.
echo Copying third-party DLLs:
echo   From: "%THIRDPARTY%"
echo   To:   "%OUTDIR%"
echo.

:: 2. THIRDPARTY 체크 및 복사
:: 여기도 괄호 충돌 방지를 위해 GOTO를 사용합니다.
if not exist "%THIRDPARTY%" goto :no_thirdparty

:: 복사 실행
robocopy "%THIRDPARTY%" "%OUTDIR%" *.dll /NFL /NDL /NJH /NJS /NP /R:3 /W:1

:: Robocopy 리턴값 체크 (8 이상이 에러)
if errorlevel 8 (
    echo.
    echo THIRD-PARTY COPY FAILED
    pause
    exit /b 1
)
goto :finish

:no_thirdparty
echo WARN: Third-party folder not found (skipped): "%THIRDPARTY%"

:finish
echo.
echo DONE (%CONFIG%)
pause
exit /b 0