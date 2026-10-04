@echo off
setlocal
cd /d "%~dp0"
dotnet restore src\WorldbuilderWriter.Desktop\WorldbuilderWriter.Desktop.csproj
if errorlevel 1 exit /b %errorlevel%
dotnet run --project src\WorldbuilderWriter.Desktop --no-restore
