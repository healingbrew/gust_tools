@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
cd /d "%~dp0"

set CL=/nologo /errorReport:none /Ox /Gm- /GF /GS- /MP /MT /W4 /WX /wd4200 /wd4204 /wd4214 /wd4324 /wd4996 /D_UNICODE /D_CRT_SECURE_NO_DEPRECATE /UNDEBUG
set LINK=/errorReport:none /INCREMENTAL:NO

echo.
set APP_NAME=gust_pak
cl.exe %APP_NAME%.c util.c parson.c /Fe%APP_NAME%.exe
if %ERRORLEVEL% neq 0 goto out
echo =^> %APP_NAME%.exe

echo.
set APP_NAME=gust_elixir
cl.exe %APP_NAME%.c util.c parson.c miniz_tinfl.c miniz_tdef.c /Fe%APP_NAME%
if %ERRORLEVEL% neq 0 goto out
echo =^> %APP_NAME%

echo.
set APP_NAME=gust_g1t
cl.exe %APP_NAME%.c util.c parson.c /Fe%APP_NAME%
if %ERRORLEVEL% neq 0 goto out
echo =^> %APP_NAME%

echo.
set APP_NAME=gust_enc
cl.exe %APP_NAME%.c util.c parson.c /Fe%APP_NAME%.exe
if %ERRORLEVEL% neq 0 goto out
echo =^> %APP_NAME%.exe

echo.
set APP_NAME=gust_ebm
cl.exe %APP_NAME%.c util.c parson.c /Fe%APP_NAME%.exe
if %ERRORLEVEL% neq 0 goto out
echo =^> %APP_NAME%.exe

:out
if %ERRORLEVEL% neq 0 pause
