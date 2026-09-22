# publish.ps1 — commit and push this repo to GitHub, then print Pages steps.
# Run with:  .\publish.ps1 -Username "your-github-user"
#            (optionally:  -RepoName "gamer-latency-meter")

param(
    [Parameter(Mandatory = $true)][string]$Username,
    [string]$RepoName = "gamer-latency-meter"
)

$ErrorActionPreference = "Stop"

# 0) guard: must run from the repo root
$here = (Get-Location).Path
if (-not (Test-Path (Join-Path $here ".git"))) {
    Write-Host "No git repo here. Run this script from the repo root: E:\wch\gamer-latency-meter" -ForegroundColor Red
    exit 1
}

# 1) git identity
$name  = git config user.name
$email = git config user.email
if (-not $name -or -not $email) {
    Write-Host "git identity is NOT set. Do this once (or use your own name):" -ForegroundColor Yellow
    Write-Host '    git config user.name  "Your Name"'
    Write-Host '    git config user.email "you@example.com"'
    exit 1
}

# 2) remote
$remote = "https://github.com/$Username/$RepoName.git"
$existing = git remote get-url origin 2>$null
if ($existing) {
    Write-Host "origin already set: $existing" -ForegroundColor Cyan
} else {
    git remote add origin $remote
    Write-Host "origin added: $remote" -ForegroundColor Green
}

# 3) commit + push
git add -A
if ($LASTEXITCODE -ne 0) { exit 1 }
git -c user.name="$name" -c user.email="$email" commit -m "Init: Gamer Peripheral Latency Meter (web app + firmware)"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Commit failed (nothing staged? already committed?). If you just created the repo at `"$remote`", re-run." -ForegroundColor Yellow
}
git branch -M main
git push -u origin main
if ($LASTEXITCODE -ne 0) {
    Write-Host "`nPush failed. On a fresh machine set a credential helper, e.g.:" -ForegroundColor Yellow
    Write-Host '    git config --global credential.helper manager'
    Write-Host "Then create a PAT (github.com -> Settings -> Developer settings -> Personal access tokens," -ForegroundColor Yellow
    Write-Host "fine-grained, Contents = Read/Write) and use it as the password when prompted." -ForegroundColor Yellow
} else {
    Write-Host "`nPushed. Open on https://github.com/$Username/$RepoName" -ForegroundColor Green
}

# 4) GitHub Pages instructions
Write-Host ""
Write-Host "== Enable GitHub Pages ==" -ForegroundColor Cyan
Write-Host "1. Open  https://github.com/$Username/$RepoName/settings/pages"
Write-Host "2. Source: 'Deploy from a branch' -> Branch 'main' -> Folder '/ (root)' -> Save"
Write-Host "3. Site goes live at  https://$Username.github.io/$RepoName/  (a few seconds/minutes)"
Write-Host "4. Open the app, click 'Connect to meter' (WebSerial), pick the Pico 2 CDC port."
Write-Host ""
Write-Host "Tip: update the live-app link in README.md to your real URL and re-push."