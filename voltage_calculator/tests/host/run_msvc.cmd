@echo off
setlocal
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /std:c11 /I..\.. test_voltage_math.c ..\..\voltage_math.c /Fe:test_voltage_math.exe
if errorlevel 1 exit /b 1
test_voltage_math.exe
exit /b %errorlevel%
