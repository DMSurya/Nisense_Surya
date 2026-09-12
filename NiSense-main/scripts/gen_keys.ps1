# =============================================================================
# MCUBoot RSA-2048 Key Generation Script (PowerShell)
# =============================================================================
# Generates RSA-2048 private and public keys for MCUBoot image signing
# Usage: powershell.exe -ExecutionPolicy Bypass -File scripts\gen_keys.ps1
# =============================================================================

$ErrorActionPreference = "Stop"

$KEY_DIR = "mcuboot"
$PRIVATE_KEY = Join-Path $KEY_DIR "root-rsa-2048.key"
$PUBLIC_KEY = Join-Path $KEY_DIR "root-rsa-2048.pub"
$PEM_KEY = Join-Path $KEY_DIR "root-rsa-2048.pem"

# Create mcuboot directory if it doesn't exist
if (-not (Test-Path $KEY_DIR)) {
    Write-Host "Creating directory: $KEY_DIR"
    New-Item -ItemType Directory -Path $KEY_DIR -Force | Out-Null
}

# Check if keys already exist
if ((Test-Path $PRIVATE_KEY) -or (Test-Path $PUBLIC_KEY) -or (Test-Path $PEM_KEY)) {
    Write-Host "⚠️  Keys already exist. Skipping generation." -ForegroundColor Yellow
    Write-Host "   Private key: $PRIVATE_KEY"
    Write-Host "   Public key:  $PUBLIC_KEY"
    Write-Host "   PEM key:     $PEM_KEY"
    exit 0
}

# Activate nRF Connect SDK virtual environment if available
$venvPath = Join-Path $env:USERPROFILE "zephyrproject\.venv\Scripts\Activate.ps1"
if (Test-Path $venvPath) {
    Write-Host "Activating nRF Connect SDK virtual environment..." -ForegroundColor Cyan
    & $venvPath
} else {
    Write-Host "⚠️  Virtual environment not found at: $venvPath" -ForegroundColor Yellow
    Write-Host "   Attempting to use system-wide imgtool..."
}

# Check if imgtool is available
$imgtoolCheck = Get-Command imgtool -ErrorAction SilentlyContinue
if (-not $imgtoolCheck) {
    Write-Host "❌ Error: imgtool not found." -ForegroundColor Red
    Write-Host "   Activate virtual environment or install MCUBoot tools: pip install imgtool"
    Write-Host ""
    Write-Host "To install in nRF Connect SDK:"
    Write-Host "  1. Activate venv: $venvPath"
    Write-Host "  2. Run: pip install imgtool"
    Write-Host "  3. Verify: imgtool --version"
    exit 1
}

Write-Host "🔑 Generating RSA-2048 keys for MCUBoot..." -ForegroundColor Cyan

try {
    # Generate RSA-2048 key pair using imgtool
    # Note: Use -k for key output, not --out (imgtool v1.x syntax)
    Write-Host "Generating key pair..."
    & imgtool keygen -t rsa-2048 -k $PEM_KEY
    
    if (-not (Test-Path $PEM_KEY)) {
        throw "Key generation failed - PEM file not created"
    }
    
    # Extract public key
    Write-Host "Extracting public key..."
    & imgtool getpub -k $PEM_KEY | Out-File -FilePath $PUBLIC_KEY -Encoding ASCII
    
    # Create private key copy for signing (same as PEM, just different extension)
    Write-Host "Creating private key copy..."
    Copy-Item $PEM_KEY -Destination $PRIVATE_KEY
    
    # Set read-only on private key (Windows equivalent of chmod 600)
    $acl = Get-Acl $PRIVATE_KEY
    $acl.SetAccessRuleProtection($true, $false)  # Disable inheritance
    $rule = New-Object System.Security.AccessControl.FileSystemAccessRule(
        [System.Security.Principal.WindowsIdentity]::GetCurrent().Name,
        "FullControl",
        "Allow"
    )
    $acl.SetAccessRule($rule)
    Set-Acl -Path $PRIVATE_KEY -AclObject $acl
    
    Write-Host ""
    Write-Host "✅ Keys generated successfully!" -ForegroundColor Green
    Write-Host "   Private key: $PRIVATE_KEY"
    Write-Host "   Public key:  $PUBLIC_KEY"
    Write-Host "   PEM key:     $PEM_KEY"
    Write-Host ""
    Write-Host "⚠️  IMPORTANT: Keep the private key secure!" -ForegroundColor Yellow
    Write-Host "   Do not commit root-rsa-2048.key or root-rsa-2048.pem to version control."
    Write-Host ""
    Write-Host "Next steps:"
    Write-Host "  1. Add to .gitignore: mcuboot/*.key and mcuboot/*.pem"
    Write-Host "  2. Rebuild project: Ctrl+Shift+B or task 'Build NiSense'"
    Write-Host "  3. Flash merged image: task 'Flash Program Only'"
}
catch {
    Write-Host "❌ Error generating keys: $_" -ForegroundColor Red
    exit 1
}

