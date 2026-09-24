@echo off
setlocal
pushd "%~dp0rocnrope"
if errorlevel 1 exit /b 1

echo --------- Convert Roc'n Rope ---------
python rocnrope_rom_convert.py
set "convert_error=%errorlevel%"
popd
if not "%convert_error%"=="0" goto :error

echo --- Success ---
endlocal
exit /b 0

:error
echo --- Error #%convert_error%.
endlocal & exit /b %convert_error%
