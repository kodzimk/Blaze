param(
    [switch]$Build,
    [switch]$SkipCompile
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BinRoot = Join-Path $Root "bin\Release-windows-x86_64"
$Dist = Join-Path $Root "dist"
$Payload = Join-Path $Dist "payload"
$ZipOut = Join-Path $Dist "Blaze-Windows-x64.zip"
$Iss = Join-Path $Root "installer\Blaze.iss"

function Find-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($msbuild -and (Test-Path $msbuild)) { return $msbuild }
    }
    $fallback = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
    )
    foreach ($path in $fallback) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

function Find-ISCC {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Require-File($path, $label) {
    if (-not (Test-Path $path)) {
        throw "Missing ${label}: $path`nBuild Release|x64 (Blaze, Sandbox, Runtime) first, or pass -Build."
    }
}

if ($Build) {
    $msbuild = Find-MSBuild
    if (-not $msbuild) {
        throw "MSBuild not found. Install Visual Studio with C++ workload, or build Release manually."
    }
    Write-Host "Building Release|x64 with: $msbuild"
    $sln = Join-Path $Root "Blaze.sln"
    $runtimeProj = Join-Path $Root "Runtime\Runtime.vcxproj"
    if (-not (Test-Path $sln)) {
        throw "Solution not found: $sln"
    }
    & $msbuild $sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE"
    }
    # Runtime may be missing from an old .sln — build the vcxproj explicitly
    if (-not (Test-Path (Join-Path $BinRoot "Runtime\Runtime.exe"))) {
        if (-not (Test-Path $runtimeProj)) {
            throw "Runtime.vcxproj not found and Runtime.exe was not produced by the solution build."
        }
        Write-Host "Runtime.exe missing from solution output; building Runtime.vcxproj..."
        & $msbuild $runtimeProj /p:Configuration=Release /p:Platform=x64 /m /v:minimal
        if ($LASTEXITCODE -ne 0) {
            throw "Runtime MSBuild failed with exit code $LASTEXITCODE"
        }
    }
}

$sandboxExe = Join-Path $BinRoot "Sandbox\Sandbox.exe"
$blazeDll = Join-Path $BinRoot "Sandbox\Blaze.dll"
$runtimeExe = Join-Path $BinRoot "Runtime\Runtime.exe"
if (-not (Test-Path $blazeDll)) {
    $blazeDll = Join-Path $BinRoot "Blaze\Blaze.dll"
}

Require-File $sandboxExe "Sandbox.exe"
Require-File $blazeDll "Blaze.dll"
Require-File $runtimeExe "Runtime.exe"

Write-Host "Staging payload -> $Payload"
if (Test-Path $Payload) {
    Remove-Item $Payload -Recurse -Force
}
New-Item -ItemType Directory -Path $Payload -Force | Out-Null

Copy-Item $sandboxExe (Join-Path $Payload "Blaze.exe")
Copy-Item $blazeDll (Join-Path $Payload "Blaze.dll")
Copy-Item $runtimeExe (Join-Path $Payload "Runtime.exe")

$engineIcons = Join-Path $Root "Blaze\res\icons"
if (Test-Path $engineIcons) {
    $destIcons = Join-Path $Payload "Blaze\res\icons"
    New-Item -ItemType Directory -Path $destIcons -Force | Out-Null
    Copy-Item (Join-Path $engineIcons "*") $destIcons -Recurse -Force
}

$sandboxIcons = Join-Path $Root "Sandbox\content\icons"
if (Test-Path $sandboxIcons) {
    $destContentIcons = Join-Path $Payload "content\icons"
    New-Item -ItemType Directory -Path $destContentIcons -Force | Out-Null
    Copy-Item (Join-Path $sandboxIcons "*") $destContentIcons -Recurse -Force
}

$license = Join-Path $Root "LICENSE"
if (Test-Path $license) {
    Copy-Item $license (Join-Path $Payload "LICENSE")
}

$docs = Join-Path $Root "DOCUMENTATION.md"
if (Test-Path $docs) {
    Copy-Item $docs (Join-Path $Payload "DOCUMENTATION.md")
}

$readmeLines = @(
    "Blaze Engine - offline package",
    "==============================",
    "",
    "This folder is complete. You do NOT need Visual Studio, Inno Setup,",
    "or any extra redistributables.",
    "",
    "1. Double-click Blaze.exe to open the editor.",
    "2. Create or open a project from the Project Hub.",
    "3. File -> Build Project packages your game (uses Runtime.exe here).",
    "",
    "Everything required is already next to Blaze.exe."
)
$readmeLines | Set-Content -Path (Join-Path $Payload "README.txt") -Encoding UTF8

Write-Host "Payload ready."

New-Item -ItemType Directory -Path $Dist -Force | Out-Null
if (Test-Path $ZipOut) {
    Remove-Item $ZipOut -Force
}
Write-Host "Creating offline zip -> $ZipOut"
Compress-Archive -Path (Join-Path $Payload "*") -DestinationPath $ZipOut -CompressionLevel Optimal
$zipMb = [math]::Round((Get-Item $ZipOut).Length / 1MB, 2)
Write-Host "Done (zip): $ZipOut ($zipMb MB) - upload THIS file; users need nothing else."

if ($SkipCompile) {
    Write-Host "Skipping Inno Setup compile (-SkipCompile)."
    exit 0
}

$iscc = Find-ISCC
if (-not $iscc) {
    Write-Host "Inno Setup not installed - that is fine for shipping."
    Write-Host "Ship the zip above as your single download:"
    Write-Host "  $ZipOut"
    Write-Host "Optional: install Inno Setup 6, then re-run for dist\BlazeSetup.exe"
    Write-Host "  https://jrsoftware.org/isinfo.php"
    exit 0
}

Write-Host "Compiling installer with: $iscc"
& $iscc $Iss
if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE"
}

$setup = Join-Path $Dist "BlazeSetup.exe"
if (Test-Path $setup) {
    $sizeMb = [math]::Round((Get-Item $setup).Length / 1MB, 2)
    Write-Host "Done (setup): $setup ($sizeMb MB) - also a single download; embeds the same files."
} else {
    Write-Warning "ISCC finished but BlazeSetup.exe was not found under dist/"
}
