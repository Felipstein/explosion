@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /EHsc /std:c++17 dunesim_bench.cpp /Fe:dunesim_bench.exe >build.log 2>&1
type build.log | findstr /i "error warning"
