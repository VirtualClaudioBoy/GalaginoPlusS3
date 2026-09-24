@echo off
setlocal
pushd "%~dp0"

echo --------- Convert Qix ---------
echo Qix Logo
python ./logoconv.py ../logos/qix.png ../source/src/machines/qix/qix_logo.h
if errorlevel 1 goto :error

echo Converting Qix ROMs
pushd qix
python qix_rom_convert.py
set "convert_error=%errorlevel%"
popd
if not "%convert_error%"=="0" goto :error

echo --- Success ---
popd
endlocal
exit /b 0

:error
echo --- Error #%errorlevel%.
popd
endlocal
exit /b 1
