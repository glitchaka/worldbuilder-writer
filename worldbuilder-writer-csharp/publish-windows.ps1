param(
    [string]$Runtime = "win-x64"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Output = Join-Path $ProjectRoot "artifacts\$Runtime"

Push-Location $ProjectRoot
try {
    dotnet restore .\src\WorldbuilderWriter.Desktop\WorldbuilderWriter.Desktop.csproj
    dotnet publish .\src\WorldbuilderWriter.Desktop\WorldbuilderWriter.Desktop.csproj `
        --configuration Release `
        --runtime $Runtime `
        --self-contained true `
        -p:PublishSingleFile=true `
        -p:IncludeNativeLibrariesForSelfExtract=true `
        -p:DebugType=None `
        -p:DebugSymbols=false `
        --output $Output
    Write-Host "Aplicación de escritorio creada en $Output\WorldbuilderWriter.exe" -ForegroundColor Green
}
finally {
    Pop-Location
}
