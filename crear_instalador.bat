@echo off
rem Genera en la carpeta dist\ el instalador (CesarDJ-Setup-X.Y.Z.exe) y la version portable (zip)
rem y los publica en GitHub como Release vX.Y.Z (ver el paso 5).
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

rem 4) Version portable (zip con el .exe, el modelo de IA de stems y el LEEME)
powershell -NoProfile -Command "Compress-Archive -Force -Path 'build\CesarDJ_artefacts\Release\CesarDJ.exe','build\CesarDJ_artefacts\Release\models','LEEME.txt' -DestinationPath 'dist\CesarDJ-%VERSION%-portable.zip'" || goto :error

echo.
echo Listo:
echo   dist\CesarDJ-Setup-%VERSION%.exe
echo   dist\CesarDJ-%VERSION%-portable.zip

rem 5) Publicar en GitHub (repositorio privado moybm/CesarDJ): sube el codigo y crea la Release
rem    vX.Y.Z con el instalador y el portable. Si esa version ya estaba publicada, reemplaza los archivos.
rem    Necesita la herramienta gh con la sesion iniciada (gh auth login) y no tener cambios sin commit.
echo.
set "GH=C:\Program Files\GitHub CLI\gh.exe"
if not exist "%GH%" set "GH=gh"
"%GH%" auth status >nul 2>&1
if errorlevel 1 (
    echo GitHub: no hay sesion iniciada ^(gh auth login^). No se publica.
    goto :done
)
git remote get-url origin >nul 2>&1
if errorlevel 1 (
    echo GitHub: este repositorio no tiene remoto. No se publica.
    goto :done
)
set "DIRTY="
for /f "delims=" %%s in ('git status --porcelain') do set "DIRTY=1"
if defined DIRTY (
    echo GitHub: hay cambios sin guardar en git. Haz commit y vuelve a ejecutar para publicar.
    goto :done
)

echo Publicando en GitHub...
git push origin HEAD:main || goto :pubfail
git log -1 --format=%%B > build\release_notes.txt
"%GH%" release view v%VERSION% >nul 2>&1
if errorlevel 1 (
    "%GH%" release create v%VERSION% "dist\CesarDJ-Setup-%VERSION%.exe" "dist\CesarDJ-%VERSION%-portable.zip" --target main --title "CesarDJ %VERSION%" --notes-file build\release_notes.txt || goto :pubfail
) else (
    echo La Release v%VERSION% ya existia: se reemplazan el instalador y el portable.
    echo Para una Release nueva, sube "project^(CesarDJ VERSION ...^)" en CMakeLists.txt.
    "%GH%" release upload v%VERSION% "dist\CesarDJ-Setup-%VERSION%.exe" "dist\CesarDJ-%VERSION%-portable.zip" --clobber || goto :pubfail
)
echo GitHub: publicado en https://github.com/moybm/CesarDJ/releases/tag/v%VERSION%

:done
pause
exit /b 0

:pubfail
echo.
echo ERROR al publicar en GitHub (el instalador si se ha generado en dist\)
pause
exit /b 1

:error
echo.
echo ERROR al generar el instalador
pause
exit /b 1
