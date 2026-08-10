param(
    [string]$Server = "localhost:50051",
    [string]$NodeExe = "$PSScriptRoot/../build/node.exe"
)

$logDir = "$PSScriptRoot/logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Start-Node($id, [string[]]$nodeArgs) {
    $log = "$logDir/$id.log"
    Start-Process -FilePath $NodeExe -ArgumentList $nodeArgs -NoNewWindow -RedirectStandardOutput $log -RedirectStandardError "$log.err"
}

Start-Node "mm-tight"    @("--strategy=market-maker", "--node-id=mm-tight", "--server=$Server", "--mm-spread=0.30", "--mm-size=10")
Start-Node "mm-wide"     @("--strategy=market-maker", "--node-id=mm-wide", "--server=$Server", "--mm-spread=1.00", "--mm-size=15")
Start-Node "mom-fast"    @("--strategy=momentum", "--node-id=mom-fast", "--server=$Server", "--mom-window=10", "--mom-threshold-pct=0.0008")
Start-Node "mom-slow"    @("--strategy=momentum", "--node-id=mom-slow", "--server=$Server", "--mom-window=30", "--mom-threshold-pct=0.002")
Start-Node "mr-1"        @("--strategy=mean-reversion", "--node-id=mr-1", "--server=$Server", "--mr-window=30", "--mr-deviation-pct=0.01")
Start-Node "noise-1"     @("--strategy=noise", "--node-id=noise-1", "--server=$Server", "--noise-action-prob=0.3")
Start-Node "noise-2"     @("--strategy=noise", "--node-id=noise-2", "--server=$Server", "--noise-action-prob=0.4", "--noise-size-max=8")
Start-Node "noise-3"     @("--strategy=noise", "--node-id=noise-3", "--server=$Server", "--noise-action-prob=0.2")
Start-Node "noise-4"     @("--strategy=noise", "--node-id=noise-4", "--server=$Server", "--noise-action-prob=0.35")
$replayFileArg = '--replay-file="' + "$PSScriptRoot\strategies\replay_data\sample_series.csv" + '"'
Start-Node "replay-1"    @("--strategy=replay", "--node-id=replay-1", "--server=$Server", $replayFileArg, "--replay-speed=10")

Write-Host "Launched 10 nodes against $Server. Logs in $logDir"
