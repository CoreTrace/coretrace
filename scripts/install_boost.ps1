# PowerShell script to download and install Boost on Windows
param (
    [string]$InstallDir = "$PSScriptRoot\..\build\boost_install",
    [string]$BoostVersion = "1.87.0"
)

# Convert version with underscores for download
$BoostUnderscoreVersion = $BoostVersion -replace '\.', '_'

Write-Host "Installing Boost $BoostVersion to $InstallDir"

# Create directories
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
$TempDir = "$env:TEMP\boost_build"
New-Item -ItemType Directory -Force -Path $TempDir | Out-Null

# Change to temp directory
Push-Location $TempDir

# Download and extract
$BoostArchive = "boost_$BoostUnderscoreVersion.zip"
$DownloadUrl = "https://boostorg.jfrog.io/artifactory/main/release/$BoostVersion/source/boost_$BoostUnderscoreVersion.zip"
$FallbackUrl = "https://sourceforge.net/projects/boost/files/boost/$BoostVersion/boost_$BoostUnderscoreVersion.zip/download"

# Download if file doesn't exist
if (-not (Test-Path $BoostArchive)) {
    Write-Host "Downloading Boost $BoostVersion..."
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $DownloadUrl -OutFile $BoostArchive
    }
    catch {
        Write-Host "Primary download failed, trying fallback..."
        try {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $FallbackUrl -OutFile $BoostArchive
        }
        catch {
            Write-Host "Failed to download Boost."
            exit 1
        }
    }
}

# Extract if directory doesn't exist
$ExtractDir = "$TempDir\boost_$BoostUnderscoreVersion"
if (-not (Test-Path $ExtractDir)) {
    Write-Host "Extracting Boost..."
    try {
        Expand-Archive -Path $BoostArchive -DestinationPath $TempDir
    }
    catch {
        Write-Host "Failed to extract Boost archive."
        exit 1
    }
}

# Build and install
Push-Location $ExtractDir
Write-Host "Setting up Boost build..."

# Run bootstrap
Write-Host "Running bootstrap..."
try {
    & .\bootstrap.bat
}
catch {
    Write-Host "Bootstrap failed."
    exit 1
}

# Build
Write-Host "Building and installing Boost... (this may take a while)"
try {
    # Build only the required libraries for faster compilation
    & .\b2.exe install --prefix="$InstallDir" --with-system --with-filesystem --with-program_options link=static runtime-link=shared
}
catch {
    Write-Host "Build failed."
    exit 1
}

# Verify installation
if (-not (Test-Path "$InstallDir\include\boost")) {
    Write-Host "Boost installation failed. Include directory not found."
    exit 1
}

Write-Host "Boost $BoostVersion installed successfully to $InstallDir"
Pop-Location
Pop-Location
exit 0