@echo off
setlocal EnableExtensions

rem Black Meridian UE5.1 Windows package entry point.
rem The default release-shaped invocation builds both client and server;
rem set BUILD_SERVER=0 only for a client-only local iteration.

if not defined UE_ROOT (
    echo UE_ROOT must point to a provisioned Unreal Engine 5.1 installation. 1>&2
    exit /b 2
)

if not defined PLATFORM set "PLATFORM=Win64"
if not defined CONFIGURATION set "CONFIGURATION=Development"
if not defined ARCHIVE_DIR set "ARCHIVE_DIR=%~dp0..\artifacts\package-win64"
if not defined BUILD_SERVER set "BUILD_SERVER=1"

if /I not "%PLATFORM%"=="Win64" (
    echo This release wrapper is intentionally Windows-only. PLATFORM=%PLATFORM% 1>&2
    exit /b 2
)

set "PROJECT_ROOT=%~dp0.."
set "PROJECT_PATH=%PROJECT_ROOT%\NightglassSignal.uproject"
set "UAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"

if not exist "%UAT%" (
    echo RunUAT.bat not found: %UAT% 1>&2
    exit /b 2
)
if not exist "%PROJECT_PATH%" (
    echo Project file not found: %PROJECT_PATH% 1>&2
    exit /b 2
)

if not exist "%ARCHIVE_DIR%" mkdir "%ARCHIVE_DIR%"

set "UAT_ARGS=BuildCookRun -noP4 -build -cook -stage -pak -archive -platform=Win64 -clientconfig=%CONFIGURATION% -serverconfig=%CONFIGURATION%"
if /I "%BUILD_SERVER%"=="1" (
    set "UAT_ARGS=%UAT_ARGS% -server"
) else (
    echo Skipping dedicated server target because BUILD_SERVER=%BUILD_SERVER%
)

echo Building %PROJECT_PATH%
echo UE_ROOT=%UE_ROOT%
echo ARCHIVE_DIR=%ARCHIVE_DIR%
call "%UAT%" %UAT_ARGS% "-project=%PROJECT_PATH%" "-archiveDirectory=%ARCHIVE_DIR%"
if errorlevel 1 (
    echo BuildCookRun failed. 1>&2
    exit /b %ERRORLEVEL%
)

echo Package created at %ARCHIVE_DIR%
echo Run Scripts\verify_windows_package.ps1 -PackageRoot "%ARCHIVE_DIR%" -RequireServer for the packaged client/server smoke.
exit /b 0
