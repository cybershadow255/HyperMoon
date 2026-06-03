# ================================================================
#  HyperMon Setup Script
#  Run as Administrator: powershell -ExecutionPolicy Bypass -File setup.ps1
# ================================================================

param(
    [switch]$Uninstall,
    [switch]$Restart
)

$DriverName = "HyperMon"
$DriverSys  = Join-Path $PSScriptRoot "HyperMon.sys"
$CertName   = "HyperMonDev"

# ----------------------------------------------------------------
function Write-Step($n, $total, $msg) {
    Write-Host ("[" + $n + "/" + $total + "] ") -ForegroundColor DarkGray -NoNewline
    Write-Host $msg -ForegroundColor Cyan
}
function Write-OK($msg)   { Write-Host "    OK  " -ForegroundColor Green  -NoNewline; Write-Host $msg }
function Write-Warn($msg) { Write-Host "    WARN " -ForegroundColor Yellow -NoNewline; Write-Host $msg }
function Write-Fail($msg) { Write-Host "    FAIL " -ForegroundColor Red    -NoNewline; Write-Host $msg }

# ----------------------------------------------------------------
#  Require admin
# ----------------------------------------------------------------
if (-not ([Security.Principal.WindowsPrincipal]
          [Security.Principal.WindowsIdentity]::GetCurrent()
         ).IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)) {
    Write-Host "[-] Run this script as Administrator!" -ForegroundColor Red
    exit 1
}

# ----------------------------------------------------------------
#  UNINSTALL
# ----------------------------------------------------------------
if ($Uninstall) {
    Write-Host "`n[HyperMon] Uninstalling..." -ForegroundColor Yellow
    & sc.exe stop   $DriverName 2>$null | Out-Null
    & sc.exe delete $DriverName 2>$null | Out-Null
    Write-Host "[+] Service removed." -ForegroundColor Green
    Write-Host "    Test signing mode was NOT disabled (you may use it for other drivers)."
    Write-Host "    To disable: bcdedit /deletevalue testsigning   (then reboot)"
    exit 0
}

# ----------------------------------------------------------------
#  Banner
# ----------------------------------------------------------------
Write-Host ""
Write-Host "  +---------------------------------------------+" -ForegroundColor Cyan
Write-Host "  |   HyperMon Kernel Monitor - Setup          |" -ForegroundColor Cyan
Write-Host "  +---------------------------------------------+" -ForegroundColor Cyan
Write-Host ""

# ----------------------------------------------------------------
#  1. Check .sys exists
# ----------------------------------------------------------------
Write-Step 1 5 "Checking driver binary..."
if (-not (Test-Path $DriverSys)) {
    Write-Fail "HyperMon.sys not found at: $DriverSys"
    Write-Host ""
    Write-Host "  Build the driver first:" -ForegroundColor Yellow
    Write-Host "    1. Open HyperMon.sln in Visual Studio"
    Write-Host "    2. Build -> x64 Release"
    Write-Host "    3. Copy driver\x64\Release\HyperMon.sys to this folder"
    exit 1
}
Write-OK "Found: $DriverSys"

# ----------------------------------------------------------------
#  2. Enable Test Signing
# ----------------------------------------------------------------
Write-Step 2 5 "Enabling Test Signing Mode..."
$bcdedit = & bcdedit /enum "{current}" 2>$null | Out-String

if ($bcdedit -match "testsigning\s+Yes") {
    Write-OK "Already enabled."
} else {
    $result = & bcdedit /set testsigning on 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-OK "Enabled. A reboot is required (see end of script)."
        $needsReboot = $true
    } else {
        Write-Fail "bcdedit failed: $result"
        Write-Host "    If Secure Boot is on, disable it in UEFI/BIOS first." -ForegroundColor Yellow
        exit 1
    }
}

# ----------------------------------------------------------------
#  3. Create self-signed code-signing certificate
# ----------------------------------------------------------------
Write-Step 3 5 "Setting up code-signing certificate..."

# Check if cert already in LocalMachine\Root
$cert = Get-ChildItem Cert:\LocalMachine\Root |
        Where-Object { $_.Subject -like "*$CertName*" } |
        Select-Object -First 1

if (-not $cert) {
    $cert = New-SelfSignedCertificate `
        -Subject        "CN=$CertName" `
        -CertStoreLocation "Cert:\LocalMachine\Root" `
        -Type           CodeSigningCert `
        -KeyUsage       DigitalSignature `
        -NotAfter       (Get-Date).AddYears(10)
    Write-OK "Certificate created  (thumbprint: $($cert.Thumbprint.Substring(0,16))...)"
} else {
    Write-OK "Certificate already exists (thumbprint: $($cert.Thumbprint.Substring(0,16))...)"
}

# ----------------------------------------------------------------
#  4. Sign the driver
# ----------------------------------------------------------------
Write-Step 4 5 "Signing driver with test certificate..."

$signtool = Get-ChildItem `
    "C:\Program Files*\Windows Kits\10\bin\*\x64\signtool.exe" `
    -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName

if ($signtool) {
    $signResult = & $signtool sign `
        /sha1 $cert.Thumbprint `
        /fd   sha256 `
        /t    "http://timestamp.digicert.com" `
        $DriverSys 2>&1

    if ($LASTEXITCODE -eq 0) {
        Write-OK "Signed successfully."
    } else {
        Write-Warn "Timestamp server unreachable, signing without timestamp..."
        & $signtool sign /sha1 $cert.Thumbprint /fd sha256 $DriverSys 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) { Write-OK "Signed (no timestamp)." }
        else { Write-Warn "Signing failed. Driver may not load if test signing reboot is pending." }
    }
} else {
    Write-Warn "signtool.exe not found. Install WDK or Windows SDK."
    Write-Warn "Driver will try to load unsigned (works after test-signing reboot)."
}

# ----------------------------------------------------------------
#  5. Register and start service
# ----------------------------------------------------------------
Write-Step 5 5 "Installing kernel service..."

# Remove stale service if it exists
& sc.exe stop   $DriverName 2>$null | Out-Null
& sc.exe delete $DriverName 2>$null | Out-Null
Start-Sleep -Milliseconds 700

$create = & sc.exe create $DriverName `
    type=    kernel `
    binPath= $DriverSys `
    DisplayName= "HyperMon Kernel Monitor" `
    start=   demand 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Fail "sc.exe create failed: $create"
    exit 1
}
Write-OK "Service registered."

$start = & sc.exe start $DriverName 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-OK "Driver started!"
} elseif ($needsReboot) {
    Write-Warn "Driver not started yet - reboot required first (Test Signing was just enabled)."
} else {
    Write-Fail "Driver failed to start: $start"
    Write-Host ""
    Write-Host "  Troubleshooting:" -ForegroundColor Yellow
    Write-Host "    - Reboot if this is the first time enabling Test Signing"
    Write-Host "    - Check DebugView (Sysinternals) for kernel log: [HyperMon] ..."
    Write-Host "    - Make sure Secure Boot is disabled"
    exit 1
}

# ----------------------------------------------------------------
#  Done
# ----------------------------------------------------------------
Write-Host ""
Write-Host "  +---------------------------------------------+" -ForegroundColor Green
Write-Host "  |   Setup complete!                          |" -ForegroundColor Green
Write-Host "  +---------------------------------------------+" -ForegroundColor Green
Write-Host ""

if ($needsReboot) {
    Write-Host "  >> REBOOT REQUIRED <<" -ForegroundColor Yellow
    Write-Host "  After reboot, re-run setup.ps1 to start the driver." -ForegroundColor Yellow
    Write-Host ""
    if ($Restart) {
        Write-Host "  Rebooting in 10 seconds..." -ForegroundColor Red
        Start-Sleep 10
        Restart-Computer -Force
    } else {
        Write-Host "  Run with -Restart to reboot automatically: setup.ps1 -Restart" -ForegroundColor DarkGray
    }
} else {
    Write-Host "  Run monitor.exe to view live kernel events." -ForegroundColor Cyan
    Write-Host "  To uninstall:  setup.ps1 -Uninstall" -ForegroundColor DarkGray
}
Write-Host ""
