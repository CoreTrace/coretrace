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
$FallbackUrl = "https://sourceforge.net/projects/boost/files/boost/$BoostVersion/boost_$BoostUnderscoreVersion.zip"

# Download if file doesn't exist
if (-not (Test-Path $BoostArchive)) {
    Write-Host "Downloading Boost $BoostVersion..."
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile($DownloadUrl, "$TempDir\$BoostArchive")
    }
    catch {
        Write-Host "Primary download failed, trying fallback..."
        try {
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($FallbackUrl, "$TempDir\$BoostArchive")
        }
        catch {
            Write-Host "Failed to download Boost: $_"
            exit 1
        }
    }
}

# Extract if directory doesn't exist
$ExtractDir = "$TempDir\boost_$BoostUnderscoreVersion"
if (-not (Test-Path $ExtractDir)) {
    Write-Host "Extracting Boost..."
    try {
        # Try using .NET built-in method
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::ExtractToDirectory("$TempDir\$BoostArchive", $TempDir)
    }
    catch {
        Write-Host ".NET extraction failed, trying PowerShell's Expand-Archive..."
        try {
            # Make sure we have the right PowerShell version for Expand-Archive
            if ($PSVersionTable.PSVersion.Major -ge 5) {
                Expand-Archive -Path "$TempDir\$BoostArchive" -DestinationPath $TempDir -Force
            }
            else {
                # Fallback to 7-Zip if available
                $7zPath = "C:\Program Files\7-Zip\7z.exe"
                if (Test-Path $7zPath) {
                    Write-Host "Using 7-Zip for extraction..."
                    & $7zPath x "$TempDir\$BoostArchive" -o"$TempDir" -y
                }
                else {
                    throw "Neither PowerShell 5+ nor 7-Zip is available for extraction"
                }
            }
        }
        catch {
            Write-Host "Failed to extract Boost archive: $_"
            exit 1
        }
    }
}

# Verify extraction worked
if (-not (Test-Path $ExtractDir)) {
    # Check if extraction created a differently named directory
    $possibleDirs = Get-ChildItem -Path $TempDir -Directory
    foreach ($dir in $possibleDirs) {
        if ($dir.Name -like "boost_*") {
            Write-Host "Found Boost directory: $($dir.Name)"
            $ExtractDir = $dir.FullName
            break
        }
    }

    if (-not (Test-Path $ExtractDir)) {
        Write-Host "Extraction failed: Cannot find Boost directory"
        exit 1
    }
}

# Build and install
Push-Location $ExtractDir
Write-Host "Setting up Boost build..."

# Run bootstrap
Write-Host "Running bootstrap..."
try {
    # Check if bootstrap.bat exists
    if (Test-Path "bootstrap.bat") {
        & cmd /c bootstrap.bat
        if ($LASTEXITCODE -ne 0) {
            throw "Bootstrap failed with error code $LASTEXITCODE"
        }
    } else {
        throw "bootstrap.bat not found in $ExtractDir"
    }
}
catch {
    Write-Host "Bootstrap failed: $_"
    Pop-Location
    Pop-Location
    exit 1
}

# Build
Write-Host "Building and installing Boost... (this may take a while)"
try {
    # Check if b2.exe exists
    if (Test-Path "b2.exe") {
        # Build only the required libraries for faster compilation
        & cmd /c b2.exe install --prefix="$InstallDir" --with-system --with-filesystem --with-program_options link=static runtime-link=shared
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with error code $LASTEXITCODE"
        }
    } else {
        throw "b2.exe not found in $ExtractDir"
    }
}
catch {
    Write-Host "Build failed: $_"
    Pop-Location
    Pop-Location
    exit 1
}

# Verify installation
if (-not (Test-Path "$InstallDir\include\boost")) {
    Write-Host "Boost installation failed. Include directory not found."
    Pop-Location
    Pop-Location
    exit 1
}

Write-Host "Boost $BoostVersion installed successfully to $InstallDir"
Pop-Location
Pop-Location
exit 0