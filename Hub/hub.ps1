<#
hub.ps1: o status da sua sessão no painel (Hub). Uma linha, sem clicar em nada. As filas da placa e do EmberGen
continuam exatamente como são (gpu_queue.ps1 / ember_queue.ps1): o painel lê os tickets delas sozinho.

  $hub = 'C:\Users\luisf\explosion\Hub\hub.ps1'
  powershell -NoProfile -File $hub set "BlastLab" "A/B da cor da fumaça (3 curtas)"              # trabalhando (padrão)
  powershell -NoProfile -File $hub set "BlastLab" "esperando o Felipe escolher A ou B" -State waiting
  powershell -NoProfile -File $hub set "BlastLab" "compile quebrado no Look" -State blocked -Note "esperando a sessão do Look"
  powershell -NoProfile -File $hub set "BlastLab" "fumaça pronta pra ver" -State review
  powershell -NoProfile -File $hub done "BlastLab" "fumaça aprovada, tag v7"                      # terminou (some dos ativos)
  powershell -NoProfile -File $hub status                                                        # o painel em texto
  powershell -NoProfile -File $hub open                                                          # abre a janela do painel

  -Session: o NOME que a sessão usa (de preferência o título da barra lateral; serve o mesmo nome que você põe no
            -Owner das filas, ex. "BlastLab"). -Alias: outros nomes que você usa nos -Owner das filas, pra o painel
            juntar tudo no seu cartão. -Title: o título exato da barra lateral, se o nome for diferente.
  Estados: working (padrão) | waiting | blocked | review | done
#>
param(
    [Parameter(Position = 0)][ValidateSet('set', 'done', 'clear', 'status', 'open', 'serve')][string]$Action = 'status',
    [Parameter(Position = 1)][string]$Session = '',
    [Parameter(Position = 2)][string]$Doing = '',
    [ValidateSet('working', 'waiting', 'blocked', 'review', 'done')][string]$State = 'working',
    [string]$Note = '',
    [string[]]$Alias = @(),
    [string]$Title = '',
    [int]$Port = 8140
)

$Dir = Join-Path $env:USERPROFILE '.hub\sessions'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = 'C:\Users\luisf\AppData\Local\Programs\Python\Python312\python.exe'
if (-not (Test-Path $Python)) { $Python = 'python' }
New-Item -ItemType Directory -Force $Dir | Out-Null

function Slug($s) { ($s.ToLower() -replace '[^a-z0-9]+', '-').Trim('-') }
function Load($s) {
    $f = Join-Path $Dir ((Slug $s) + '.json')
    if (Test-Path $f) { try { return (Get-Content $f -Raw -Encoding UTF8 | ConvertFrom-Json) } catch {} }
    $null
}
function Save($rec) {
    $f = Join-Path $Dir ((Slug $rec.session) + '.json')
    [IO.File]::WriteAllText($f, ($rec | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding $false))
}
function Server-Up { try { (Invoke-WebRequest "http://localhost:$Port/state" -UseBasicParsing -TimeoutSec 2).StatusCode -eq 200 } catch { $false } }

switch ($Action) {
    'set' {
        if (-not $Session) { Write-Error 'Falta o nome da sessão: hub.ps1 set "Nome" "o que está fazendo"'; exit 4 }
        $old = Load $Session
        $now = (Get-Date).ToString('o')
        $hist = @(); if ($old -and $old.history) { $hist = @($old.history) }
        $changed = (-not $old) -or ($old.doing -ne $Doing) -or ($old.state -ne $State)
        if ($changed -and $old -and $old.doing) { $hist = @(@{ t = $old.since; state = $old.state; doing = $old.doing }) + $hist }
        $aliases = @(); if ($old -and $old.aliases) { $aliases = @($old.aliases) }
        foreach ($a in $Alias) { if ($a -and ($aliases -notcontains $a)) { $aliases += $a } }
        $rec = [ordered]@{
            session = $Session
            title   = if ($Title) { $Title } elseif ($old -and $old.title) { $old.title } else { '' }
            aliases = $aliases
            state   = $State
            doing   = $Doing
            note    = if ($Note) { $Note } elseif (-not $changed -and $old) { $old.note } else { '' }
            since   = if ($changed -or -not $old) { $now } else { $old.since }
            updated = $now
            history = @($hist | Select-Object -First 8)
        }
        Save $rec
        "$Session · $State · $Doing"
    }
    'done' {
        if (-not $Session) { Write-Error 'Falta o nome da sessão.'; exit 4 }
        & $MyInvocation.MyCommand.Path set $Session $Doing -State done -Note $Note -Title $Title
    }
    'clear' {
        if (-not $Session) { Write-Error 'Falta o nome da sessão.'; exit 4 }
        Remove-Item (Join-Path $Dir ((Slug $Session) + '.json')) -Force -ErrorAction SilentlyContinue
        "$Session · apagado do painel"
    }
    'status' {
        if (Server-Up) {
            $s = Invoke-RestMethod "http://localhost:$Port/state"
            foreach ($pair in @(@('unreal', 'Placa'), @('ember', 'EmberGen'))) {
                $q = $s.($pair[0]); $label = $pair[1]
                if ($q.holder) { "{0}: em uso por {1} ({2}) há {3:N0} min" -f $label, $q.holder.session, $q.holder.task, $q.holder.since_min } else { "${label}: livre" }
                $i = 1; foreach ($t in $q.waiting) { "   {0}. {1} ({2}) esperando há {3:N0} min" -f $i, $t.session, $t.task, $t.since_min; $i++ }
            }
            'Sessões:'
            foreach ($c in $s.sessions) {
                $st = if ($c.status) { $c.status.state } else { '-' }
                $doing = if ($c.status) { $c.status.doing } elseif ($c.unreal) { $c.unreal.task } elseif ($c.ember) { $c.ember.task } else { '' }
                "   [{0}] {1}: {2}" -f $st, $c.title, $doing
            }
        } else {
            "painel fora do ar (hub.ps1 open sobe). Status gravados:"
            Get-ChildItem $Dir -Filter *.json | ForEach-Object { $r = Get-Content $_.FullName -Raw -Encoding UTF8 | ConvertFrom-Json; "   [{0}] {1}: {2}" -f $r.state, $r.session, $r.doing }
        }
    }
    'serve' {
        & $Python (Join-Path $Here 'server.py') $Port
    }
    'open' {
        if (-not (Server-Up)) {
            Start-Process -FilePath $Python -ArgumentList @((Join-Path $Here 'server.py'), $Port) -WindowStyle Hidden -WorkingDirectory $Here
            $t = 0; while (-not (Server-Up) -and $t -lt 20) { Start-Sleep -Milliseconds 250; $t++ }
        }
        $chrome = 'C:\Program Files\Google\Chrome\Application\chrome.exe'
        if (Test-Path $chrome) {
            Start-Process $chrome -ArgumentList @("--app=http://localhost:$Port", '--window-size=1180,840')
        } else { Start-Process "http://localhost:$Port" }
        "Hub em http://localhost:$Port"
    }
}
