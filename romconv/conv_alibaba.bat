@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1

echo --------- Convert Alibaba ---------
echo Alibaba Unpack roms
python -c "import shutil; shutil.unpack_archive('../romszip/alibaba.zip', './roms/alibaba')"
set "convert_error=%errorlevel%"
if not "%convert_error%"=="0" goto :error

echo Converting Alibaba
cd alibaba
set "convert_error=%errorlevel%"
if not "%convert_error%"=="0" goto :error
python ./alibaba_rom_convert.py
set "convert_error=%errorlevel%"
if not "%convert_error%"=="0" goto :error

echo --- Success ---
popd
endlocal
exit /b 0

:error
echo --- Error #%convert_error%.
popd
endlocal & exit /b %convert_error%
