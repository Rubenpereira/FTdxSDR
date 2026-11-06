@echo off
cd /d "%~dp0pronto"
if not exist FTdxSDR.exe (
  echo Nao achei pronto\FTdxSDR.exe - rode COMPILAR.bat primeiro.
  pause
  exit /b 1
)
start "" FTdxSDR.exe
