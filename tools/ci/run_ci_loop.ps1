param(
    # Which workflow file to run (default: ci.yml in .github/workflows/)
    [string]$Workflow = "ci.yml",
    # Which ref/branch to run on (default: current git branch)
    [string]$Ref = "",
    # Max iterations (push -> run -> fix -> push ...). This script does NOT auto-fix code.
    [int]$MaxIters = 20,
    # Poll interval seconds while waiting run completion
    [int]$PollInterval = 30,
    # Timeout per CI run (seconds)
    [int]$TimeoutPerRun = 3600,
    # Save full failed job logs under ci_logs/
    [switch]$SaveLogs
)

$ErrorActionPreference = "Stop"

function Get-CurrentBranch {
    $b = (git branch --show-current).Trim()
    if (-not $b) { throw "cannot detect current branch" }
    return $b
}

if (-not $Ref) {
    $Ref = Get-CurrentBranch
}

Write-Host "[ci-loop] repo=little-white1111/BlessStar workflow=$Workflow ref=$Ref"
Write-Host "[ci-loop] token source: env:GITHUB_TOKEN or tools/ci/.github_token"

for ($i = 1; $i -le $MaxIters; $i++) {
    Write-Host ""
    Write-Host ("=" * 72)
    Write-Host "[ci-loop] iter $i / $MaxIters"

    # 1) Ensure changes are pushed (manual step)
    $dirty = git status --porcelain
    if ($dirty) {
        Write-Host "[ci-loop] working tree is dirty. Please commit & push, then press Enter."
        Read-Host | Out-Null
    } else {
        Write-Host "[ci-loop] working tree clean (good). Ensure remote is up-to-date."
    }

    # 2) Trigger workflow_dispatch
    python tools/ci/trigger_workflow_dispatch.py --workflow $Workflow --ref $Ref

    # 3) Wait completion and capture run id (last line)
    $waitOut = python tools/ci/wait_workflow_run.py --branch $Ref --workflow_name "full test" --interval $PollInterval --timeout $TimeoutPerRun
    $lines = $waitOut -split "`n"
    $runId = ($lines[-1]).Trim()
    if (-not $runId) { throw "failed to parse run id" }
    Write-Host "[ci-loop] run id = $runId"

    # 4) Fetch errors if failed
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0) {
        Write-Host "[ci-loop] ✅ CI green. Stop."
        exit 0
    }

    $save = ""
    if ($SaveLogs) { $save = "--save-logs" }
    Write-Host "[ci-loop] ❌ CI failed. Fetching errors..."
    python tools/ci/fetch_ci_errors.py --branch $Ref --run $runId $save

    # 5) Manual fix step
    Write-Host ""
    Write-Host "[ci-loop] Fix issues locally, commit + push, then press Enter to rerun."
    Read-Host | Out-Null
}

Write-Host "[ci-loop] reached MaxIters=$MaxIters without green."
exit 4

