# The MSVC leg of canon's determinism golden proof: build det_proof from canon's source at /O2 /fp:fast
# and write its digest over proof/corpus to digest-msvc.txt at the workspace root — the bytes the four
# Linux legs (scripts/det_public_proof.sh) emit, byte-compared by the golden's `compare` job.
#
# Run by malf-toolchain's coderoast-golden-proof.yml on its MSVC leg, after `setup-proof-msvc` has
# activated MSVC 14.52, CMake 4.3.x, Ninja and Conan and staged the windows-msvc-release profile into
# $env:CONAN_HOME. It stays a committed script of this repository, not inline YAML, because WHAT a leg
# builds and emits is the proof's subject and belongs to the repository that owns it.
#
# Emits in pwsh, never by calling the bash driver: on a Windows runner `bash` is Git Bash only by
# accident of PATH, and on our self-hosted host it resolves to WSL.

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$workspace = $env:GITHUB_WORKSPACE
Set-Location $workspace

if (-not $env:MALF_TOOLCHAIN_DIR) { Write-Error "MALF_TOOLCHAIN_DIR is unset: the install resolves only against <malf-toolchain>/conan.lock"; exit 1 }
$proofBuild = "build-msvc-proof"
conan install core `
  --profile:host="$env:CONAN_HOME/profiles/windows-msvc-release" `
  --profile:build="$env:CONAN_HOME/profiles/windows-msvc-release" `
  --build=missing --lockfile="$env:MALF_TOOLCHAIN_DIR/conan.lock" -of $proofBuild
$toolchain = Get-ChildItem -Path $proofBuild -Recurse -Filter conan_toolchain.cmake | Select-Object -First 1
if (-not $toolchain) { Write-Error "no conan_toolchain.cmake under $proofBuild"; exit 1 }
# CELL_FLAGS = SPDLOG off (the digest is det_proof's stdout) + /O2 /fp:fast (ship optimization +
# contraction ON — the dominating corner; matching the other legs proves contraction-invariance).
cmake -S proof -B $proofBuild -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$($toolchain.FullName)" `
  -DCANON_ROOT="$($PWD.Path)" `
  -DCELL_FLAGS="-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF /O2 /fp:fast"
cmake --build $proofBuild --target det_proof
$det = Get-ChildItem -Path $proofBuild -Recurse -Filter det_proof.exe | Select-Object -First 1
if (-not $det) { Write-Error "det_proof.exe was not produced — the MSVC det harness build failed"; exit 1 }

# ORDINAL, not Sort-Object: det_proof prints one section per argument, so this order IS digest order
# on one side of a five-leg byte compare. `Sort-Object Name` collates culture-aware, where punctuation
# is ignorable — measured on pwsh 7.4.6, service.log / service_a.log / service-b.log come back in a
# different order than `LC_ALL=C sort` gives on the Linux legs (scripts/det_public_proof.sh pins
# that). -Path wildcard, never -Filter: `-Filter` goes to FindFirstFile, which matches a file's 8.3
# SHORT name too, so `*.log` there also takes `<name>.logfile`, while the Linux leg globs `*.log`.
$byName = @{}
Get-ChildItem -Path proof/corpus/*.log -File | ForEach-Object { $byName[$_.Name] = $_.FullName }
$names = [string[]]$byName.Keys
[Array]::Sort($names, [StringComparer]::Ordinal)
$corpus = @($names | ForEach-Object { $byName[$_] })
if ($corpus.Count -eq 0) { Write-Error "no *.log under proof/corpus"; exit 1 }
$p = Start-Process -FilePath $det.FullName -ArgumentList $corpus -NoNewWindow -Wait -PassThru `
       -RedirectStandardOutput "$workspace/digest-msvc.txt"
if ($p.ExitCode -ne 0) { Write-Error "det_proof exited $($p.ExitCode)"; exit 1 }
Write-Host "MSVC digest sha256: $((Get-FileHash -Algorithm SHA256 -Path "$workspace/digest-msvc.txt").Hash.ToLower())"
