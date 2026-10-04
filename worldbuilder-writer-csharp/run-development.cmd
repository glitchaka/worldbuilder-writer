@echo off
setlocal
cd /d "%~dp0"
dotnet restore src\WorldbuilderWriter.Web\WorldbuilderWriter.Web.csproj
if errorlevel 1 exit /b %errorlevel%
dotnet run --project src\WorldbuilderWriter.Web --no-restore
