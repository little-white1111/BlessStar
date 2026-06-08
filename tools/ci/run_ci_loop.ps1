param(
    # Which workflow file to run (default: ci.yml in .github/workflows/)
    [string]$Workflow = "ci.yml",
    # Branch whose code to build/test (default: current git branch)
    [string]$Ref = "",
    # Workflow definition ref for dispatch (default: same as Ref).
    # Set to "main" to always use main's ci.yml while testing another branch via inputs.branch.
    [string]$DispatchRef = "",
    # workflow display name for wait_workflow_run.py (ci.yml -> "full test")
    [string]$WorkflowName = "full test",
    # ci.yml workflow_dispatch.inputs.suite (full | day21 | day19-smoke | ...)
    [ValidateSet("full", "day21", "day19-smoke", "day19-smoke-fail", "day19-gha-6h", "day19-full")]
    [string]$Suite = "full",
    # Create a new branch before CI loop (from -FromBranch, push to origin)
    [string]$CreateBranch = "",
    [string]$FromBranch = "main",
    # Delete branch only (local+remote) then exit; no CI run
    [string]$DeleteBranch = "",
    # After CI green, delete -Ref branch (local+remote)
    [switch]$DeleteBranchOnSuccess,
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

function Get-DispatchInputsJson {
    param(
        [bool]$UseBranchInput,
        [string]$TargetRef,
        [string]$SuiteName
    )
    $parts = @()
    if ($UseBranchInput) {
        $parts += "`"branch`":`"$TargetRef`""
    }
    if ($SuiteName -and $SuiteName -ne "full") {
        $parts += "`"suite`":`"$SuiteName`""
    }
    if ($parts.Count -eq 0) { return "" }
    return "{" + ($parts -join ",") + "}"
}

function Get-DefaultTimeoutForSuite {
    param([string]$SuiteName)
    switch ($SuiteName) {
        "day21" { return 1800 }
        "day19-smoke" { return 1800 }
        "day19-smoke-fail" { return 1800 }
        "day19-gha-6h" { return 21600 }
        "day19-full" { return 270000 }
        default { return 3600 }
    }
}

if ($DeleteBranch) {
    Write-Host "[ci-loop] delete-only mode: $DeleteBranch"
    python tools/ci/manage_branch.py delete $DeleteBranch --local --remote
    exit $LASTEXITCODE
}

if ($CreateBranch) {
    Write-Host "[ci-loop] create branch: $CreateBranch from $FromBranch"
    python tools/ci/manage_branch.py create $CreateBranch --from $FromBranch
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $Ref = $CreateBranch
    if (-not $DispatchRef) {
        $DispatchRef = $FromBranch
    }
}

if (-not $Ref) {
    $Ref = Get-CurrentBranch
}
if (-not $DispatchRef) {
    $DispatchRef = $Ref
}

$watchBranch = $DispatchRef
$useBranchInput = ($DispatchRef -ne $Ref)

# Ensure remote branch exists before dispatch (CI needs pushed ref)
$existsLine = python tools/ci/manage_branch.py exists $Ref 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ci-loop] branch $Ref missing locally and on origin."
    exit 2
}
$existsInfo = $existsLine | ConvertFrom-Json
if (-not $existsInfo.remote) {
    Write-Host "[ci-loop] branch $Ref not on origin. Commit, push, then rerun."
    Write-Host "  git push -u origin $Ref"
    exit 2
}

$suiteTimeout = Get-DefaultTimeoutForSuite -SuiteName $Suite
if ($TimeoutPerRun -eq 3600 -and $suiteTimeout -ne 3600) {
    $TimeoutPerRun = $suiteTimeout
}

Write-Host "[ci-loop] repo=little-white1111/BlessStar workflow=$Workflow test_ref=$Ref dispatch_ref=$DispatchRef suite=$Suite"
if ($useBranchInput) {
    Write-Host "[ci-loop] mode: dispatch from $DispatchRef, checkout inputs.branch=$Ref"
}
Write-Host "[ci-loop] timeout_per_run=${TimeoutPerRun}s token: GITHUB_TOKEN or tools/ci/.github_token"

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
    $inputsJson = Get-DispatchInputsJson -UseBranchInput $useBranchInput -TargetRef $Ref -SuiteName $Suite
    if ($inputsJson) {
        $dispatchRef = if ($useBranchInput) { $DispatchRef } else { $Ref }
        python tools/ci/trigger_workflow_dispatch.py --workflow $Workflow --ref $dispatchRef --inputs $inputsJson
    } else {
        python tools/ci/trigger_workflow_dispatch.py --workflow $Workflow --ref $Ref
    }

    # 3) Wait completion and capture run id (last line)
    $waitOut = python tools/ci/wait_workflow_run.py --branch $watchBranch --workflow_name $WorkflowName --interval $PollInterval --timeout $TimeoutPerRun
    $lines = $waitOut -split "`n"
    $runId = ($lines[-1]).Trim()
    if (-not $runId) { throw "failed to parse run id" }
    Write-Host "[ci-loop] run id = $runId"

    # 4) Fetch errors if failed
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0) {
        Write-Host "[ci-loop] ✅ CI green. Stop."
        if ($DeleteBranchOnSuccess) {
            Write-Host "[ci-loop] deleting branch $Ref (local+remote)"
            python tools/ci/manage_branch.py delete $Ref --local --remote --fallback $FromBranch
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
        exit 0
    }

    $save = ""
    if ($SaveLogs) { $save = "--save-logs" }
    Write-Host "[ci-loop] ❌ CI failed. Fetching errors..."
    python tools/ci/fetch_ci_errors.py --branch $watchBranch --run $runId $save

    # 5) Manual fix step
    Write-Host ""
    Write-Host "[ci-loop] Fix issues locally, commit + push, then press Enter to rerun."
    Read-Host | Out-Null
}

Write-Host "[ci-loop] reached MaxIters=$MaxIters without green."
exit 4

