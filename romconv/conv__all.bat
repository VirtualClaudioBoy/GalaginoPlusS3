@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1
echo --------- Convert all ---------

cmd /d /c call "%~dp0conv_z80.bat"






cmd /d /c call "%~dp0conv_1942.bat"
cmd /d /c call "%~dp0conv_alibaba.bat"
cmd /d /c call "%~dp0conv_amidar.bat"
cmd /d /c call "%~dp0conv_anteater.bat"
cmd /d /c call "%~dp0conv_bagman.bat"
cmd /d /c call "%~dp0conv_bnj.bat"
cmd /d /c call "%~dp0conv_bombjack.bat"
cmd /d /c call "%~dp0conv_btime.bat"
cmd /d /c call "%~dp0conv_circusc.bat"
cmd /d /c call "%~dp0conv_crush.bat"
cmd /d /c call "%~dp0conv_digdug.bat"
cmd /d /c call "%~dp0conv_dkong.bat"
cmd /d /c call "%~dp0conv_dkong3.bat"
cmd /d /c call "%~dp0conv_dkongjr.bat"
cmd /d /c call "%~dp0conv_eyes.bat"
cmd /d /c call "%~dp0conv_fantasy.bat"
cmd /d /c call "%~dp0conv_frogger.bat"
cmd /d /c call "%~dp0conv_galaga.bat"
cmd /d /c call "%~dp0conv_galaxian.bat"
cmd /d /c call "%~dp0conv_gaplus.bat"
cmd /d /c call "%~dp0conv_gyruss.bat"
cmd /d /c call "%~dp0conv_invaders.bat"
cmd /d /c call "%~dp0conv_kangaroo.bat"
cmd /d /c call "%~dp0conv_ladybug.bat"
cmd /d /c call "%~dp0conv_lizwiz.bat"
cmd /d /c call "%~dp0conv_mappy.bat"
cmd /d /c call "%~dp0conv_mooncresta.bat"
cmd /d /c call "%~dp0conv_mrdo.bat"
cmd /d /c call "%~dp0conv_mrtnt.bat"
cmd /d /c call "%~dp0conv_mspacman.bat"
cmd /d /c call "%~dp0conv_nibbler.bat"
cmd /d /c call "%~dp0conv_pacman.bat"
cmd /d /c call "%~dp0conv_pbaction.bat"
cmd /d /c call "%~dp0conv_pengo.bat"
cmd /d /c call "%~dp0conv_phoenix.bat"
cmd /d /c call "%~dp0conv_pooyan.bat"
cmd /d /c call "%~dp0conv_qix.bat"
cmd /d /c call "%~dp0conv_rocnrope.bat"
cmd /d /c call "%~dp0conv_scramble.bat"
cmd /d /c call "%~dp0conv_scregg.bat"
cmd /d /c call "%~dp0conv_starforce.bat"
cmd /d /c call "%~dp0conv_supercobra.bat"
cmd /d /c call "%~dp0conv_theglob.bat"
cmd /d /c call "%~dp0conv_timeplt.bat"
cmd /d /c call "%~dp0conv_todruaga.bat"
cmd /d /c call "%~dp0conv_turtles.bat"
cmd /d /c call "%~dp0conv_tutankhm.bat"
cmd /d /c call "%~dp0conv_vanvan.bat"
cmd /d /c call "%~dp0conv_vanguard.bat"
cmd /d /c call "%~dp0conv_xevious.bat"





echo ---- Important
echo ---- Please check if any errors occured!
echo ---- Important
Pause
popd
endlocal
