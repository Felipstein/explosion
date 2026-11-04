// Roda o measure_menu.js no mockup aprovado pelo cap.mjs (o atalho do node quebra o & da URL e as linhas do script)
import { readFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
const expr = readFileSync('C:/Users/luisf/explosion/Game/Plugins/RTSShell/Tools/measure_menu.js', 'utf8');
process.argv = [process.argv[0], 'cap.mjs', 'eval', 'file:///C:/Users/luisf/explosion/docs/concept/ui/menu/menu.html?dev=1&still=1&mute=1', expr];
await import(pathToFileURL('C:/Users/luisf/explosion/captures/concept/ui/shell/tools/cap.mjs').href);
