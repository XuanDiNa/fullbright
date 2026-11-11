[CmdletBinding()]
param([string]$GameImagesRoot='')
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
function Run([string]$Program,[string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw ($Program+' failed: '+($Arguments -join ' ')) }
}
$imageArguments=@()
if ($GameImagesRoot) {
    foreach ($name in 'Minecraft.Windows.exe.orig','Minecraft.Windows.exe') {
        $imageArguments += (Resolve-Path -LiteralPath (Join-Path $GameImagesRoot $name)).Path
    }
} else { Write-Output 'SKIP private exact-image checks: no locally owned images supplied.' }
Push-Location -LiteralPath $project
try {
    foreach ($mode in 'debug','release') {
        Run 'xmake' @('f','-P','.','-p','windows','-a','x64','-m',$mode,'-y')
        Run 'xmake' @('build','-P','.','-y','-j','4')
        foreach ($suite in 'Core','ConfigFile','Profile','Native','Smoke') {
            $arguments=@(); if ($suite -eq 'Profile') { $arguments=$imageArguments }
            Run (Join-Path $project ('build/windows/x64/'+$mode+'/FullBright'+$suite+'Tests.exe')) $arguments
        }
    }
    Run 'py' @('-3','tools/test_payload.py')
    Run 'py' @('-3','tools/check_release.py')
    Write-Output 'PASS five standalone suites per mode and payload mutation tests. No game launch or installation.'
} finally { Pop-Location }
