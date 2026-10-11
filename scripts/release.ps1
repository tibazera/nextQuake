<#
.SYNOPSIS
  Builds and packages a nextQuake Android release.

.DESCRIPTION
  VERSION (repo root) is the single source of the release version: it feeds the
  APK versionName/versionCode (app/build.gradle.kts), NEXTQUAKE_VERSION in the
  native code (f_version, update check) and this script.

  The script does NOT commit, tag or push. It prints the exact commands at the end.

.EXAMPLE
  .\scripts\release.ps1                  # build the version currently in VERSION
  .\scripts\release.ps1 -Version 0.1.1   # bump VERSION first, then build
  .\scripts\release.ps1 -AllowDirty      # build with uncommitted changes (test builds only)
#>
param(
    [string]$Version,
    [switch]$AllowDirty,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# ---- version -----------------------------------------------------------
if ($Version) {
    if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "Version must be MAJOR.MINOR.PATCH, got '$Version'" }
    Set-Content -Path VERSION -Value $Version -NoNewline -Encoding ascii
    Add-Content -Path VERSION -Value ""
}
$ver = (Get-Content VERSION -TotalCount 1).Trim()
if ($ver -notmatch '^\d+\.\d+\.\d+$') { throw "VERSION file is not MAJOR.MINOR.PATCH: '$ver'" }
$tag = "v$ver"
Write-Host "== nextQuake $ver ($tag)"

# ---- git state ---------------------------------------------------------
$branch = (git rev-parse --abbrev-ref HEAD).Trim()
$dirty = (git status --porcelain)
if ($dirty -and -not $AllowDirty) {
    throw "Working tree has uncommitted changes. Commit them first, or pass -AllowDirty for a test build."
}
if (git tag --list $tag) { Write-Warning "Tag $tag already exists." }
if (-not (Select-String -Path CHANGELOG.md -Pattern "^## \[?$([regex]::Escape($ver))\]?" -Quiet)) {
    throw "CHANGELOG.md has no '## $ver' section. Add release notes first."
}

# ---- toolchain (same pins as AGENTS.md / BUILD.md) ---------------------
if (-not $env:JAVA_HOME)        { $env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-17.0.19.10-hotspot' }
if (-not $env:ANDROID_HOME)     { $env:ANDROID_HOME = 'C:\Android\Sdk' }
if (-not $env:ANDROID_SDK_ROOT) { $env:ANDROID_SDK_ROOT = $env:ANDROID_HOME }
if (-not $env:ANDROID_NDK_HOME) { $env:ANDROID_NDK_HOME = Join-Path $env:ANDROID_HOME 'ndk\28.2.13676358' }
foreach ($p in @($env:JAVA_HOME, $env:ANDROID_HOME, $env:ANDROID_NDK_HOME)) {
    if (-not (Test-Path $p)) { throw "Missing toolchain path: $p" }
}
if (-not (Get-Command glslang -ErrorAction SilentlyContinue) -and -not (Get-Command glslangValidator -ErrorAction SilentlyContinue)) {
    Write-Warning "glslang is not on PATH; add the vcpkg host tools dir (see BUILD.md) or the shader step will fail."
}

# ---- build -------------------------------------------------------------
$apkSrc = Join-Path $root 'app\build\outputs\apk\release\app-release.apk'
if (-not $SkipBuild) {
    & .\gradlew.bat :app:assembleRelease
    if ($LASTEXITCODE -ne 0) { throw "gradle assembleRelease failed ($LASTEXITCODE)" }
}
if (-not (Test-Path $apkSrc)) { throw "APK not found: $apkSrc" }

# ---- verify ------------------------------------------------------------
$apksigner = Get-ChildItem (Join-Path $env:ANDROID_HOME 'build-tools') -Recurse -Filter apksigner.bat -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
if ($apksigner) {
    & $apksigner.FullName verify --verbose $apkSrc | Select-String 'Verifies|v2|v3'
    if ($LASTEXITCODE -ne 0) { throw "APK signature verification failed" }
} else {
    Write-Warning "apksigner not found, signature not verified."
}
$aapt = Get-ChildItem (Join-Path $env:ANDROID_HOME 'build-tools') -Recurse -Filter aapt2.exe -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
if ($aapt) {
    $badging = & $aapt.FullName dump badging $apkSrc | Select-String "package:"
    Write-Host $badging
    if ($badging -notmatch "versionName='$([regex]::Escape($ver))'") { throw "APK versionName does not match VERSION ($ver)" }
}

# ---- package -----------------------------------------------------------
$outDir = Join-Path $root 'release'
New-Item -ItemType Directory -Force $outDir | Out-Null
$apkOut = Join-Path $outDir "nextQuake-$ver-android-arm64.apk"
Copy-Item $apkSrc $apkOut -Force
$sha = (Get-FileHash $apkOut -Algorithm SHA256).Hash
"$sha  $(Split-Path -Leaf $apkOut)" | Set-Content (Join-Path $outDir "nextQuake-$ver-android-arm64.apk.sha256") -Encoding ascii

# release notes = the CHANGELOG section for this version
$lines = Get-Content CHANGELOG.md
$start = ($lines | Select-String -Pattern "^## \[?$([regex]::Escape($ver))\]?" | Select-Object -First 1).LineNumber
$rest = $lines[$start..($lines.Count - 1)]
$end = ($rest | Select-String -Pattern '^## ' | Select-Object -First 1)
$body = if ($end) { $rest[0..($end.LineNumber - 2)] } else { $rest }
$notes = Join-Path $outDir "RELEASE_NOTES-$ver.md"
@("# nextQuake $ver", "", $body, "", "APK SHA-256: ``$sha``", "",
  "Android 10+ (API 29), arm64-v8a. Needs the Quake data in Documents/ezQuake (see README).") | Set-Content $notes -Encoding utf8

Write-Host ""
Write-Host "== Done"
Write-Host "APK    : $apkOut"
Write-Host "SHA256 : $sha"
Write-Host "Notes  : $notes"
if ($dirty) { Write-Warning "Built from a DIRTY tree: do not publish this APK." }
Write-Host ""
Write-Host "Next steps (run by hand, nothing was committed/tagged/pushed):"
Write-Host "  git add VERSION CHANGELOG.md; git commit -m `"release: nextQuake $ver`""
Write-Host "  git tag -a $tag -m `"nextQuake $ver`""
Write-Host "  git push origin $branch $tag"
Write-Host "  gh release create $tag `"$apkOut`" `"$apkOut.sha256`" --title `"nextQuake $ver`" --notes-file `"$notes`""
