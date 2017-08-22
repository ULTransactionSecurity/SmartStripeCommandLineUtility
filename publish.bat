@echo off
SET zipcommand="c:\Program Files\7-Zip\7z.exe" a -tzip 
SET visualstudiopath="C:\Program Files (x86)\Microsoft Visual Studio 14.0"

call SSPCommandLine\version.bat
SET name=SSPCommandLineTool-V%UTILITY_VERSION%
echo Utility version: %name%


echo Removing data from previous run:
rmdir /s /q %name%-bin
mkdir %name%-bin

rmdir /s /q %name%-source 
mkdir %name%-source
del %name%-source.zip
del %name%-bin.zip


echo Clean and build executable in release mode
if [%VisualStudioVersion%] NEQ [] goto skipVsDevCmd
rem setup build environment
call %visualstudiopath%\Common7\Tools\VsDevCmd.bat
:skipVsDevCmd

rem clean
msbuild SSPCommandLineTool.sln /t:clean /p:Configuration=Release;Platform=x86 /v:quiet
if errorlevel 1 goto buildError
rem build
msbuild SSPCommandLineTool.sln /t:build /p:Configuration=Release;Platform=x86 /v:quiet
if errorlevel 1 goto buildError


echo Copying executable and documentation for the executable
copy Release\hidapi.dll %name%-bin
copy Release\SSPCommandLineTool.exe %name%-bin
copy LICENSE*.md %name%-bin
copy GETTING-STARTED.md %name%-bin
copy *.md %name%-source

echo Copying source
mkdir %name%-source\hidapi
xcopy /S/E/Q hidapi %name%-source\hidapi

mkdir %name%-source\SSPCommandLine
xcopy /S/E/Q SSPCommandLine %name%-source\SSPCommandLine

copy SSPCommandLineTool.sln %name%-source

echo Removing unneccesary files from source:
rmdir /s /q %name%-source\SSPCommandLine\Debug
rmdir /s /q %name%-source\SSPCommandLine\Release
rmdir /s /q %name%-source\SSPCommandLine\x64
rmdir /s /q %name%-source\hidapi\windows\Release
rmdir /s /q %name%-source\hidapi\windows\Debug
del /s /q %name%-source\hidapi\windows\hidapi.sdf 
del %name%-source\SSPCommandLine\.kate*

echo Zipping everything:
%zipcommand% %name%-bin.zip  %name%-bin > nul
%zipcommand% %name%-source.zip  %name%-source > nul

goto end

:buildError

echo Building unsuccesfull

:end