@echo off
rem Abre o WorldForge (gerador de mundos). Primeira vez num PC novo: forge.ps1 setup.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\forge.ps1" view %*
