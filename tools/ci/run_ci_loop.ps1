param(

    # What to run: ci | day21 | day19-smoke | day19-smoke-fail | day19-gha-6h | day19-full

    # (also accepts workflow filename e.g. day21.yml)

    [string]$Target = "ci",

    # via-ci = dispatch ci.yml + inputs.suite (Mode A friendly)

    # direct  = dispatch standalone workflow yml on test branch (Mode B friendly)

    [ValidateSet("via-ci", "direct")]

    [string]$Route = "via-ci",

    # Branch whose code to build/test (default: current git branch)

    [string]$Ref = "",

    # Workflow definition ref for dispatch (via-ci default: main when Ref differs)

    [string]$DispatchRef = "",

    # Legacy overrides (normally auto-resolved from -Target / -Route)

    [string]$Workflow = "",

    [string]$WorkflowName = "",

    [string]$Suite = "",

    # Create a new branch before CI loop (from -FromBranch, push to origin)

    [string]$CreateBranch = "",

    [string]$FromBranch = "main",

    # Delete branch only (local+remote) then exit; no CI run

    [string]$DeleteBranch = "",

    # After CI green, delete -Ref branch (local+remote)

    [switch]$DeleteBranchOnSuccess,

    [int]$MaxIters = 20,

    [int]$PollInterval = 30,

    [int]$TimeoutPerRun = 0,

    # Wait/fetch only this Actions job (e.g. "cmake (ubuntu-latest)"); omit = whole workflow
    [string]$JobName = "",

    [switch]$SaveLogs

)



$ErrorActionPreference = "Stop"



function Get-CurrentBranch {

    $b = (git branch --show-current).Trim()

    if (-not $b) { throw "cannot detect current branch" }

    return $b

}



function ConvertTo-InputsJson {

    param([hashtable]$Inputs)

    if (-not $Inputs -or $Inputs.Count -eq 0) { return "" }

    $parts = @()

    foreach ($k in ($Inputs.Keys | Sort-Object)) {

        $parts += "`"$k`":`"$($Inputs[$k])`""

    }

    return "{" + ($parts -join ",") + "}"

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



# Legacy: -Suite day21 maps to -Target

if ($Suite -and $Suite -ne "full" -and $Target -eq "ci") {

    $Target = $Suite

}



# Legacy: -Workflow day21.yml without -Target

if ($Workflow -and $Target -eq "ci" -and $Workflow -ne "ci.yml") {

    $Target = $Workflow

    if ($Route -eq "via-ci") {

        $Route = "direct"

    }

}



$resolveArgs = @(

    "tools/ci/resolve_ci_target.py",

    "--target", $Target,

    "--ref", $Ref,

    "--route", $Route

)

if ($DispatchRef) {

    $resolveArgs += @("--dispatch-ref", $DispatchRef)

}



$planLine = python @resolveArgs

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$plan = $planLine | ConvertFrom-Json



$Workflow = if ($Workflow) { $Workflow } else { $plan.workflow_file }

$WorkflowName = if ($WorkflowName) { $WorkflowName } else { $plan.workflow_name }

$DispatchRef = $plan.dispatch_ref

$watchBranch = $plan.watch_branch

$useBranchInput = [bool]$plan.use_branch_input

$Suite = if ($Suite) { $Suite } else { $plan.inputs.suite }

if (-not $Suite) { $Suite = "full" }



if ($TimeoutPerRun -le 0) {

    $TimeoutPerRun = [int]$plan.timeout

}



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



Write-Host "[ci-loop] repo=little-white1111/BlessStar"

Write-Host "[ci-loop] target=$($plan.target) route=$($plan.route) label=$($plan.label)"

Write-Host "[ci-loop] workflow_file=$Workflow workflow_name=$WorkflowName"

Write-Host "[ci-loop] test_ref=$Ref dispatch_ref=$DispatchRef watch_branch=$watchBranch suite=$Suite"

if ($useBranchInput) {

    Write-Host "[ci-loop] checkout via inputs.branch=$Ref on workflow from $DispatchRef"

}

Write-Host "[ci-loop] timeout_per_run=${TimeoutPerRun}s token: GITHUB_TOKEN or tools/ci/.github_token"
if ($JobName) {
    Write-Host "[ci-loop] job_mode=$JobName (wait/fetch single job; full workflow may still run)"
} else {
    Write-Host "[ci-loop] job_mode=whole workflow"
}
Write-Host "[ci-loop] list targets: python tools/ci/resolve_ci_target.py list"
Write-Host "[ci-loop] list jobs:   python tools/ci/wait_workflow_job.py --run-id <run_id> --list-jobs"



for ($i = 1; $i -le $MaxIters; $i++) {

    Write-Host ""

    Write-Host ("=" * 72)

    Write-Host "[ci-loop] iter $i / $MaxIters"



    $dirty = git status --porcelain

    if ($dirty) {

        Write-Host "[ci-loop] working tree is dirty. Please commit & push, then press Enter."

        Read-Host | Out-Null

    } else {

        Write-Host "[ci-loop] working tree clean (good). Ensure remote is up-to-date."

    }



    $inputsHash = @{}

    if ($plan.inputs) {

        $plan.inputs.PSObject.Properties | ForEach-Object {

            $inputsHash[$_.Name] = [string]$_.Value

        }

    }

    if ($useBranchInput -and -not $inputsHash.ContainsKey("branch")) {

        $inputsHash["branch"] = $Ref

    }

    $inputsJson = ConvertTo-InputsJson -Inputs $inputsHash



    if ($inputsJson) {

        python tools/ci/trigger_workflow_dispatch.py --workflow $Workflow --ref $DispatchRef --inputs $inputsJson

    } else {

        python tools/ci/trigger_workflow_dispatch.py --workflow $Workflow --ref $DispatchRef

    }



    if ($JobName) {
        $waitOut = python tools/ci/wait_workflow_job.py --branch $watchBranch --workflow_name $WorkflowName `
            --job-name $JobName --interval $PollInterval --timeout $TimeoutPerRun
    } else {
        $waitOut = python tools/ci/wait_workflow_run.py --branch $watchBranch --workflow_name $WorkflowName `
            --interval $PollInterval --timeout $TimeoutPerRun
    }

    $lines = $waitOut -split "`n"
    $runId = ($lines[-1]).Trim()

    if (-not $runId) { throw "failed to parse run id" }

    Write-Host "[ci-loop] run id = $runId"

    $exitCode = $LASTEXITCODE

    if ($exitCode -eq 0) {
        if ($JobName) {
            Write-Host "[ci-loop] ✅ job '$JobName' green. Stop."
        } else {
            Write-Host "[ci-loop] ✅ CI green. Stop."
        }

        if ($DeleteBranchOnSuccess) {

            Write-Host "[ci-loop] deleting branch $Ref (local+remote)"

            python tools/ci/manage_branch.py delete $Ref --local --remote --fallback $FromBranch

            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        }

        exit 0

    }



    $save = ""
    if ($SaveLogs) { $save = "--save-logs" }

    if ($JobName) {
        Write-Host "[ci-loop] ❌ job '$JobName' failed. Fetching errors..."
        python tools/ci/fetch_ci_errors.py --branch $watchBranch --run $runId --job-name $JobName $save
    } else {
        Write-Host "[ci-loop] ❌ CI failed. Fetching errors..."
        python tools/ci/fetch_ci_errors.py --branch $watchBranch --run $runId $save
    }



    Write-Host ""

    Write-Host "[ci-loop] Fix issues locally, commit + push, then press Enter to rerun."

    Read-Host | Out-Null

}



Write-Host "[ci-loop] reached MaxIters=$MaxIters without green."

exit 4

