# A placa de vídeo é uma só: a trava entre as sessões

A GTX 1660 (6 GB) não aguenta duas Unreal abertas com janela ao mesmo tempo: em 07/10/2026
caiu (DXGI_ERROR_DEVICE_HUNG). Várias sessões do Claude usam a placa (BlastLab, FXLab,
WorldForge, o jogo RTSGame, renders no Blender). O Felipe não fica no meio liberando ("pode
abrir agora", "espera"): a trava abaixo resolve sozinha, sem mensagem entre as sessões.

## A fila (preferida, desde 08/10)

`C:\Users\luisf\.gpu_queue\gpu_queue.ps1` (fora do repositório, comum a todas as sessões; o cabeçalho explica):
quem chega primeiro usa primeiro, em vez de ganhar quem confere a trava mais vezes.

```powershell
$q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
do { powershell -NoProfile -File $q enter -Owner "minha-sessão | tarefa" -Tag "<pedaço da linha de comando dos meus processos>" } while ($LASTEXITCODE -ne 0)
try { <usar a placa; a cada ~5-10 min: powershell -NoProfile -File $q touch -Owner "..."> }
finally { powershell -NoProfile -File $q leave -Owner "..." }
```

A fila escreve a trava abaixo e espera qualquer Unreal com janela de fora, então quem ainda usa só a
trava continua respeitado. O `FXLab/Tools/fxlab.ps1 shot` usa a fila.

## A regra (a trava)

Antes de abrir qualquer Unreal com janela (editor, jogo, captura) ou qualquer render pesado na
placa (Blender com GPU):

1. Olhar `captures/.gpu_lock` (na raiz do repositório, fora do git).
2. Se existe e foi mexido há menos de 20 min: esperar, olhando de 15 em 15 s. Não perguntar a
   ninguém e não apagar a trava dos outros.
3. Se não existe (ou tem mais de 20 min: dono caiu): criar com uma linha
   `sessão | tarefa | hora` e só então abrir.
4. Ao fechar a Unreal (ou terminar o render), apagar a trava. Também quando der erro ou for
   cancelado.
5. Trabalho longo (mais de 15 min seguidos): atualizar a hora do arquivo a cada ~10 min, senão
   os outros acham que é trava velha.
6. Não segurar a placa à toa: entre um teste e outro, fechar e soltar, pra os outros intercalarem.
   Editor parado aberto também ocupa a placa.

Fora da trava, mas que também conta:
- O Felipe abriu um app pra testar: a placa é dele. Nada automático por cima
  (memória "teste dele é dele").
- Bake no Blender só com CPU (`cycles.device = 'CPU'`) não precisa da trava.

## Exemplo (PowerShell)

```powershell
$Lock = 'C:\Users\luisf\explosion\captures\.gpu_lock'
while ((Test-Path $Lock) -and (((Get-Date) - (Get-Item $Lock).LastWriteTime).TotalMinutes -lt 20)) { Start-Sleep 15 }
Set-Content $Lock "FXLab | captura do tiro | $(Get-Date -Format s)"
try { <abrir a Unreal e esperar fechar> } finally { Remove-Item $Lock -ErrorAction SilentlyContinue }
```

O `FXLab/Tools/fxlab.ps1 shot` já faz isso.
