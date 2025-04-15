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

# Switch to a more reliable mirror for GitHub Actions
$BoostArchive = "boost_$BoostUnderscoreVersion.zip"

# Use multiple mirrors to increase reliability
$DownloadUrls = @(
    "https://boostorg.jfrog.io/artifactory/main/release/$BoostVersion/source/boost_$BoostUnderscoreVersion.zip",
    "https://github.com/boostorg/boost/releases/download/boost-$BoostVersion/boost_$BoostUnderscoreVersion.zip",
    "https://sourceforge.net/projects/boost/files/boost/$BoostVersion/boost_$BoostUnderscoreVersion.zip/download"
)

$ArchivePath = "$TempDir\$BoostArchive"

# Try downloading from each mirror until successful
$downloadSuccess = $false
foreach ($url in $DownloadUrls) {
    if (-not (Test-Path $ArchivePath)) {
        Write-Host "Downloading Boost $BoostVersion from $url"
        try {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($url, $ArchivePath)
            
            # Verify download
            if (Test-Path $ArchivePath) {
                $fileSize = (Get-Item $ArchivePath).Length
                if ($fileSize -gt 1MB) {  # Ensure it's not just an error page
                    $downloadSuccess = $true
                    Write-Host "Download successful. Archive size: $([Math]::Round($fileSize / 1MB, 2)) MB"
                    break
                } else {
                    Write-Host "Downloaded file is too small, likely corrupt"
                    Remove-Item $ArchivePath -Force
                }
            }
        }
        catch {
            Write-Host "Download from $url failed: $_"
            if (Test-Path $ArchivePath) {
                Remove-Item $ArchivePath -Force
            }
        }
    }
}

if (-not $downloadSuccess) {
    Write-Host "Failed to download Boost from any mirror. Using pre-installed libraries."
    exit 1
}

# Use NuGet to install pre-compiled Boost binaries as fallback
# This is a more reliable solution for CI environments
if ($env:CI -eq "true") {
    try {
        Write-Host "CI environment detected. Using NuGet to install Boost..."
        
        # Create a packages directory
        $nugetPkgDir = "$TempDir\packages"
        New-Item -ItemType Directory -Force -Path $nugetPkgDir | Out-Null
        
        # NuGet URL
        $nugetUrl = "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe"
        $nugetPath = "$TempDir\nuget.exe"
        
        # Download NuGet
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile($nugetUrl, $nugetPath)
        
        # Install Boost via NuGet
        Write-Host "Installing Boost via NuGet..."
        & $nugetPath install boost-vc142 -Version 1.78.0 -OutputDirectory $nugetPkgDir
        
        # Copy files from NuGet package to install directory
        $nugetBoostDir = Get-ChildItem -Path $nugetPkgDir -Filter "boost-vc142*" -Directory | Select-Object -First 1
        if ($nugetBoostDir) {
            # Create include and lib directories
            New-Item -ItemType Directory -Force -Path "$InstallDir\include" | Out-Null
            New-Item -ItemType Directory -Force -Path "$InstallDir\lib" | Out-Null
            
            # Copy include files
            Copy-Item -Path "$($nugetBoostDir.FullName)\lib\native\include\boost" -Destination "$InstallDir\include\boost" -Recurse -Force
            
            # Copy lib files
            Copy-Item -Path "$($nugetBoostDir.FullName)\lib\native\lib\x64\*" -Destination "$InstallDir\lib" -Recurse -Force
            
            Write-Host "Successfully installed Boost from NuGet package"
            exit 0
        }
    }
    catch {
        Write-Host "NuGet installation failed: $_"
        # Continue with normal extraction as fallback
    }
}

# Extract if directory doesn't exist
$ExtractDir = "$TempDir\boost_$BoostUnderscoreVersion"
if (-not (Test-Path $ExtractDir)) {
    Write-Host "Extracting Boost..."
    
    # First try PowerShell's native Expand-Archive
    try {
        # Make sure we have the right PowerShell version for Expand-Archive
        if ($PSVersionTable.PSVersion.Major -ge 5) {
            Expand-Archive -Path $ArchivePath -DestinationPath $TempDir -Force -ErrorAction Stop
            Write-Host "Extraction using Expand-Archive succeeded"
        }
        else {
            throw "PowerShell version is below 5.0"
        }
    }
    catch {
        Write-Host "PowerShell extraction failed: $_"
        
        # Fallback to .NET method
        try {
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            [System.IO.Compression.ZipFile]::ExtractToDirectory($ArchivePath, $TempDir)
            Write-Host "Extraction using .NET succeeded"
        }
        catch {
            Write-Host ".NET extraction failed: $_"
            
            # Last resort - use third-party tool if available
            try {
                # Check if 7-Zip is available
                $7zPath = "C:\Program Files\7-Zip\7z.exe"
                if (Test-Path $7zPath) {
                    Write-Host "Using 7-Zip for extraction..."
                    & $7zPath x $ArchivePath -o"$TempDir" -y
                    if ($LASTEXITCODE -eq 0) {
                        Write-Host "7-Zip extraction succeeded"
                    }
                    else {
                        throw "7-Zip extraction failed with code $LASTEXITCODE"
                    }
                }
                else {
                    throw "7-Zip not found"
                }
            }
            catch {
                Write-Host "All extraction methods failed. Installing CMake package as fallback."
                # Use vcpkg as a last resort (often available in GitHub Actions)
                try {
                    # Find VCPKG
                    $vcpkgPath = "C:\vcpkg\vcpkg.exe"
                    if (-not (Test-Path $vcpkgPath)) {
                        $vcpkgPath = "$env:VCPKG_INSTALLATION_ROOT\vcpkg.exe"
                    }
                    
                    if (Test-Path $vcpkgPath) {
                        Write-Host "Found vcpkg, installing Boost..."
                        & $vcpkgPath install boost:x64-windows
                        
                        if ($LASTEXITCODE -eq 0) {
                            Write-Host "vcpkg Boost installation succeeded"
                            # Copy files from vcpkg to our install directory
                            $vcpkgRoot = Split-Path -Parent $vcpkgPath
                            $vcpkgInclude = "$vcpkgRoot\installed\x64-windows\include\boost"
                            $vcpkgLib = "$vcpkgRoot\installed\x64-windows\lib"
                            
                            if (Test-Path $vcpkgInclude) {
                                New-Item -ItemType Directory -Force -Path "$InstallDir\include" | Out-Null
                                Copy-Item -Path $vcpkgInclude -Destination "$InstallDir\include\boost" -Recurse -Force
                                
                                New-Item -ItemType Directory -Force -Path "$InstallDir\lib" | Out-Null
                                Copy-Item -Path "$vcpkgLib\*boost*" -Destination "$InstallDir\lib" -Force
                                
                                Write-Host "Successfully copied Boost from vcpkg"
                                exit 0
                            }
                        }
                        else {
                            throw "vcpkg installation failed with code $LASTEXITCODE"
                        }
                    }
                    else {
                        throw "vcpkg not found"
                    }
                }
                catch {
                    Write-Host "vcpkg installation failed: $_"
                    Write-Host "All Boost installation methods failed"
                    exit 1
                }
            }
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
    exit 1
}

# Verify installation
if (-not (Test-Path "$InstallDir\include\boost")) {
    Write-Host "Boost installation failed. Include directory not found."
    Pop-Location
    exit 1
}

Write-Host "Boost $BoostVersion installed successfully to $InstallDir"
Pop-Location
exit 0