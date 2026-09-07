# ------------------------------------------------------------------------------
#  sync-deploy.ps1 — Compile LuxRev-1 et deploie le VST3 systeme.
#
#  Meme enchainement que TOTUMstrip, et pour les memes raisons :
#    0. refuse de tourner si le DAW est ouvert (Windows verrouille le VST3
#       tant qu'il est charge, et la copie echoue a moitie)
#    1. regenere Programmes.h depuis l'accordage — le plugin ne doit jamais
#       diverger de _proto/accord/programmes.json
#    2. Projucer --resave (le .jucer est la source de verite du projet)
#    3. msbuild Release x64
#    4. pluginval — refuse de deployer si la validation echoue
#    5. deploie vers Common Files\VST3
#
#  Parametre : severite de pluginval, de 1 a 10. 5 pour une passe de routine,
#  10 avant de partager une version.
#
#  ⚠️ pluginval N'A PAS D'OREILLES. Il attrape les fautes de CONSTRUCTION —
#  plantages, NaN, denormaux, latence mal declaree, etat qui ne se recharge pas.
#  Un moteur qui sonne faux passe au vert. Pour le SON, le juge est
#  `_proto/comparer_portage.py`, qui compare le C++ au prototype accorde sur les
#  mesures des 14 IR — et, en dernier ressort, l'oreille.
# ------------------------------------------------------------------------------

param([int]$PluginvalLevel = 5)

$ErrorActionPreference = 'Stop'

$racine   = Split-Path $PSScriptRoot -Parent                 # ...\LuxRev-1
$projet   = Join-Path $racine 'LuxRev-1.jucer'
$builds   = Join-Path $racine 'Builds\VisualStudio2022'
$sln      = Join-Path $builds 'LuxRev-1.sln'
$src      = Join-Path $builds 'x64\Release\VST3\LuxRev-1.vst3'
$dst      = 'C:\Program Files\Common Files\VST3\LuxRev-1.vst3'
$projucer = 'C:\Users\llg53\Desktop\JUCE\Projucer.exe'

function Info ($m) { Write-Host "  $m" -ForegroundColor Cyan }

# --- 0. le DAW verrouille le VST3 charge ---------------------------------------
foreach ($p in 'FL64', 'FL', 'Ableton Live', 'reaper', 'Cubase') {
    if (Get-Process -Name $p -ErrorAction SilentlyContinue) {
        Write-Host "  $p est ouvert. Windows verrouille le VST3 tant qu'il est charge." -ForegroundColor Red
        exit 1
    }
}

# --- 1. les programmes suivent l'accordage -------------------------------------
Info 'generer_programmes.py ...'
python (Join-Path $PSScriptRoot 'generer_programmes.py')
if ($LASTEXITCODE -ne 0) { throw 'generation des programmes echouee' }

# --- 2. le .jucer est la source de verite --------------------------------------
if (Test-Path $projucer) {
    Info 'Projucer --resave ...'
    & $projucer --resave "$projet"
    if ($LASTEXITCODE -ne 0) { throw 'Projucer --resave a echoue' }
} else {
    Info 'Projucer absent — on compile la solution telle quelle'
}

# --- 3. compilation ------------------------------------------------------------
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
             -latest -requires Microsoft.Component.MSBuild `
             -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild introuvable' }

Info 'msbuild Release x64 ...'
& $msbuild "$sln" /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'compilation echouee' }
if (-not (Test-Path $src)) { throw "VST3 introuvable : $src" }

# --- 4. pluginval, verrou de deploiement ---------------------------------------
$pluginval = Join-Path (Split-Path (Split-Path $racine -Parent) -Parent) `
                       'TOTUMstrip VST\_outils\pluginval\pluginval.exe'
if (Test-Path $pluginval) {
    Info "pluginval (severite $PluginvalLevel) ..."
    & $pluginval --strictness-level $PluginvalLevel --timeout-ms 600000 --validate "$src" |
        Select-String -Pattern 'FAILED|SUCCESS|!!!' | ForEach-Object { Write-Host "    $_" }
    if ($LASTEXITCODE -ne 0) {
        Write-Host '  pluginval a echoue — RIEN N''EST DEPLOYE.' -ForegroundColor Red
        exit 1
    }
    Info 'pluginval : SUCCESS'
} else {
    Info 'pluginval absent — validation sautee'
}

# --- 5. deploiement ------------------------------------------------------------
Info "deploiement vers $dst"
if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
Copy-Item $src $dst -Recurse -Force
Write-Host '  LuxRev-1 deploye.' -ForegroundColor Green
Write-Host '  ⚠️ pluginval ne juge pas le son. Pour ca : python _proto/comparer_portage.py' -ForegroundColor DarkYellow
