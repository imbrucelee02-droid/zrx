@echo off
echo Building simple_table_prase.zrx ...
set MSBUILD="D:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not exist %MSBUILD% (
    set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
)
%MSBUILD% ZrxDlgApp1.sln /p:Configuration=Debug /p:Platform=x64
