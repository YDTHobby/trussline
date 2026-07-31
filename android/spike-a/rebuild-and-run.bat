@echo off
call "C:\Users\nico1\Desktop\Rigs Port\spike-a\build-shell.bat"
if errorlevel 1 exit /b 1
powershell -ExecutionPolicy Bypass -File "C:\Users\nico1\Desktop\Rigs Port\spike-a\run-spike.ps1"
