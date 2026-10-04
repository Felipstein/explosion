# Restaurar a sessão do Mac no Windows

Este arquivo é pro Claude Code que o Felipe abrir no Windows (app Claude
Desktop, aba Code, na pasta deste repositório) depois de clonar. O Felipe vai
pedir pra você seguir estas instruções. **Execute os passos 1 e 2 você
mesmo**; no passo 3 ele abre a sessão restaurada no próprio app.

Os comandos estão em PowerShell. Se o seu terminal for o bash do Git, rode-os
com `powershell -NoProfile -Command "..."` ou faça o equivalente em bash.

O zip `sessao/sessao-claude-explosion.zip` traz a sessão inteira do Mac:
- a conversa, `7fab7fca-b804-45c4-8302-972a750d5b27.jsonl`;
- a pasta de mesmo nome, com os resultados guardados das ferramentas;
- as memórias do projeto, `memory/`.

Retomar essa sessão traz de volta o Claude que fez o projeto até aqui, com
todo o contexto.

## Passos (execute no PowerShell, na raiz do repositório clonado)

1. **Achar a pasta de sessões deste projeto no Windows.** O Claude Code
   guarda as sessões em `%USERPROFILE%\.claude\projects\<pasta>`, onde
   `<pasta>` é o caminho do projeto com tudo que não é letra ou número trocado
   por `-` (ex.: `C:\Users\Felipe\explosion` → `C--Users-Felipe-explosion`). A
   sua própria sessão, esta que está lendo o arquivo, já criou essa pasta;
   confirme com:

   ```powershell
   Get-ChildItem "$env:USERPROFILE\.claude\projects" | Sort-Object LastWriteTime -Descending | Select-Object -First 5 Name, LastWriteTime
   ```

   Use a que corresponde ao caminho atual (`(Get-Location).Path`). Na
   dúvida, a certa é a que contém o `.jsonl` da sua própria sessão atual (o
   mais recente). Guarde numa variável:

   ```powershell
   $dest = Join-Path "$env:USERPROFILE\.claude\projects" ((Get-Location).Path -replace '[^A-Za-z0-9]', '-')
   Test-Path $dest   # tem que dar True; se não, ajuste $dest pro nome que apareceu na lista acima
   ```

2. **Descompactar e copiar** a conversa, a pasta da sessão e as memórias:

   ```powershell
   $tmp = Join-Path $env:TEMP "sessao-explosion"
   Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
   Expand-Archive -Path "sessao\sessao-claude-explosion.zip" -DestinationPath $tmp
   Copy-Item "$tmp\-Users-felipe-explosion\*" $dest -Recurse -Force
   Get-ChildItem $dest   # deve listar 7fab7fca-...jsonl, a pasta 7fab7fca-... e memory
   ```

3. **Dizer pro Felipe abrir a sessão restaurada no app**, nesta ordem
   (documentado em https://code.claude.com/docs/en/desktop, "Coming from the
   CLI?"):

   - **No próprio app:** na aba Code, começar uma sessão nova nesta mesma
     pasta e digitar `/resume`. Na lista, escolher a sessão longa
     ("Explosão volumétrica…", id `7fab7fca-…`).
   - **Se ela não aparecer na lista:** no PowerShell, na pasta do projeto,
     rodar o comando abaixo. Ele abre a sessão direto no app de desktop.

     ```powershell
     claude --desktop --resume 7fab7fca-b804-45c4-8302-972a750d5b27
     ```

   - **Último recurso:** `claude --resume 7fab7fca-b804-45c4-8302-972a750d5b27`
     no terminal e, já dentro dela, digitar `/desktop` pra levar pro app.

   Trazer uma sessão de OUTRA máquina não está na documentação, mas o arquivo
   é uma transcrição comum na pasta certa. Se nada disso a encontrar,
   confira se o `.jsonl` está na pasta `$dest` do passo 1.

## Depois de retomar (pra mim, o Claude restaurado)

- Os caminhos do histórico são do Mac (`/Users/felipe/explosion`). Agora o
  projeto está no Windows: usar os caminhos novos, e `python` em vez de
  `python3`.
- Não continuar esta mesma sessão no Mac (as duas divergiriam).
- Próximo passo combinado: `MIGRACAO.md`, fase 0. Primeiro descobrir a
  versão da Unreal instalada; depois montar o laço de captura automática pra
  eu ver os quadros.
- As regras de trabalho continuam as do `CLAUDE.md`. Commit só com o
  "aprovado" dele.
