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
trava continua respeitado. Desde 14/10 isso inclui o **jogo empacotado** (`RTSGame.exe` sem `-nullrhi`):
quando o Felipe joga a versão do pacote, todos esperam, como manda "teste dele é dele". O `FXLab/Tools/fxlab.ps1 shot` usa a fila.

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

**O EmberGen tem a fila dele** (decisão do Felipe, 13/10): o EmberGen **não entra na fila da placa**, mas também
**não é de todo mundo ao mesmo tempo**. A regravação levava 20 a 40 min segurando a fila da placa e os testes rápidos
de todas as sessões esperavam; e mais de uma sessão quer o EmberGen. Por isso ele tem uma fila separada:

```powershell
powershell -NoProfile -File C:\Users\luisf\.ember_queue\ember_queue.ps1 enter -Owner "<sessão> | <tarefa>"   # 0 = é seu; 1 = chame de novo em até 2 min
powershell -NoProfile -File C:\Users\luisf\.ember_queue\ember_queue.ps1 touch -Owner "..."   # a cada ~5-10 min
powershell -NoProfile -File C:\Users\luisf\.ember_queue\ember_queue.ps1 leave -Owner "..."   # SEMPRE ao fechar o EmberGen
powershell -NoProfile -File C:\Users\luisf\.ember_queue\ember_queue.ps1 status
```

- Quem chegou primeiro usa primeiro. Ninguém abre o EmberGen sem estar na frente dessa fila.
- Ela **não** escreve o `captures/.gpu_lock` nem olha a Unreal: a fila da placa segue livre pros testes.
- A Unreal que abrir por cima do EmberGen (o BlastLab pra importar, os outros testes) segue a fila da placa normal.
- `-Mine`: o EmberGen que já está aberto é seu (aberto antes da fila existir); entra sem esperar por ele.
- Fechar o EmberGen sempre pelo PID que você abriu, e dar o `leave`.

Os cuidados:
- **Memória da placa:** 6 GB. EmberGen + uma Unreal pesada podem estourar. Quem cair por isso
  repete; se repetir, avisa a sessão do EmberGen.
- **Medida de custo:** o teste que mede milissegundos (A/B de custo) não vale com o EmberGen aberto.
  Antes de medir, conferir o processo (`Get-Process EmberGen`): esperar ou marcar a medida como suja e
  repetir. Foto e funcionamento podem rodar junto.

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
