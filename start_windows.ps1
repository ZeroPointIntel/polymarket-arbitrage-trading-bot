Write-Host "Starting Polymarket Arbitrage Trading Core with Dashboard Bridge..." -ForegroundColor Cyan

# Check if binary exists
if (-not (Test-Path "build/trading-core.exe")) {
    Write-Host "Error: C++ binary not found. Please build the project first." -ForegroundColor Red
    exit 1
}

# Start the bridge in a new window or background
# We'll use Start-Process to run it in the background
$bridgeProcess = Start-Process python -ArgumentList "dashboard_bridge.py" -NoNewWindow -PassThru -RedirectStandardOutput "bridge.log" -RedirectStandardError "bridge_error.log"

Write-Host "Bridge started. Waiting for initialization..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

Write-Host "Launching CLI Dashboard..." -ForegroundColor Green
python cli_dashboard.py

# Cleanup
Write-Host "Shutting down..." -ForegroundColor Yellow
Stop-Process -Id $bridgeProcess.Id -Force -ErrorAction SilentlyContinue
Get-Process "trading-core" -ErrorAction SilentlyContinue | Stop-Process -Force
