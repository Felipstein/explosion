# As telas novas no jogo, guardadas antes de compilar (01/11)

Cópia de segurança (pedido do Felipe: "commita o que está feito pra não perder") do plugin `RTSShell` e do
`RTSShellGame` **antes** da primeira compilação. Esta pasta não é compilada (o UBT só lê `Source/` e `Plugins/`).

- `apply_shared.py apply` põe o plugin e os dois arquivos no lugar e liga os ganchos nos arquivos compartilhados
  (`RTSPlayerController`, `RTSHUD`, `RTSSessionSubsystem`, `RTSMatchSubsystem`, o `.uproject` e o `Build.cs`);
  `revert` desfaz. Só na vez da fila, sem compilação de ninguém rodando (`compile_shell.ps1` faz tudo).
- As páginas (`Resources/UI`) saem do protótipo por `docs/concept/ui/menu/tools/export_game.py`.

Depois que compilar e for testado, os arquivos de verdade ficam em `Game/Plugins/RTSShell` e
`Game/Source/RTSGame/*/RTSShellGame.*`, e esta pasta sai.
