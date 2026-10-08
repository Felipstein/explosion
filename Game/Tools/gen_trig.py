"""Gera as tabelas de seno e arco-tangente da simulação (SimCore/Private/TrigTables.inl).

A simulação não pode chamar sin/atan da biblioteca C: o resultado pode mudar no
último bit entre CPUs e compiladores (a CRT do x64 escolhe a versão com ou sem
FMA3 conforme a CPU), e isso vira dessincronia no multiplayer. A tabela é
calculada uma vez, aqui, e entra no código como constante: todo PC usa os
mesmos inteiros.

Formatos (ver Sim/FixedMath.h):
  - ângulo binário: 65536 = 360°;
  - seno: quarto de onda, 1025 amostras de 0° a 90°, em Q16 (65536 = 1,0);
  - arco-tangente: 1025 amostras de atan(t) para t em [0, 1], em ângulo binário
    (8192 = 45°).

Uso: python Game/Tools/gen_trig.py
"""
import math
from pathlib import Path

SIN_N = 1024
ATAN_N = 1024
ONE = 1 << 16
FULL_TURN = 1 << 16

out = Path(__file__).resolve().parents[1] / "Source/SimCore/Private/TrigTables.inl"


def row(values, per_line=12):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append("\t" + ", ".join(str(v) for v in values[i:i + per_line]) + ",")
    return "\n".join(lines)


sin_q16 = [round(math.sin(i / SIN_N * math.pi / 2) * ONE) for i in range(SIN_N + 1)]
atan_bam = [round(math.atan(i / ATAN_N) / (2 * math.pi) * FULL_TURN) for i in range(ATAN_N + 1)]
assert sin_q16[0] == 0 and sin_q16[-1] == ONE
assert atan_bam[0] == 0 and atan_bam[-1] == FULL_TURN // 8

text = f"""// GERADO por Game/Tools/gen_trig.py. Não editar à mão.
// Seno em quarto de onda (Q16) e arco-tangente em [0, 1] (ângulo binário, 65536 = 360°).

static constexpr int32_t SinQuarterTable[{SIN_N + 1}] = {{
{row(sin_q16)}
}};

static constexpr int32_t AtanTable[{ATAN_N + 1}] = {{
{row(atan_bam)}
}};
"""
out.write_text(text, encoding="utf-8", newline="\n")
print(f"{out}: seno {len(sin_q16)} e atan {len(atan_bam)} amostras")
