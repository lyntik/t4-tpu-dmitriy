@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Обязательные переменные (как в Qt Creator / CMakeLists.txt.user)
if not defined T4LIB_ROOT set "T4LIB_ROOT=C:\thirdparty\t4Lib"
if not defined OPENCV_ROOT set "OPENCV_ROOT=C:\thirdparty\opencv"
if not defined VTK_ROOT set "VTK_ROOT=C:\thirdparty\VTK"
if not defined VTK_VERSION set "VTK_VERSION=9.4"
if not defined LIBTIFF_ROOT set "LIBTIFF_ROOT=C:\thirdparty\libtiff"

REM Qt 6 нужен для VTK (GUISupportQt). Подставьте свой путь или установите через: pip install aqtinstall ^& aqt install-qt ...
if not defined CMAKE_PREFIX_PATH set "CMAKE_PREFIX_PATH=C:\Qt\6.8.2\msvc2022_64"

echo T4LIB_ROOT=%T4LIB_ROOT%
echo CMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH%

if not exist "%T4LIB_ROOT%\include\Common" (
  echo [ERROR] Неверный T4LIB_ROOT
  pause
  exit /b 1
)
if not exist "%CMAKE_PREFIX_PATH%\lib\cmake\Qt6\Qt6Config.cmake" (
  echo [ERROR] Не найден Qt6. Задайте CMAKE_PREFIX_PATH на каталог компилятора Qt6 ^(например msvc2022_64^).
  pause
  exit /b 1
)

set "BUILD=%~dp0build"
cmake -S "%~dp0" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 -D CMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"
if errorlevel 1 ( pause & exit /b 1 )

cmake --build "%BUILD%" --config Release
if errorlevel 1 ( pause & exit /b 1 )

echo.
echo Готово: %BUILD%\bin\Plugins\Release\AutoDenoiseTool.dll
pause
