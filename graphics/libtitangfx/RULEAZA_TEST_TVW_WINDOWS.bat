@echo off
REM ============================================================
REM  TVW - test nativ Windows (dublu-click)
REM  Pune .bat-ul in radacina pachetului SoftCAD_libtitangfx_TVW_LGPL
REM  (langa libtitangfx.dll si folderul path2\). Dublu-click.
REM  Testeaza: motorul libtitangfx.dll + companionul path2 ts_gfx_path2.dll
REM ============================================================
setlocal
chcp 65001 >nul
cd /d "%~dp0"

echo ============================================================
echo   TVW - test biblioteci native pe Windows
echo ============================================================
echo.

set "PY="
py -3 --version >nul 2>&1 && set "PY=py -3"
if not defined PY ( python --version >nul 2>&1 && set "PY=python" )
if not defined PY (
  echo [EROARE] Python negasit in PATH. Instaleaza Python 3.8+ bifand "Add to PATH".
  echo.
  pause & exit /b 1
)
%PY% --version

if not exist "libtitangfx.dll" ( echo [EROARE] Nu gasesc libtitangfx.dll langa .bat. & pause & exit /b 1 )
if not exist "path2\ts_gfx_path2.dll" ( echo [ATENTIE] Nu gasesc path2\ts_gfx_path2.dll ^(testul va raporta LIPSA^). )
if not exist "TEST_TVW_WINDOWS.py" ( echo [EROARE] Nu gasesc TEST_TVW_WINDOWS.py langa .bat. & pause & exit /b 1 )
echo.

%PY% TEST_TVW_WINDOWS.py
set "RC=%ERRORLEVEL%"
echo.
echo ( Daca scrie TOT VERDE, fa un screenshot si trimite-i-l lui Clodel. )
echo.
pause
endlocal & exit /b %RC%
