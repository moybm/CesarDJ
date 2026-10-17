@echo off
rem Genera en la carpeta dist\ el instalador (CesarDJ-Setup-X.Y.Z.exe) y la version portable (zip).
rem La version sale de CMakeLists.txt: para una actualizacion, sube "project(CesarDJ VERSION ...)".
setlocal
set "PATH=C:\Program Files\Git\cmd;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
cd /d "%~dp0"

rem 1) Compilar en Release
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build build --config Release --parallel || goto :error

rem 2) Leer la version
for /f "usebackq delims=" %%v in (`powershell -NoProfile -Command "(Select-String -Path CMakeLists.txt -Pattern 'project\(CesarDJ VERSION ([0-9.]+)\)').Matches[0].Groups[1].Value"`) do set "VERSION=%%v"
if "%VERSION%"=="" goto :error
echo Version: %VERSION%

rem 3) Instalador con Inno Setup
set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo No se encuentra Inno Setup. Instalalo con:  winget install --id JRSoftware.InnoSetup -e --scope user
    goto :error
)
if not exist dist mkdir dist
"%ISCC%" /Q /DAppVersion=%VERSION% installer\CesarDJ.iss || goto :error

rem 4) Version portable (un zip con el .exe y el LEEME)
powershell -NoProfile -Command "Compress-Archive -Force -Path 'build\CesarDJ_artefacts\Release\CesarDJ.exe','LEEME.txt' -DestinationPath 'dist\CesarDJ-%VERSION%-portable.zip'" || goto :error

echo.
echo Listo:
echo   dist\CesarDJ-Setup-%VERSION%.exe
echo   dist\CesarDJ-%VERSION%-portable.zip
pause
exit /b 0

:error
echo.
echo ERROR al generar el instalador
pause
exit /b 1
