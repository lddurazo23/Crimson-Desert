@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"
echo DesiertoSeguro: compilación automática local, Windows x64.
echo.
set "DS_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%DS_VSWHERE%" goto :sin_vs
set "DS_VSROOT="
for /f "usebackq delims=" %%I in (`"%DS_VSWHERE%" -latest -version "[17.0,18.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "DS_VSROOT=%%I"
if not defined DS_VSROOT goto :sin_vs
set "DS_CMAKE=%DS_VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%DS_CMAKE%" goto :sin_cmake
set "DS_CTEST=%DS_VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
"%DS_CMAKE%" -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fallo
"%DS_CMAKE%" --build build --config Release --parallel
if errorlevel 1 goto :fallo
"%DS_CTEST%" --test-dir build -C Release --output-on-failure
if errorlevel 1 goto :fallo
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\empacar.ps1 -Archivo build\Release\DesiertoSeguro.asi
if errorlevel 1 goto :fallo
echo.
echo LISTO. El archivo está en build\Release\DesiertoSeguro.asi.
echo Importa DesiertoSeguro-0.3-DMM.zip desde DMM con el juego cerrado.
echo Lee docs\INSTALAR.md antes de instalar.
pause
exit /b 0
:sin_vs
echo No se encontró Visual Studio 2022 con Desarrollo para el escritorio con C++.
echo Abre docs\COMPILAR.md y completa los pasos de compilación local.
pause
exit /b 1
:sin_cmake
echo Falta Herramientas de CMake de C++ para Windows en Visual Studio Installer.
echo Abre Modificar, marca ese componente y vuelve a ejecutar este archivo.
pause
exit /b 1
:fallo
echo.
echo La compilación o verificación falló. No instales archivos de una compilación anterior.
echo Conserva el primer mensaje de error que aparece arriba para pedir ayuda.
pause
exit /b 1
