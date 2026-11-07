# (06/11) A mira dos poderes PROJETADA NO CHÃO (RTSPowerPreview; o estudo em docs/poderes/MIRA.md). A v1 (05/11) era desenhada no
# Canvas do HUD: linhas finas por cima de tudo, passando por cima dos tanques e dos prédios, sem a luz do chão. O Felipe, 06/11: "os
# cursores dos poderes parece que é um png em cima do chão". Os RTS grandes fazem a área do poder como marca projetada no chão (o
# "splat" do StarCraft II, do tamanho de verdade do poder; o decalque do 0 A.D. projetado de cima pra baixo no relevo; o RadiusDecal do
# ZH, só no terreno), e o jeito de hoje é desenhar no shader, pela profundidade da cena, não por cima da tela.
# Aqui: um pós-processo ANTES do TSR (o encanamento do clarão da seleção, M_SelectFlash, mas antes do anti-serrilhado) que reconstrói o ponto do mundo de
# cada pixel pela profundidade e desenha as 6 formas por distância (SDF), no chão de verdade:
#   - colada no relevo, nas pedras e nos destroços (é o próprio chão da cena);
#   - por BAIXO das unidades e dos prédios vivos: eles marcam o bit 128 do stencil (a máscara da poeira, ARTSUnitActor::ApplyStencil),
#     e ali a mira não pinta; a unidade fica por cima da marca, como em pé nela;
#   - a área é LUZ no chão, não tinta: a cor do pixel vira o matiz do tipo com a mesma luminância (a areia continua areia, a sombra
#     continua sombra), mais forte na borda (o degradê de um facho), e a linha clara é a luz do chão × 2,4 (lê de dia e na sombra);
#   - a face em pé (a parede, o flanco da pedra) e o que está alto (a copa da palmeira, o telhado) não recebem a área; a linha sobe a
#     pedra mais fraca (o plano do chão vem do C++ pelo relevo em volta do centro);
#   - a linha tem a largura fixa em pixels (pela derivada do ponto do mundo, presa à escala do centro: na quina de uma pedra a derivada
#     explode e a linha engordava) e borda suave (sem serrilhado);
#   - viva: o golpe com o pulso FECHANDO no alvo, o apoio com a onda ABRINDO, os olhos com a varredura do radar, o espalhamento girando,
#     as setas da faixa correndo no sentido do voo, a queda das tropas com o anel fechando em cada ponto, o alvo único com a trava
#     fechando, a órbita girando. O inválido: cinza, tracejado e hachurado, com o motivo no mouse (o Canvas ainda escreve o texto).
# Os números da forma vêm do C++ a cada quadro (os nomes das entradas abaixo; RTSPowerPreview.cpp, Upload).
# Rodar (sem a placa): UnrealEditor-Cmd RTSGame.uproject -run=pythonscript -script=Tools/make_power_preview.py -nullrhi
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER, NAME = '/Game/UI', 'M_PowerPreview'
PATH = f'{FOLDER}/{NAME}'

# as entradas de 4 números (o C++ preenche; a ordem é a do RTSPowerPreview.cpp):
#   Pc  o centro (x, y em cm), o chão no centro (z) e a forma (0 círculo, 1 faixa, 2 traçado, 3 queda, 4 alvo, 5 órbita)
#   Pk  a cor do tipo (linear) e o estado (1 vale, 0 inválido, -1 desligada)
#   Pa  o raio, o espalhamento, o achatado (a elipse) e o relógio (s)
#   Pd  a direção (x, y), o comprimento e a largura da faixa
#   Pe  as fileiras, o comprimento do traçado, quantos pontos (traçado ou queda) e quantos afetados
#   Pf  cm por pixel no centro (no Canvas), a altura do Canvas (px), até onde a forma vai (cm) e o tipo (0 golpe, 1 apoio, 2 tropas, 3 olhos)
#   Pg  a inclinação do chão em volta (dz/dx, dz/dy)
#   Tg  o alvo único (x, y relativos ao centro, o raio da trava, 1 se há alvo)
#   Q0..Q11  o traçado: 24 pontos, 2 por entrada (relativos ao centro)
#   D0..D4   a queda: 10 pontos, 2 por entrada
#   U0..U15  quem o poder pega: (x, y relativos, o raio, 1)
VECS = ['Pc', 'Pk', 'Pa', 'Pd', 'Pe', 'Pf', 'Pg', 'Tg'] + [f'Q{k}' for k in range(12)] + [f'D{k}' for k in range(5)] + [f'U{k}' for k in range(16)]

CODE = r"""
float3 S = Scene.rgb;
if (Pk.a < -0.5) { return S; }
// os vivos (as unidades e os prédios: o bit 128, a máscara da poeira) ficam por cima: a mira mora no chão
if (round(ST.r) >= 127.5) { return S; }
float2 p = Rel.xy;
if (dot(p, p) > Pf.z * Pf.z) { return S; }
const float3 LUMA = float3(0.2126, 0.7152, 0.0722);
const float PI2 = 6.2831853;
const float PI = 3.1415927;
float T = Pa.w;
float ui = max(VS.y / 900.0, 0.5);
// cm por pixel deste pós (o C++ mede no Canvas): a largura das linhas e o tamanho dos traços
float ref = max(Pf.x * Pf.y / max(VS.y, 1.0), 0.01);
float fw = clamp(length(fwidth(p)) * 0.7071, ref * 0.4, ref * 2.5);
float px = ref * ui;
// o chão de verdade: a face em pé e o que está alto não recebem a área; a linha sobe a pedra mais fraca
float h = Rel.z - (Pg.x * p.x + Pg.y * p.y);
float lowK = 1.0 - saturate((h - 400.0) / 350.0);
float upK = saturate((WN.z - 0.3) / 0.45);
float wFill = upK * lowK;
float wLine = max(upK, 0.4) * lowK;
bool valid = Pk.a > 0.5;
int shape = (int)round(Pc.w);
int kind = (int)round(Pf.w);
float breathe = 0.88 + 0.12 * sin(T * PI2 / 1.6);

float fill = 0.0;   // a área (luz no chão)
float ink = 0.0;    // o contorno escuro
float lin = 0.0;    // a linha clara
float beam = 0.0;   // a luz que passa (os pulsos, o radar)
float grayK = valid ? 0.0 : 1.0;

// a linha de meia largura hw (px a 900 de altura) na distância d (cm), com a borda suave
#define AA(d, hw) saturate((hw) * ui - abs(d) / fw + 0.5)
#define STROKE(d, hw, a) { ink = max(ink, AA(d, (hw) + 1.3) * (a)); lin = max(lin, AA(d, hw) * (a)); }
// o tracejado: s anda na linha (cm), period (cm), duty a parte acesa; as pontas suaves
// a faixa de cor por dentro da linha: 12 px na tela, pela distância com sinal (sd < 0 dentro), forte junto da linha
#define RIM(sd) pow(saturate(1.0 + (sd) / (12.0 * px)), 2.0)
#define DASH(s, period, duty) saturate(min(frac((s) / (period)) * (period), (duty) * (period) - frac((s) / (period)) * (period)) / fw + 0.5)

if (shape == 0 || shape == 5)
{
	// o círculo (e a elipse do Naval III, achatada na direção Pd) e a órbita
	float2 u = normalize(Pd.xy + float2(1e-4, 0.0));
	float2 q = float2(dot(p, u), dot(p, float2(-u.y, u.x)) / max(Pa.z, 0.05));
	float r = length(q);
	float R = max(Pa.x, 1.0);
	float a = atan2(q.y, q.x);
	float dR = r - R;
	float inside = saturate(0.5 - dR / fw);
	float rim = RIM(dR);
	float veil = shape == 5 ? 0.5 : 1.0;
	fill = max(fill, inside * (0.035 + 0.75 * rim) * breathe * veil);
	if (shape == 5)
	{
		// a órbita: o anel tracejado girando no sentido do voo
		float n = max(12.0, round(PI2 * R / (21.0 * px)));
		STROKE(dR, 1.3, DASH(a * R + (valid ? T * 14.0 * px : 0.0), PI2 * R / n, valid ? 0.66 : 0.45));
	}
	else if (valid)
	{
		STROKE(dR, 1.3, 1.0);
		if (Pa.y > 0.0)
		{
			// o espalhamento: até onde o estilhaço chega, tracejado girando devagar
			float n = max(16.0, round(PI2 * Pa.y / (17.0 * px)));
			STROKE(r - Pa.y, 0.8, 0.8 * DASH((a + T * 0.25) * Pa.y, PI2 * Pa.y / n, 0.53));
		}
		if (kind == 0)
		{
			// o golpe: os 4 dentes girando por fora e o pulso fechando no alvo
			float dt = (frac(a / (PI / 12.0) + 0.5) - 0.5) * (PI / 12.0);
			STROKE(r * sin(dt), 0.55, 0.75 * saturate((R - 2.5 * px - r) / fw + 0.5) * saturate((r - (R - 9.0 * px)) / fw + 0.5));
			float Rt = R + max(150.0, R * 0.08);
			float da = (frac((a - T * 0.35) / (PI * 0.5) + 0.5) - 0.5) * (PI * 0.5);
			STROKE(r - Rt, 1.8, saturate((0.12 - abs(da)) * r / fw + 0.5));
			float ph = frac(T / 1.4);
			beam = max(beam, AA(r - R * (1.0 - 0.85 * ph), 1.0) * sin(ph * PI) * 0.7);
		}
		else if (kind == 1)
		{
			// o apoio: a onda abrindo do centro
			float ph = frac(T / 1.8);
			beam = max(beam, AA(r - R * (0.1 + 0.9 * ph), 1.1) * sin(ph * PI) * 0.7);
		}
		else if (kind == 3)
		{
			// os olhos: a varredura do radar (o rastro atrás do braço que gira)
			float back = frac((T * 1.3 - a) / PI2);
			beam = max(beam, inside * exp(-back * 14.0) * 0.2);
			float dl = (frac((a - T * 1.3) / PI2 + 0.5) - 0.5) * PI2;
			STROKE(r * sin(dl), 0.8, inside * step(abs(dl), 1.0));
		}
	}
	else
	{
		float n = max(12.0, round(PI2 * R / (11.0 * px)));
		STROKE(dR, 1.3, DASH(a * R, PI2 * R / n, 0.55));
	}
}
else if (shape == 1)
{
	// a faixa com direção: do início (o centro) pra frente Pd, com as fileiras lado a lado
	float2 u = normalize(Pd.xy + float2(1e-4, 0.0));
	float2 n = float2(-u.y, u.x);
	float a = dot(p, u);
	float c = dot(p, n);
	float L = max(Pd.z, 1.0);
	float W = max(Pd.w * 0.5, 1.0);
	float2 dq = abs(float2(a - L * 0.5, c)) - float2(L * 0.5, W);
	float sd = length(max(dq, 0.0)) + min(max(dq.x, dq.y), 0.0);
	float inside = saturate(0.5 - sd / fw);
	float rim = RIM(sd);
	fill = max(fill, inside * (0.04 + 0.6 * rim) * breathe);
	if (valid)
	{
		STROKE(sd, 1.3, 1.0);
		int rows = max(1, (int)round(Pe.x));
		float rw = 2.0 * W / rows;
		[unroll] for (int k = 1; k < 8; ++k)
		{
			if (k < rows)
			{
				STROKE(c - (-W + rw * k), 0.6, 0.7 * inside * DASH(a, 12.0 * px, 0.5));
			}
		}
		// cada fileira: a fileira de impactos no eixo e as setas correndo no sentido do voo (4 na faixa, 9 m/s)
		float cc = c - ((floor((c + W) / rw) + 0.5) * rw - W);
		float step12 = L / 12.0;
		float along = (frac(a / step12 + 0.5) - 0.5) * step12;
		float dotOn = inside * step(step12 * 0.5, a) * step(a, L - step12 * 0.5);
		STROKE(length(float2(along, cc)), 1.0, dotOn);
		float period = L / 4.0;
		float x = frac((a - T * 900.0) / period) * period - period * 0.5;
		float Hc = clamp(rw * 0.3, 6.0 * px, 22.0 * px);
		float tip = a - x;
		float fade = saturate(min(tip, L - tip) / 600.0);
		float arm = saturate((Hc - abs(cc)) / fw + 0.5) * saturate((x + Hc) / fw + 0.5) * saturate(-x / fw + 1.0);
		STROKE((x + abs(cc)) * 0.7071, 1.0, inside * fade * arm);
	}
	else
	{
		STROKE(sd, 1.3, DASH(a + c, 11.0 * px, 0.55));
	}
}
else if (shape == 2)
{
	// o traçado livre (a Fortified Line): a faixa da muralha pelo caminho; o que passa do comprimento do poder, em cinza
	float4 QA[12] = { Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, Q10, Q11 };
	int cnt = (int)round(Pe.z);
	float best = 1e9;
	float sAt = 0.0;
	float acc = 0.0;
	// (06/11) desenrolado: com o índice variável a lista ia pra memória lenta da placa (1,69 ms no ProfileGPU, o vídeo mira_custo_v1)
	[unroll] for (int k = 0; k < 23; ++k)
	{
		if (k + 1 >= cnt) { continue; }
		float4 E0 = QA[k / 2];
		float4 E1 = QA[(k + 1) / 2];
		float2 A = (k % 2 == 0) ? E0.xy : E0.zw;
		float2 B = ((k + 1) % 2 == 0) ? E1.xy : E1.zw;
		float2 AB = B - A;
		float l = length(AB);
		float t = saturate(dot(p - A, AB) / max(l * l, 1.0));
		float d = length(p - (A + AB * t));
		if (d < best) { best = d; sAt = acc + t * l; }
		acc += l;
	}
	float sd = best - 140.0;
	float over = (Pe.y > 0.0 && sAt > Pe.y) ? 1.0 : 0.0;
	grayK = max(grayK, over);
	float inside = saturate(0.5 - sd / fw);
	fill = max(fill, inside * (0.06 + 0.7 * RIM(sd)) * breathe);
	if (valid && over < 0.5)
	{
		STROKE(sd, 1.1, 1.0);
		// os postes a cada 4 m (o quadrado no eixo)
		float alongP = (frac(sAt / 400.0 + 0.5) - 0.5) * 400.0;
		STROKE(max(abs(alongP), best), 1.6, step(sAt, min(acc, Pe.y > 0.0 ? Pe.y : acc)));
	}
	else
	{
		STROKE(sd, 1.0, DASH(sAt, 10.0 * px, 0.5));
	}
}
else if (shape == 3)
{
	// a queda: até onde as tropas (ou os pedaços da carga) se espalham, tracejado, e cada ponto de queda com o anel fechando nele
	float r = length(p);
	float R = max(Pa.x, 1.0);
	float inside = saturate(0.5 - (r - R) / fw);
	float rim = RIM(r - R);
	fill = max(fill, inside * (0.03 + 0.6 * rim) * breathe);
	float n = max(12.0, round(PI2 * R / (16.0 * px)));
	STROKE(r - R, 1.1, DASH(atan2(p.y, p.x) * R, PI2 * R / n, valid ? 0.62 : 0.45));
	if (valid)
	{
		float4 DA[5] = { D0, D1, D2, D3, D4 };
		int nd = (int)round(Pe.z);
		// (06/11, os poderes) o raio do anel de cada ponto: o de sempre (1,6 m) ou o que a mira manda no Pe.y (o Tornado-S: o dano cheio
		// do foguete); o pulso fecha de 3x (o de sempre) ou de 1,4x (o anel grande)
		float Rd = Pe.y > 1.0 ? Pe.y : 160.0;
		float Rp = Rd > 300.0 ? Rd * 1.4 : Rd * 3.0;
		[unroll] for (int k = 0; k < 10; ++k)
		{
			if (k >= nd) { continue; }
			float4 E = DA[k / 2];
			float2 P0 = (k % 2 == 0) ? E.xy : E.zw;
			float dd = length(p - P0);
			float ph = frac(T / 1.2 + k * 0.137);
			STROKE(dd - Rd, 1.0, 1.0);
			STROKE(dd, 1.7, 1.0);
			beam = max(beam, AA(dd - lerp(Rp, Rd, ph), 0.9) * ph * 0.8);
		}
	}
}
else if (shape == 4)
{
	// o alvo único: o anel no chão debaixo dele e a trava (4 riscos fechando por fora); sem alvo, o anel cinza tracejado
	if (Tg.w > 0.5)
	{
		float2 q = p - Tg.xy;
		float r = length(q);
		float Rt = max(Tg.z, 50.0);
		float a = atan2(q.y, q.x);
		fill = max(fill, saturate(0.5 - (r - Rt) / fw) * RIM(r - Rt) * 0.65 * breathe);
		if (valid)
		{
			STROKE(r - Rt, 1.3, 1.0);
			float da = (frac((a - T * 0.5) / (PI * 0.5) + 0.5) - 0.5) * (PI * 0.5);
			float radial = saturate(min(r - Rt * 1.12, Rt * 1.5 - r) / fw + 0.5);
			STROKE(r * sin(da), 1.5, radial);
		}
		else
		{
			float n = max(8.0, round(PI2 * Rt / (11.0 * px)));
			STROKE(r - Rt, 1.2, DASH(a * Rt, PI2 * Rt / n, 0.55));
		}
	}
}

// quem o poder pega agora: o anel fino debaixo de cada um, com o chão aceso por dentro
int na = (int)round(Pe.w);
if (valid && na > 0)
{
	float4 UA[16] = { U0, U1, U2, U3, U4, U5, U6, U7, U8, U9, U10, U11, U12, U13, U14, U15 };
	[unroll] for (int k = 0; k < 16; ++k)
	{
		if (k >= na) { continue; }
		float2 q = p - UA[k].xy;
		float r = length(q);
		float Ru = max(UA[k].z, 50.0);
		STROKE(r - Ru, 1.0, 0.9);
		fill = max(fill, saturate(0.5 - (r - Ru) / fw) * RIM(r - Ru) * 0.7 * breathe);
	}
}

// o inválido: a área hachurada (listras de 1,4 m no chão)
if (!valid)
{
	fill = max(fill, 0.10 * DASH((p.x + p.y) * 0.7071, 140.0, 0.5) * (shape == 2 ? 0.0 : 1.0) * saturate(fill * 30.0));
}

fill *= wFill;
beam *= wFill;
ink *= wLine;
lin *= wLine;
float3 col = lerp(Pk.rgb, float3(0.32, 0.35, 0.38), grayK);
float3 hue = col / max(dot(col, LUMA), 0.04);
float lum = dot(S, LUMA);
float3 C = S;
// a área: no miolo quase nada; na borda, a cor do tipo acesa (a luz do chão × 1,15), a faixa estreita por dentro da linha. A cor da
// areia sai antes (fica cinza na mesma luz) e só então entra a do tipo: o azul por cima do laranja passava por um lilás no meio da
// faixa (a mistura de cores opostas); assim vai de areia a cinza-aço a azul (o vídeo mira_poderes_v4, 06/11)
float kf = saturate(fill * 0.85);
C = lerp(C, float3(lum, lum, lum), saturate(kf * 1.6));
C = lerp(C, hue * max(lum, 0.05) * 1.15, kf) * (1.0 + 0.08 * fill);
// a luz que passa (o pulso, o radar)
C = lerp(C, hue * max(lum, 0.05) * 1.9, saturate(beam));
// o contorno escuro e a linha clara (a luz do chão × 2,4: lê de dia e na sombra)
C *= 1.0 - 0.62 * ink;
C = lerp(C, hue * (max(lum, 0.05) * 2.4 + 0.03), lin);
return C;
"""


def vec_output(m, name, x, y):
    """O parâmetro de 4 números: o pino RGBA quando existe; senão RGB + A juntados."""
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, -1.0 if name == 'Pk' else 0.0))
    return e


def main():
    if eal.does_asset_exist(PATH):
        eal.delete_asset(PATH)
    m = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    # ANTES do TSR (depois da profundidade de campo, na resolução de desenho): o TSR trata a mira como o resto do chão. Antes do
    # bloom (depois do TSR) a profundidade lida é a do quadro com o tremido do TSR (o subpixel muda a cada quadro), e a linha pulava
    # 1 px pra lá e pra cá com a câmera parada (o vídeo mira_poderes_v5, 06/11: "pq ta tremelicando?")
    loc = getattr(unreal.BlendableLocation, 'BL_SCENE_COLOR_AFTER_DOF', None) or getattr(unreal.BlendableLocation, 'BL_BEFORE_TONEMAPPING')
    m.set_editor_property('blendable_location', loc)
    ids = unreal.SceneTextureId
    srcs = {}   # nome da entrada do Custom → (expressão, pino)
    for k, (name, tid) in enumerate((('Scene', ids.PPI_POST_PROCESS_INPUT0), ('ST', ids.PPI_CUSTOM_STENCIL), ('WN', ids.PPI_WORLD_NORMAL))):
        e = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -1400, -600 + k * 110)
        e.set_editor_property('scene_texture_id', tid)
        srcs[name] = (e, '')
    vs = mel.create_material_expression(m, unreal.MaterialExpressionViewSize, -1400, -260)
    srcs['VS'] = (vs, '')
    vecs = {}
    for k, name in enumerate(VECS):
        vecs[name] = vec_output(m, name, -1400, -140 + k * 70)
    # o ponto do mundo deste pixel (pela profundidade) menos o centro: o relativo (precisão boa num mapa de 1 km)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1100, -400)
    mask = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -1100, -300)
    for c in ('r', 'g', 'b'):
        mask.set_editor_property(c, True)
    mask.set_editor_property('a', False)
    sub = mel.create_material_expression(m, unreal.MaterialExpressionSubtract, -900, -350)
    ok = [mel.connect_material_expressions(vecs['Pc'], '', mask, ''),
          mel.connect_material_expressions(wp, '', sub, 'A'),
          mel.connect_material_expressions(mask, '', sub, 'B')]
    srcs['Rel'] = (sub, '')
    # os de 4 números: RGB + A juntados (o pino '' da VectorParameter é o RGB)
    for k, name in enumerate(VECS):
        ap = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -1150, -140 + k * 70)
        ok.append(mel.connect_material_expressions(vecs[name], '', ap, 'A'))
        ok.append(mel.connect_material_expressions(vecs[name], 'A', ap, 'B'))
        srcs[name] = (ap, '')
    cu = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -500, 0)
    cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cu.set_editor_property('description', 'PowerPreview')
    ins = []
    for n in srcs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    cu.set_editor_property('inputs', ins)
    cu.set_editor_property('code', CODE)
    for n, (e, pin) in srcs.items():
        ok.append(mel.connect_material_expressions(e, pin, cu, n))
    ok.append(mel.connect_material_property(cu, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR))
    if not all(ok):
        raise RuntimeError('M_PowerPreview: ligações falharam %s' % ok)
    errs = mel.recompile_material(m)
    if errs:
        raise RuntimeError('M_PowerPreview: erros %s' % errs)
    eal.save_asset(PATH)
    unreal.log_warning(f'M_PowerPreview pronto: {PATH}')


main()
