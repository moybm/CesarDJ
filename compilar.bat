@echo off
rem Compila CesarDJ en modo Release usando el CMake que trae Visual Studio 2022
set "PATH=C:\Program Files\Git\cmd;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
cd /d "%~dp0"

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build build --config Release --parallel || goto :error

echo.
echo Listo: build\CesarDJ_artefacts\Release\CesarDJ.exe
pause
exit /b 0

:error
echo.
echo ERROR al compilar
pause
exit /b 1
