@echo off
REM //---------- set up variable ----------
setlocal
set ROOT_DIR=%~dp0

REM // Check command line arguments
set "noFullPolyCar="
set "buildMode="

REM //check VS version
if "%VisualStudioVersion%" == "" (
    echo(
    echo oh oh... You need to run this command from x64 Native Tools Command Prompt for VS 2019.
    goto :buildfailed_nomsg
)
if "%VisualStudioVersion%" lss "16.0" (
    echo(
    echo Hello there! We just upgraded AirSim to Unreal Engine 4.24 and Visual Studio 2019.
    echo Here are few easy steps for upgrade so everything is new and shiny:
    echo https://github.com/Microsoft/AirSim/blob/master/docs/unreal_upgrade.md
    goto :buildfailed_nomsg
)

if "%1"=="" goto noargs
if "%1"=="--no-full-poly-car" set "noFullPolyCar=y"
if "%1"=="--Debug" set "buildMode=--Debug"
if "%1"=="--Release" set "buildMode=--Release"

if "%2"=="" goto noargs
if "%2"=="--Debug" set "buildMode=--Debug"
if "%2"=="--Release" set "buildMode=--Release"

:noargs

chdir /d %ROOT_DIR% 

REM //---------- Check cmake version ----------
CALL check_cmake.bat
if ERRORLEVEL 1 (
  CALL check_cmake.bat
  if ERRORLEVEL 1 (
    echo(
    echo ERROR: cmake was not installed correctly, we tried.
    goto :buildfailed
  )
)

REM //---------- get PX4-ECL ----------
IF NOT EXIST external\PX4-ECL mkdir external\PX4-ECL
IF NOT EXIST external\PX4-ECL\PX4-ECL-0.9.0 (
	REM //leave some blank lines because powershell shows download banner at top of console
	ECHO(
	ECHO(   
	ECHO(   
	ECHO *****************************************************************************************
	ECHO Downloading PX4-ECL
	ECHO *****************************************************************************************
	@echo on
	powershell -command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; iwr https://github.com/PX4/PX4-ECL/archive/v0.9.0.zip -OutFile external\PX4-ECL.zip }"
	@echo off
	
	REM //remove any previous versions
	rmdir external\PX4-ECL /q /s

	powershell -command "& { Expand-Archive -Path external\PX4-ECL.zip -DestinationPath external\PX4-ECL }"
	del external\PX4-ECL.zip /q
	
	REM //Don't fail the build if the high-poly car is unable to be downloaded
	REM //Instead, just notify users that the gokart will be used.
	IF NOT EXIST external\PX4-ECL\PX4-ECL-0.9.0 (
		ECHO Unable to download PX4-ECL
		goto :buildfailed
	)
)

REM //---------- copy PX4-ECL binaries and include folder inside AirLib folder ----------
set PX4_ECL_TARGET_LIB=AirLib\deps\ecl
if NOT exist %PX4_ECL_TARGET_LIB% mkdir %PX4_ECL_TARGET_LIB%
robocopy /MIR external\PX4-ECL\PX4-ECL-0.9.0 %PX4_ECL_TARGET_LIB%

REM //---------- get PX4-Matrix ----------
IF NOT EXIST external\matrix mkdir external\matrix
IF NOT EXIST external\matrix\PX4-Matrix-1.0.2 (
	REM //leave some blank lines because powershell shows download banner at top of console
	ECHO(
	ECHO(   
	ECHO(   
	ECHO *****************************************************************************************
	ECHO Downloading PX4-Matrix
	ECHO *****************************************************************************************
	@echo on
	powershell -command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; iwr https://github.com/PX4/PX4-Matrix/archive/v1.0.2.zip -OutFile external\matrix.zip }"
	@echo off
	
	REM //remove any previous versions
	rmdir external\matrix /q /s

	powershell -command "& { Expand-Archive -Path external\matrix.zip -DestinationPath external\matrix }"
	del external\matrix.zip /q
	
	REM //Don't fail the build if the high-poly car is unable to be downloaded
	REM //Instead, just notify users that the gokart will be used.
	IF NOT EXIST external\matrix\PX4-Matrix-1.0.2 (
		ECHO Unable to download matrix
		goto :buildfailed
	)
)

REM //---------- copy PX4-Matrix folder inside AirLib folder ----------
set PX4_ECL_TARGET_LIB=AirLib\deps\matrix
if NOT exist %PX4_ECL_TARGET_LIB% mkdir %PX4_ECL_TARGET_LIB%
robocopy /MIR external\matrix\PX4-Matrix-1.0.2 %PX4_ECL_TARGET_LIB%

REM //---------- get rpclib ----------
IF NOT EXIST external\rpclib mkdir external\rpclib
IF NOT EXIST external\rpclib\rpclib-2.2.1 (
	REM //leave some blank lines because powershell shows download banner at top of console
	ECHO(
	ECHO(   
	ECHO(   
	ECHO *****************************************************************************************
	ECHO Downloading rpclib
	ECHO *****************************************************************************************
	@echo on
	powershell -command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; iwr https://github.com/madratman/rpclib/archive/v2.2.1.zip -OutFile external\rpclib.zip }"
	@echo off
	
	REM //remove any previous versions
	rmdir external\rpclib /q /s

	powershell -command "& { Expand-Archive -Path external\rpclib.zip -DestinationPath external\rpclib }"
	del external\rpclib.zip /q
	
	REM //Don't fail the build if the high-poly car is unable to be downloaded
	REM //Instead, just notify users that the gokart will be used.
	IF NOT EXIST external\rpclib\rpclib-2.2.1 (
		ECHO Unable to download high-polycount SUV. Your AirSim build will use the default vehicle.
		goto :buildfailed
	)
)

REM //---------- Build rpclib ------------
ECHO Starting cmake to build rpclib...
IF NOT EXIST external\rpclib\rpclib-2.2.1\build mkdir external\rpclib\rpclib-2.2.1\build
cd external\rpclib\rpclib-2.2.1\build
cmake -G"Visual Studio 16 2019" ..

if "%buildMode%" == "--Debug" (
cmake --build . --config Debug -DPOSIX_SHARED
) else if "%buildMode%" == "--Release" (
cmake --build . --config Release -DPOSIX_SHARED
) else (
cmake --build .
cmake --build . --config Release
)

if ERRORLEVEL 1 goto :buildfailed
chdir /d %ROOT_DIR% 

REM //---------- copy rpclib binaries and include folder inside AirLib folder ----------
set RPCLIB_TARGET_LIB=AirLib\deps\rpclib\lib\x64
if NOT exist %RPCLIB_TARGET_LIB% mkdir %RPCLIB_TARGET_LIB%
set RPCLIB_TARGET_INCLUDE=AirLib\deps\rpclib\include
if NOT exist %RPCLIB_TARGET_INCLUDE% mkdir %RPCLIB_TARGET_INCLUDE%
robocopy /MIR external\rpclib\rpclib-2.2.1\include %RPCLIB_TARGET_INCLUDE%

if "%buildMode%" == "--Debug" (
robocopy /MIR external\rpclib\rpclib-2.2.1\build\Debug %RPCLIB_TARGET_LIB%\Debug
) else if "%buildMode%" == "--Release" (
robocopy /MIR external\rpclib\rpclib-2.2.1\build\Release %RPCLIB_TARGET_LIB%\Release
) else (
robocopy /MIR external\rpclib\rpclib-2.2.1\build\Debug %RPCLIB_TARGET_LIB%\Debug
robocopy /MIR external\rpclib\rpclib-2.2.1\build\Release %RPCLIB_TARGET_LIB%\Release
)

REM //---------- get High PolyCount SUV Car Model ------------
IF NOT EXIST Unreal\Plugins\AirSim\Content\VehicleAdv mkdir Unreal\Plugins\AirSim\Content\VehicleAdv
IF NOT EXIST Unreal\Plugins\AirSim\Content\VehicleAdv\SUV\v1.2.0 (
    IF NOT DEFINED noFullPolyCar (
        REM //leave some blank lines because powershell shows download banner at top of console
        ECHO(   
        ECHO(   
        ECHO(   
        ECHO *****************************************************************************************
        ECHO Downloading high-poly car assets.... The download is ~37MB and can take some time.
        ECHO To install without this assets, re-run build.cmd with the argument --no-full-poly-car
        ECHO *****************************************************************************************
       
        IF EXIST suv_download_tmp rmdir suv_download_tmp /q /s
        mkdir suv_download_tmp
        @echo on
        REM powershell -command "& { Start-BitsTransfer -Source https://github.com/Microsoft/AirSim/releases/download/v1.2.0/car_assets.zip -Destination suv_download_tmp\car_assets.zip }"
        REM powershell -command "& { (New-Object System.Net.WebClient).DownloadFile('https://github.com/Microsoft/AirSim/releases/download/v1.2.0/car_assets.zip', 'suv_download_tmp\car_assets.zip') }"
        powershell -command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; iwr https://github.com/Microsoft/AirSim/releases/download/v1.2.0/car_assets.zip -OutFile suv_download_tmp\car_assets.zip }"
        @echo off
		rmdir /S /Q Unreal\Plugins\AirSim\Content\VehicleAdv\SUV
        powershell -command "& { Expand-Archive -Path suv_download_tmp\car_assets.zip -DestinationPath Unreal\Plugins\AirSim\Content\VehicleAdv }"
        rmdir suv_download_tmp /q /s
        
        REM //Don't fail the build if the high-poly car is unable to be downloaded
        REM //Instead, just notify users that the gokart will be used.
        IF NOT EXIST Unreal\Plugins\AirSim\Content\VehicleAdv\SUV ECHO Unable to download high-polycount SUV. Your AirSim build will use the default vehicle.
    ) else (
        ECHO Not downloading high-poly car asset. The default unreal vehicle will be used.
    )
)

REM //---------- get Eigen library ----------
IF NOT EXIST AirLib\deps mkdir AirLib\deps
IF NOT EXIST AirLib\deps\eigen3 (
    powershell -command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; iwr https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.zip -OutFile eigen3.zip }"
    powershell -command "& { Expand-Archive -Path eigen3.zip -DestinationPath AirLib\deps }"
    powershell -command "& { Move-Item -Path AirLib\deps\eigen* -Destination AirLib\deps\del_eigen }"
    REM move AirLib\deps\eigen* AirLib\deps\del_eigen
    mkdir AirLib\deps\eigen3
    move AirLib\deps\del_eigen\Eigen AirLib\deps\eigen3\Eigen
    rmdir /S /Q AirLib\deps\del_eigen
    del eigen3.zip
)
IF NOT EXIST AirLib\deps\eigen3 goto :buildfailed


REM //---------- now we have all dependencies to compile AirSim.sln which will also compile MavLinkCom ----------
if "%buildMode%" == "--Debug" (
msbuild -maxcpucount:12 /p:Platform=x64 /p:Configuration=Debug AirSim.sln
if ERRORLEVEL 1 goto :buildfailed
) else if "%buildMode%" == "--Release" (
msbuild -maxcpucount:12 /p:Platform=x64 /p:Configuration=Release AirSim.sln
if ERRORLEVEL 1 goto :buildfailed
) else (
msbuild -maxcpucount:12 /p:Platform=x64 /p:Configuration=Debug AirSim.sln
if ERRORLEVEL 1 goto :buildfailed
msbuild -maxcpucount:12 /p:Platform=x64 /p:Configuration=Release AirSim.sln 
if ERRORLEVEL 1 goto :buildfailed
)

REM //---------- copy binaries and include for MavLinkCom in deps ----------
set MAVLINK_TARGET_LIB=AirLib\deps\MavLinkCom\lib
if NOT exist %MAVLINK_TARGET_LIB% mkdir %MAVLINK_TARGET_LIB%
set MAVLINK_TARGET_INCLUDE=AirLib\deps\MavLinkCom\include
if NOT exist %MAVLINK_TARGET_INCLUDE% mkdir %MAVLINK_TARGET_INCLUDE%
robocopy /MIR MavLinkCom\include %MAVLINK_TARGET_INCLUDE%
robocopy /MIR MavLinkCom\lib %MAVLINK_TARGET_LIB%

REM //---------- all our output goes to Unreal/Plugin folder ----------
if NOT exist Unreal\Plugins\AirSim\Source\AirLib mkdir Unreal\Plugins\AirSim\Source\AirLib
robocopy /MIR AirLib Unreal\Plugins\AirSim\Source\AirLib  /XD temp *. /njh /njs /ndl /np
copy /y AirSim.props Unreal\Plugins\AirSim\Source\AirLib

REM //---------- done building ----------
exit /b 0

:buildfailed
echo(
echo #### Build failed - see messages above. 1>&2

:buildfailed_nomsg
chdir /d %ROOT_DIR% 
exit /b 1



