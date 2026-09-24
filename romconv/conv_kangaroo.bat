@echo off
setlocal
pushd "%~dp0"

echo --------- Convert Kangaroo ---------
echo Kangaroo Logo
python ./logoconv.py ../logos/kangaroo.png ../source/src/machines/kangaroo/kangaroo_logo.h
if errorlevel 1 goto :error

echo Converting Kangaroo ROMs
pushd kangaroo
python kangaroo_rom_convert.py
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
