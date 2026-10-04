param(
    [string]$Runtime = "win-x64"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Output = Join-Path $ProjectRoot "artifacts\web-$Runtime"

Push-Location $ProjectRoot
try {
    dotnet restore .\src\WorldbuilderWriter.Web\WorldbuilderWriter.Web.csproj
    dotnet publish .\src\WorldbuilderWriter.Web\WorldbuilderWriter.Web.csproj `
        --configuration Release `
        --runtime $Runtime `
        --self-contained true `
        -p:PublishSingleFile=true `
        -p:IncludeNativeLibrariesForSelfExtract=true `
        -p:DebugType=None `
        -p:DebugSymbols=false `
        --output $Output
    Write-Host "Servidor web creado en $Output" -ForegroundColor Green
}
finally {
    Pop-Location
}
