# (06/11/2026, a trilha da partida, docs/TRILHA_DA_PARTIDA.md) Os planos de composição das faixas novas (ElevenLabs Music v2.5): cada faixa
# em trechos de um número inteiro de compassos no andamento dela (o compasso = 240 / BPM s), então a grade das batidas e o começo de cada
# trecho são conhecidos de antemão: o jogo entra e sai da música no tempo forte sem adivinhar. Escreve docs/trilha/planos/<id>.json;
# compose.mjs gera cada um.
#   python Game/Tools/music/plans.py
# As regras de todas (o que faltou nas faixas antigas, pelas medidas e pelo ouvido do Gemini, TRILHA_DA_PARTIDA.md §3):
#  - um motivo curto (o gancho) que aparece cedo e volta a faixa inteira, com os instrumentos da facção (a identidade);
#  - mixagem seca, firme e clara (o defeito de quase todas era o grave/médio-grave embolado);
#  - um fim de verdade (a última nota com o eco natural), nunca corte nem fade longo;
#  - sem letra nem voz feminina (o coro, quando há, é sem palavras).
import json
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'docs', 'trilha', 'planos')
NEG = ['lyrics', 'spoken words', 'female vocals', 'pop', 'EDM drop', 'lo-fi', 'muddy mix']


def bars(bpm, n):
    return int(round(n * 240000 / bpm))


def plan(id, faction, role, bpm, why, styles, sections, seed=1, extra_neg=()):
    """sections: [(nome, compassos, [direções], [estilos do trecho])]; o 1º trecho leva os estilos gerais (6-7)."""
    chunks = []
    for i, (name, n, lines, st) in enumerate(sections):
        text = '\n'.join([f'[{name}]'] + [f'{{{l}}}' for l in lines])
        assert all(len(l) <= 200 for l in text.split('\n')), (id, name)
        ms = bars(bpm, n[0]) + int(n[1] * 1000) if isinstance(n, tuple) else bars(bpm, n)   # (compassos, segundos do eco no fim)
        chunks.append({'text': text, 'duration_ms': ms, 'positive_styles': styles if i == 0 else st,
                       'negative_styles': NEG + list(extra_neg) if i == 0 else ['lyrics', 'muddy mix'] + (['fade out', 'abrupt cut'] if name == 'Ending' else [])})
    d = {'id': id, 'faction': faction, 'role': role, 'bpm': bpm, 'bar_s': round(240 / bpm, 5), 'why': why, 'model_id': 'music_v2_5', 'seed': seed,
         'sections': [{'name': s[0], 'start_s': round(sum(c['duration_ms'] for c in chunks[:i]) / 1000, 3)} for i, s in enumerate(sections)],
         'composition_plan': {'chunks': chunks}}
    json.dump(d, open(os.path.join(OUT, f'{id}.json'), 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    return d


os.makedirs(OUT, exist_ok=True)
ENDING_HIT = 'then one unison hit on the downbeat and a long natural ring-out; a definitive ending, no fade-out'

# --- RÚSSIA: fria, pesada, de Estado; coro masculino sem palavras, metais graves, bigorna, baixo sintético industrial, acordeão (bayan)
# e balalaica de cor. O motivo da facção: 4 notas descendentes em menor (o mesmo pedido da russia_red_tide, a 1ª, gerada antes)
plan('russia_iron_directive', 'russia', 'theme', 112,
     'O tema da Rússia (o começo da partida e o tema da facção): a marcha de Estado, lenta e enorme.',
     ['dark orchestral march', 'deep Russian male choir', 'low brass', 'bayan accordion', 'war drums', 'anvil percussion', '112 bpm, minor key'],
     [('Intro', 8, ['a lone bayan accordion plays the faction motif: a dark 4-note descending minor phrase', 'a deep male choir hums under it, cold wind texture'], []),
      ('Theme', 16, ['a military snare march starts on the downbeat', 'the low brass states the 4-note motif, strong and slow, the choir answers'], ['military snare march', 'low brass melody', 'male choir']),
      ('Build', 8, ['string ostinato and anvil hits on the backbeat, the tension rises bar by bar'], ['string ostinato', 'anvil hits', 'rising tension']),
      ('Anthem', 16, ['the full choir sings the motif fortissimo over brass and war drums, huge and proud'], ['full choir fortissimo', 'brass', 'war drums']),
      ('Ending', (6, 4.0), ['the motif one last time in half-time by choir and brass', ENDING_HIT], ['final statement', 'unison hit', 'natural ring-out'])])

plan('russia_cold_line', 'russia', 'calm', 96,
     'Calma da Rússia (a base, a economia): a tensão fria de quem espera, com o motivo baixinho.',
     ['dark cinematic ambient', 'industrial', 'low synth drone', 'cello ostinato', 'ticking metal percussion', 'bayan accordion', '96 bpm, minor key'],
     [('Intro', 8, ['a low synth drone and a ticking metallic pulse like a clock in a bunker', 'a distant bayan accordion breathes the 4-note motif'], []),
      ('Pulse', 16, ['a slow pulsing synth bass, sparse deep drum hits, a cello ostinato', 'a soft wordless male choir pad'], ['pulsing synth bass', 'sparse deep drums', 'cello ostinato']),
      ('Motif', 16, ['a low balalaika tremolo plays the 4-note motif over the pulse, restrained, cold, never loud'], ['balalaika tremolo melody', 'restrained', 'cold']),
      ('Ending', (6, 4.0), ['the pulse slows, the motif once more on the bayan', 'a last low gong and the drone ring out naturally'], ['slowing', 'natural ending'])])

plan('russia_northern_hammer', 'russia', 'battle', 140,
     'Batalha 2 da Rússia: o peso industrial (a 1ª, a russia_red_tide, ficou mais orquestral).',
     ['industrial metal', 'heavy distorted guitars', 'distorted synth bass', 'deep Russian male choir shouts', 'brass stabs', 'anvil percussion', '140 bpm, minor key'],
     [('Intro', 8, ['drums and a distorted bass riff built on the 4-note descending minor motif', 'anvil hits on every bar'], []),
      ('Main', 16, ['heavy half-time groove: chugging guitars, brass stabs on the motif, short wordless choir shouts'], ['half-time groove', 'chugging guitars', 'brass stabs']),
      ('Drive', 16, ['double-time drums, the guitars play the motif as a riff, the choir chants on the off-beats'], ['double-time drums', 'guitar riff', 'choir chants']),
      ('Break', 8, ['only the distorted bass pulse and ticking metal, then a big tom fill'], ['sparse', 'tension', 'tom fill']),
      ('Climax', 16, ['everything at once at full force: choir, brass and guitars in unison on the motif'], ['maximum intensity', 'unison', 'choir and brass']),
      ('Ending', (4, 4.0), ['the riff stops on a final unison hit', 'the choir and a low gong ring out naturally; no fade-out'], ['final hit', 'natural ring-out'])])

# --- CHINA: a potência moderna; erhu, dizi, guzheng, pipa, tambores dagu, gongos, com pulso eletrônico e orquestra. As antigas da China
# eram fortes e de batalha; faltava a calma (e as duas que o Gemini reprovou, sem instrumento chinês, eram as mais calmas)
plan('china_jade_dawn', 'china', 'calm', 92,
     'Calma da China: o amanhecer sobre a base; a pentatônica clara, o guzheng e a dizi sobre um pulso eletrônico macio.',
     ['modern Chinese cinematic', 'guzheng arpeggios', 'dizi bamboo flute', 'soft electronic pulse', 'warm strings', 'light dagu drums', '92 bpm, pentatonic'],
     [('Intro', 8, ['guzheng arpeggios in a pentatonic mode, a soft electronic pulse fades in'], []),
      ('Theme', 16, ['the dizi flute plays a memorable 5-note pentatonic motif, warm strings underneath, light dagu drum hits'], ['dizi melody', 'warm strings', 'light drums']),
      ('Lift', 16, ['the erhu answers the dizi, the pulse grows a little, a low gong on each phrase'], ['erhu answer', 'gentle growth', 'gong']),
      ('Ending', (6, 4.0), ['the motif once more on the guzheng alone', 'a last soft gong rings out naturally'], ['quiet ending', 'natural ring-out'])])

plan('china_red_lantern', 'china', 'calm', 100,
     'Calma 2 da China: a vigília, mais tensa; a pipa e o erhu sobre um pulso grave, a máquina de guerra se preparando.',
     ['modern Chinese cinematic', 'pipa', 'erhu', 'deep electronic pulse', 'low strings', 'taiko-like dagu drums', '100 bpm, minor pentatonic'],
     [('Intro', 8, ['a deep electronic pulse and a plucked pipa ostinato in minor pentatonic'], []),
      ('Watch', 16, ['the erhu sings a tense, memorable phrase over low strings, soft dagu drums every two bars'], ['erhu lead', 'low strings', 'soft drums']),
      ('Machine', 16, ['metallic percussion and a mechanical rhythm join, the erhu phrase repeats higher, restrained'], ['mechanical rhythm', 'metallic percussion', 'restrained']),
      ('Ending', (6, 4.0), ['the pulse stops, the pipa plays the phrase one last time', 'a gong rings out naturally'], ['quiet ending', 'natural ring-out'])])

# --- GLA: os insurgentes do deserto; oud, ney, darbuka, tambores de moldura, guitarra suja, eletrônico quebrado. Entra no lugar das duas
# que o Gemini reprovou (o metal ocidental e a orquestra genérica)
plan('gla_scorpion_road', 'gla', 'battle', 126,
     'Batalha da GLA: a emboscada; o riff de oud distorcido na escala hijaz, darbuka e breakbeat, o baixo industrial.',
     ['Middle Eastern industrial', 'distorted oud riff', 'darbuka and frame drums', 'breakbeat', 'industrial bass', 'ney flute', '126 bpm, hijaz scale'],
     [('Intro', 8, ['a darbuka pattern and a ney flute call in the hijaz scale, a dusty desert wind'], []),
      ('Riff', 16, ['a breakbeat kicks in with a distorted oud riff, short and memorable, in the hijaz scale, industrial bass'], ['distorted oud riff', 'breakbeat', 'industrial bass']),
      ('Raid', 16, ['dirty electric guitars double the oud riff, frame drums and claps, gritty and fast'], ['dirty guitars', 'frame drums', 'gritty']),
      ('Break', 8, ['only the darbuka and a filtered bass, a radio static texture, then a drum fill'], ['sparse', 'tension', 'drum fill']),
      ('Climax', 16, ['everything together at full force, the ney screams the riff above the band'], ['maximum intensity', 'ney lead', 'full band']),
      ('Ending', (4, 4.0), ['the riff ends on one unison hit', 'the hit rings out naturally; no fade-out'], ['final hit', 'natural ring-out'])])

# --- USA: alta tecnologia e heroísmo; guitarra, orquestra e eletrônico. Tem batalha de sobra; falta calma (a única, Tactical Alignment,
# acaba cortada)
plan('usa_overwatch', 'usa', 'calm', 100,
     'Calma da USA: a vigilância por satélite; pulso de sintetizador, guitarra limpa com eco, cordas graves e caixa militar de leve.',
     ['modern military cinematic', 'synth pulse', 'clean electric guitar with delay', 'low strings', 'light military snare', 'soft brass', '100 bpm, minor key'],
     [('Intro', 8, ['a precise synth pulse like a radar sweep, a clean electric guitar with delay plays a short heroic motif'], []),
      ('Theme', 16, ['low strings and a light military snare join, soft French horns state the motif'], ['low strings', 'light snare', 'soft horns']),
      ('Lift', 16, ['the motif rises one step, the pulse gets busier, still calm and controlled'], ['controlled growth', 'busier pulse']),
      ('Ending', (6, 4.0), ['the guitar alone plays the motif', 'a soft low brass chord rings out naturally'], ['quiet ending', 'natural ring-out'])])

# --- os fins da partida (o ScoreScreenMusic do ZH): vitória e derrota de cada facção, curtos, com o fim de verdade
STING = {
    'usa': (['heroic orchestral rock', 'brass fanfare', 'electric guitar', 'military snare', 'strings'], 'brass and guitars'),
    'china': (['modern Chinese cinematic', 'erhu', 'dizi', 'dagu drums', 'gongs', 'strings'], 'erhu, dizi and gongs'),
    'gla': (['Middle Eastern cinematic', 'oud', 'ney flute', 'darbuka', 'frame drums', 'low strings'], 'oud, ney and darbuka'),
    'russia': (['dark orchestral', 'deep Russian male choir', 'low brass', 'war drums', 'bayan accordion'], 'choir and low brass'),
}
for fac, (st, inst) in STING.items():
    plan(f'{fac}_victory', fac, 'victory', 100, f'Vitória da {fac.upper()}: a fanfarra curta no fim da partida.',
         st + ['triumphant', 'major key'],
         [('Fanfare', 4, [f'a triumphant short fanfare by {inst}, the faction motif in a major key, proud'], []),
          ('Ending', (2, 4.0), ['a final big chord', 'it rings out naturally; no fade-out'], ['final chord', 'natural ring-out'])])
    plan(f'{fac}_defeat', fac, 'defeat', 72, f'Derrota da {fac.upper()}: o lamento curto no fim da partida.',
         st + ['somber', 'minor key'],
         [('Lament', 3, [f'a slow somber lament by {inst}, the faction motif in a minor key, descending, heavy'], []),
          ('Ending', (1, 4.0), ['a last low chord', 'it rings out naturally; no fade-out'], ['final low chord', 'natural ring-out'])])

# --- (06/11) a 2ª volta, pelo ouvido do Gemini (TRILHA_DA_PARTIDA.md §3): as duas calmas da China saíram orquestra ocidental genérica
# (os instrumentos chineses não vieram: "cinematic" e "warm strings" puxaram a orquestra) e a vitória da China também; a derrota da
# Rússia saiu um coro a cappella sem nada; a da USA, rock. A v2 pede os instrumentos pelo nome nos estilos gerais e proíbe a orquestra
# ocidental, o coro e os metais onde não cabem
CN_NEG = ['western symphony orchestra', 'choir', 'brass section', 'epic trailer']
plan('china_jade_dawn_v2', 'china', 'calm', 92,
     'Calma da China, v2: a v1 saiu orquestra genérica. Os instrumentos chineses pelo nome, sem orquestra ocidental.',
     ['traditional Chinese instrumental', 'guzheng', 'dizi bamboo flute', 'erhu', 'soft electronic pulse', 'Chinese drums and gongs', '92 bpm, pentatonic'],
     [('Intro', 8, ['solo guzheng arpeggios in a pentatonic mode, a soft electronic pulse fades in'], []),
      ('Theme', 16, ['the dizi flute plays a memorable 5-note pentatonic motif over the guzheng, light Chinese drum hits'], ['dizi melody', 'guzheng', 'light Chinese drums']),
      ('Lift', 16, ['the erhu answers the dizi, the pulse grows a little, a low gong on each phrase'], ['erhu answer', 'gentle growth', 'gong']),
      ('Ending', (6, 4.0), ['the motif once more on the guzheng alone', 'a last soft gong rings out naturally'], ['quiet ending', 'natural ring-out'])],
     extra_neg=CN_NEG)
plan('china_red_lantern_v2', 'china', 'calm', 100,
     'Calma 2 da China, v2: a v1 saiu épico genérico. A pipa e o erhu pelo nome sobre um pulso eletrônico grave.',
     ['traditional Chinese instrumental', 'pipa', 'erhu', 'guzheng', 'deep electronic pulse', 'Chinese drums', '100 bpm, minor pentatonic'],
     [('Intro', 8, ['a deep electronic pulse and a plucked pipa ostinato in minor pentatonic'], []),
      ('Watch', 16, ['the erhu sings a tense, memorable phrase over the pipa, soft Chinese drums every two bars'], ['erhu lead', 'pipa ostinato', 'soft Chinese drums']),
      ('Machine', 16, ['metallic percussion and a mechanical rhythm join, the erhu phrase repeats higher, restrained'], ['mechanical rhythm', 'metallic percussion', 'restrained']),
      ('Ending', (6, 4.0), ['the pulse stops, the pipa plays the phrase one last time', 'a gong rings out naturally'], ['quiet ending', 'natural ring-out'])],
     extra_neg=CN_NEG)
plan('china_victory_v2', 'china', 'victory', 100, 'Vitória da China, v2: a v1 saiu fanfarra ocidental. A suona, os tambores e os gongos.',
     ['traditional Chinese instrumental', 'suona', 'dagu drums', 'gongs and cymbals', 'erhu', 'dizi', 'triumphant, major pentatonic'],
     [('Fanfare', 4, ['a triumphant short fanfare: the suona plays the faction motif over dagu drums, gongs and cymbals'], []),
      ('Ending', (2, 4.0), ['a final big gong and drum hit', 'it rings out naturally; no fade-out'], ['final hit', 'natural ring-out'])],
     extra_neg=CN_NEG)
plan('russia_defeat_v2', 'russia', 'defeat', 72, 'Derrota da Rússia, v2: a v1 saiu um coro a cappella solto. A marcha fúnebre com metais e tímpanos.',
     ['dark orchestral', 'low brass', 'deep male choir humming', 'slow timpani', 'bayan accordion', 'somber funeral march', 'minor key'],
     [('Lament', 3, ['a slow funeral march: timpani, low brass and a humming male choir play the faction motif, descending in minor'], []),
      ('Ending', (1, 4.0), ['a last low chord on brass and choir', 'it rings out naturally; no fade-out'], ['final low chord', 'natural ring-out'])],
     extra_neg=['a cappella'])
plan('usa_defeat_v2', 'usa', 'defeat', 72, 'Derrota da USA, v2: a v1 saiu rock. O trompete solo e as cordas graves.',
     ['cinematic orchestral', 'solo trumpet', 'low strings', 'muffled military snare', 'french horns', 'somber', 'minor key'],
     [('Lament', 3, ['a solo trumpet plays the faction motif slowly in minor over low strings and a muffled military snare'], []),
      ('Ending', (1, 4.0), ['a last low chord on horns and strings', 'it rings out naturally; no fade-out'], ['final low chord', 'natural ring-out'])],
     extra_neg=['rock', 'electric guitar', 'drum kit'])

# --- (06/11) a Rússia com mais duas: a calma tinha só uma (a russia_cold_line) e a batalha 2 (a russia_northern_hammer) saiu metal genérico,
# sem o coro e os metais da facção (o Gemini: "fit 6, sounds like a generic metal instrumental")
plan('russia_white_silence', 'russia', 'calm', 88,
     'Calma 2 da Rússia: a neve e o aço; o acordeão e as cordas graves, sinos ao longe, o coro bem baixo; o motivo de 4 notas.',
     ['dark Russian cinematic', 'bayan accordion', 'low strings', 'distant church bells', 'deep male choir pad', 'slow war drum', '88 bpm, minor key'],
     [('Intro', 8, ['distant bells in a cold wide space, a bayan accordion breathes the dark 4-note descending motif'], []),
      ('Snow', 16, ['low strings hold long minor chords, a slow war drum every two bars, the motif on a solo cello'], ['low strings', 'slow war drum', 'solo cello']),
      ('Steel', 16, ['a deep male choir pad hums the motif under the strings, a metallic pulse begins, restrained and cold'], ['male choir pad', 'metallic pulse', 'restrained']),
      ('Ending', (6, 4.0), ['the bayan plays the motif one last time alone', 'a last bell and the strings ring out naturally'], ['quiet ending', 'natural ring-out'])],
     extra_neg=['rock', 'electric guitar'])
plan('russia_steel_avalanche', 'russia', 'battle', 136,
     'Batalha 3 da Rússia: a avalanche de blindados; o coro masculino gritando o motivo sobre metais e o peso industrial.',
     ['epic dark orchestral', 'deep Russian male choir', 'heavy brass', 'industrial percussion', 'distorted synth bass', 'war drums', '136 bpm, minor key'],
     [('Intro', 8, ['war drums and a distorted synth bass pulse, the low brass hints the dark 4-note descending motif'], []),
      ('Charge', 16, ['the full choir shouts the motif in unison over heavy brass and pounding war drums'], ['choir unison', 'heavy brass', 'pounding drums']),
      ('Assault', 16, ['double-time industrial percussion, brass stabs on the motif, the choir answers with short wordless shouts'], ['double-time', 'brass stabs', 'choir shouts']),
      ('Break', 8, ['only the synth bass and ticking metal percussion, then a huge drum fill'], ['sparse', 'tension', 'drum fill']),
      ('Climax', 16, ['everything at maximum: choir fortissimo, brass and drums in unison on the motif'], ['maximum intensity', 'choir fortissimo', 'unison']),
      ('Ending', (4, 4.0), ['the motif ends on one huge unison hit', 'the choir and a low gong ring out naturally; no fade-out'], ['final hit', 'natural ring-out'])],
     extra_neg=['electric guitar'])

# --- (06/11, a 3ª volta) A USA do zero, menos o tema. Ele, depois de ouvir no jogo: "a da USA ficou muito repetida algumas, muito ruim,
# péssimas, me enjoa de ouvir; a primeira é top [The Last Advance], mas as outras sei lá". A medida (repetition.py) concordou: as faixas de
# 120 s de 05/10 eram um loop só (Forward Secure Command 94% de cópia exata, a maior volta de 112 s; Tactical Superiority 87%; Tactical
# Alignment 84%), e o tema dele tem 43%. As regras das novas, contra o enjoo:
#  - trechos de 8 compassos, cada um com uma textura própria (nunca o mesmo trecho duas vezes);
#  - o motivo volta sempre transformado (outro instrumento, outra altura, outra harmonia), como numa trilha de verdade;
#  - uma subida de tom antes do clímax; a bateria muda de desenho de trecho pra trecho;
#  - "repetitive loop" e "same section repeated" proibidos;
#  - a mesma família do tema que ele aprovou (a marcha heroica de metais, caixa militar e cordas, com guitarra), pelo áudio dele de
#    referência na ElevenLabs (o upload.mjs: song_id em docs/trilha/receitas/usa_last_advance.upload.json, o trecho do tema 15–45 s).
USA_REF = {'song_id': 'LqIsaMflvuqkc4a0dft4_upload', 'range': {'start_ms': 15000, 'end_ms': 45000}}
USA_NEG = ['repetitive loop', 'same section repeated', 'choir', 'synthwave', 'EDM', 'lo-fi']


def plan_ref(id, faction, role, bpm, why, styles, sections, ref=None, strength='medium', ref_chunks=(0, 1), extra_neg=()):
    d = plan(id, faction, role, bpm, why, styles, sections, extra_neg=extra_neg)
    if ref:
        for i in ref_chunks:
            d['composition_plan']['chunks'][i]['conditioning_ref'] = ref
            d['composition_plan']['chunks'][i]['condition_strength'] = strength
        d['reference'] = {'song_id': ref['song_id'], 'range': ref['range'], 'strength': strength, 'chunks': list(ref_chunks), 'source': 'usa_last_advance (The_Last_Advance.mp3, o tema aprovado)'}
        json.dump(d, open(os.path.join(OUT, f'{id}.json'), 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    return d


USA_VANGUARD = (
    ['heroic orchestral rock', 'military brass fanfare', 'marching snare and rock drums', 'electric guitar power chords', 'strings', 'timpani', '128 bpm, major key'],
    [('Intro', 4, ['a snare roll and a timpani hit; the brass announces the first four notes of a heroic march motif', 'electric guitar power chords swell under it'], []),
     ('Theme', 8, ['the full heroic brass melody over a driving rock beat and a marching snare', 'the guitars chug in eighth notes'], ['heroic brass melody', 'driving rock beat', 'marching snare']),
     ('Answer', 8, ['the strings take the melody one octave higher, the brass answers with short fanfare stabs', 'the drums switch to a half-time groove'], ['strings melody', 'brass stabs', 'half-time drums']),
     ('Development', 8, ['the motif broken into fragments passed between horns, trumpets and guitar over new chords', 'a rising sequence, tom-heavy drums'], ['motif fragments', 'new harmony', 'tom drums']),
     ('Breakdown', 8, ['the band drops out: a low string ostinato and timpani pulses', 'a solo French horn plays the motif slowly in minor, tense'], ['sparse', 'solo horn', 'tension']),
     ('Build', 4, ['a snare roll crescendo and rising strings, a cymbal swell into a key change'], ['crescendo', 'key change']),
     ('Climax', 8, ['everything returns one whole step higher: brass in unison on the motif, guitars and drums at full power'], ['full power', 'higher key', 'brass unison']),
     ('Climax 2', 8, ['the trumpets harmonize the motif in thirds, a new countermelody in the strings, double-time snare'], ['harmonized trumpets', 'countermelody', 'double-time snare']),
     ('Ending', (4, 4.0), ['a final heroic statement in half-time', 'one big unison hit on the downbeat and a natural ring-out; no fade-out'], ['final statement', 'unison hit', 'natural ring-out'])])
plan_ref('usa_vanguard_ref', 'usa', 'battle', 128, 'Batalha 1 da USA, a 3ª volta, A/B: com o tema dele de referência (condition medium nos 2 primeiros trechos).',
         *USA_VANGUARD, ref=USA_REF, extra_neg=USA_NEG)
plan_ref('usa_vanguard_free', 'usa', 'battle', 128, 'Batalha 1 da USA, a 3ª volta, A/B: o mesmo plano sem a referência.',
         *USA_VANGUARD, ref=None, extra_neg=USA_NEG)

# (06/11, a 3ª volta da USA, continuação) A A/B da batalha 1: com a referência saiu um pouco melhor no ouvido (produção 6 × 5, as duas
# com ~50% de cópia, no nível do tema dele) e fica a com referência. Mais uma calma e uma batalha, as duas pelo tema dele de referência.
# Fora da USA (as que ele chamou de "muito repetidas, péssimas": os loops de 120 s de 05/10, 66 a 95% de cópia): Command at Dawn, Forward
# Secure Command, Operation Dawn Watch, Tactical Alignment e Tactical Superiority. Ficam o tema e as menos repetidas.
plan_ref('usa_dawn_patrol', 'usa', 'calm', 96,
         'Calma nova da USA: a madrugada na base; o motivo heroico do tema dele, mas de longe e devagar (trompa solo, cordas, piano), cada trecho com outra cor.',
         ['cinematic military orchestral', 'solo French horn', 'warm strings', 'piano', 'soft snare brushes', 'distant trumpet', '96 bpm, major key, calm'],
         [('Intro', 4, ['a quiet piano and a sustained string chord; far away, a solo trumpet plays the first notes of a heroic motif'], []),
          ('Horn', 8, ['a solo French horn sings the full heroic motif slowly over warm strings, soft snare brushes keep time'], ['solo horn melody', 'warm strings', 'soft brushes']),
          ('Piano', 8, ['the piano takes the motif in a new harmony, the strings answer with a gentle countermelody, no drums'], ['piano melody', 'countermelody', 'no drums']),
          ('Rise', 8, ['low brass and timpani join softly, the motif rises one step, hopeful, still calm'], ['low brass', 'soft timpani', 'hopeful rise']),
          ('Strings', 8, ['the full strings play the motif warmly in a higher register, a snare march pattern very soft in the distance'], ['full strings', 'warm', 'distant snare']),
          ('Ending', (4, 4.0), ['the solo horn plays the motif one last time', 'a soft final chord rings out naturally; no fade-out'], ['quiet ending', 'natural ring-out'])],
         ref=USA_REF, strength='low', extra_neg=USA_NEG + ['rock drums', 'electric guitar'])
plan_ref('usa_steel_rain', 'usa', 'battle', 140,
         'Batalha nova 2 da USA: o ataque blindado; mais rápida e pesada que a vanguard, a guitarra e os metais sobre a caixa militar dobrada, pelo tema dele.',
         ['heroic orchestral rock', 'distorted electric guitars', 'brass section', 'double-time military snare', 'rock drums', 'strings', '140 bpm, minor to major'],
         [('Intro', 4, ['a fast military snare cadence and a guitar riff hinting the heroic motif in minor'], []),
          ('Charge', 8, ['the band hits: chugging guitars and brass stabs on the motif, double-time snare, aggressive'], ['chugging guitars', 'brass stabs', 'double-time snare']),
          ('Theme', 8, ['the brass plays the heroic motif in major over the riff, cymbals on every bar, triumphant'], ['brass melody', 'major key', 'triumphant']),
          ('Counter', 8, ['the strings play fast runs, the guitars take a new riff, the drums switch to a half-time groove'], ['string runs', 'new guitar riff', 'half-time']),
          ('Breakdown', 8, ['only timpani, low strings and a muted guitar chug, the horns hint the motif, tension'], ['sparse', 'tension', 'timpani']),
          ('Build', 4, ['a snare roll and rising brass into a key change'], ['crescendo', 'key change']),
          ('Climax', 8, ['one step higher: guitars, brass and drums in unison on the motif at full power'], ['full power', 'unison', 'higher key']),
          ('Ending', (4, 4.0), ['the riff ends on one big unison hit', 'a natural ring-out; no fade-out'], ['final hit', 'natural ring-out'])],
         ref={'song_id': USA_REF['song_id'], 'range': {'start_ms': 100000, 'end_ms': 130000}}, strength='medium', extra_neg=USA_NEG)

# --- (06/11, a 4ª volta: os ícones) Ele: "eu quero algo ICÔNICO, trilhas LINDAS, MARAVILHOSAS, ICÔNICAS, nível de onde o pessoal vai
# pegar e botar no YouTube e os outros vão comentar 'THE GOAT'". Saem os loops das outras facções (repetition.py: China Iron Assembly 72%
# e Red Dragon 84% de cópia; GLA Dust and Shrapnel 80%, Scrap and Shadow 64% e a minha Scorpion Road 74%; Rússia Northern Hammer 72%).
# O que faz um tema de RTS virar ícone (os clássicos de Klepacki e do ZH), e o que cada plano pede:
#  - uma ASSINATURA da facção, um som que só ela tem (um riff, um canto sem palavras, um timbre), já nos primeiros compassos;
#  - o gancho curto (1 ou 2 compassos) que gruda, voltando sempre transformado (outro instrumento, outra altura, outro ritmo);
#  - groove forte e pesado, a mixagem seca e firme, o grave limpo (o defeito das antigas era o grave embolado);
#  - trechos de 8 compassos com textura própria, a subida de tom antes do clímax, o fim de verdade;
#  - a mesma família da melhor peça de cada facção, de referência (o song_id da geração, sem custo de upload).
REF_CN = {'song_id': '5OdaiS6F7anrJ7L5GxnZ', 'range': {'start_ms': 0, 'end_ms': 14000}}       # china_defeat: erhu, metais, gongo (10/9/9)
REF_GLA = {'song_id': 'rKMv041OxAkXjxTix0o8', 'range': {'start_ms': 1500, 'end_ms': 15000}}   # gla_defeat: duduk, oud, cordas (10/8/9)
REF_RU = {'song_id': 'lfQv9xgS5WOmktQNxyB3', 'range': {'start_ms': 17000, 'end_ms': 47000}}   # russia_iron_directive, o Theme (10/9/8); a referência vai até 30 s (a 1ª tentativa, com 34 s, a API recusou)
ICON_NEG = ['repetitive loop', 'same section repeated', 'generic epic trailer', 'muddy low end']

plan_ref('china_dragon_ascent', 'china', 'battle', 136,
         'Ícone da China, batalha 1: o riff do erhu dobrado com a guitarra distorcida, em pentatônica, sobre os tambores dagu de guerra e o sub eletrônico; a suona no clímax.',
         ['modern Chinese battle anthem', 'erhu and distorted electric guitar unison riff', 'dagu war drums', 'electronic sub bass', 'gongs and cymbals', 'suona lead', '136 bpm, minor pentatonic'],
         [('Intro', 4, ['a huge gong hit, then the erhu alone plays the signature: a fierce two-bar pentatonic riff, instantly memorable'], []),
          ('Riff', 8, ['the distorted guitar doubles the erhu riff in unison over pounding dagu war drums and a deep electronic sub bass', 'short wordless male shouts "ha" on the accents'], ['erhu and guitar unison riff', 'dagu drums', 'shouts']),
          ('Verse', 8, ['the pipa plays a fast tremolo answer, the drums switch to a broken half-time groove, the riff hinted by low brass'], ['pipa tremolo', 'half-time groove', 'low brass']),
          ('Lift', 8, ['the dizi flute takes the riff an octave higher over driving drums, Peking opera clappers and cymbals as accents'], ['dizi lead', 'opera percussion', 'driving']),
          ('Break', 8, ['only the sub bass, a ticking woodblock and a long erhu note bending, then a big dagu roll'], ['sparse', 'tension', 'drum roll']),
          ('Climax', 8, ['one step higher: the suona screams the riff over the full band, guitars, erhu and drums in unison'], ['suona lead', 'full power', 'higher key']),
          ('Climax 2', 8, ['the riff in canon between suona, erhu and guitar, double-time drums, gong on every phrase'], ['canon', 'double-time', 'gongs']),
          ('Ending', (4, 4.0), ['the riff one last time in half-time', 'a final unison hit with a giant gong ringing out; no fade-out'], ['final hit', 'gong ring-out'])],
         ref=REF_CN, strength='medium', extra_neg=ICON_NEG + ['western symphony orchestra', 'choir', 'lyrics'])
plan_ref('china_iron_lotus', 'china', 'battle', 118,
         'Ícone da China, batalha 2: pesada e em meio-tempo; o tremolo da pipa como riff, o baixo industrial, os pratos da ópera de Pequim como assinatura e o erhu cantando por cima.',
         ['Chinese industrial electronic', 'pipa tremolo riff', 'heavy industrial synth bass', 'Peking opera cymbals and clappers', 'soaring erhu melody', 'taiko-like Chinese drums', '118 bpm, minor pentatonic'],
         [('Intro', 4, ['Peking opera clappers and cymbals start a fast pattern, a dark synth bass drone underneath'], []),
          ('Riff', 8, ['a heavy half-time beat; the pipa tremolo plays the signature riff, the industrial bass locks with it'], ['pipa riff', 'half-time beat', 'industrial bass']),
          ('Song', 8, ['the erhu sings a soaring, memorable melody over the riff, wide and emotional'], ['erhu melody', 'soaring', 'emotional']),
          ('Machine', 8, ['the beat turns mechanical and faster, metallic hits, the riff chopped and glitched'], ['mechanical', 'metallic hits', 'glitched riff']),
          ('Break', 8, ['the drums stop: a guzheng plays the melody softly over the bass drone, then the opera cymbals return building'], ['sparse', 'guzheng', 'build']),
          ('Climax', 8, ['one step higher: the erhu melody and the pipa riff together over the full beat at maximum weight'], ['full weight', 'higher key', 'erhu and pipa']),
          ('Ending', (4, 4.0), ['the riff slows to a final heavy hit', 'a gong and the bass ring out naturally; no fade-out'], ['final hit', 'natural ring-out'])],
         ref=REF_CN, strength='low', extra_neg=ICON_NEG + ['western symphony orchestra', 'choir', 'lyrics'])

plan_ref('gla_black_market', 'gla', 'battle', 126,
         'Ícone da GLA, batalha 1: o lamento sem palavras de uma voz masculina (no estilo mawwal) como gancho, o riff do oud distorcido na escala hijaz e o breakbeat de darbuka.',
         ['Middle Eastern industrial hip-hop', 'wordless male vocal wail, mawwal style', 'distorted oud riff, hijaz scale', 'darbuka breakbeat', 'heavy sub bass', 'dusty and gritty', '126 bpm'],
         [('Intro', 4, ['a lone wordless male vocal wail in the hijaz scale over desert wind, the signature hook of the track'], []),
          ('Riff', 8, ['a heavy darbuka breakbeat drops with a distorted oud riff, short and catchy, and a deep sub bass'], ['oud riff', 'darbuka breakbeat', 'sub bass']),
          ('Wail', 8, ['the vocal wail returns over the beat, answered by the oud, gritty electric guitar doubling the bass'], ['vocal wail', 'oud answer', 'gritty guitar']),
          ('Market', 8, ['the beat switches to a fast frame drum pattern with claps, the riff played by a zurna, chaotic and alive'], ['frame drums', 'claps', 'zurna riff']),
          ('Break', 8, ['only a filtered bass and a darbuka, radio static, the wail far away, then a big drum fill'], ['sparse', 'tension', 'drum fill']),
          ('Climax', 8, ['one step higher: the full beat, the oud riff and the vocal wail together at full force'], ['full force', 'higher key', 'wail and riff']),
          ('Ending', (4, 4.0), ['the riff stops on one hit', 'the last vocal note and the hit ring out naturally; no fade-out'], ['final hit', 'natural ring-out'])],
         ref=REF_GLA, strength='low', extra_neg=ICON_NEG + ['lyrics', 'female vocals', 'western orchestra'])
plan_ref('gla_ambush_alley', 'gla', 'battle', 140,
         'Ícone da GLA, batalha 2: a emboscada rápida; a zurna estridente no riff, tambores de moldura com bateria de rock, a guitarra suja e sirenes ao longe.',
         ['Middle Eastern rock', 'shrill zurna lead riff', 'frame drums and rock drums', 'dirty electric guitar', 'distant sirens texture', 'darbuka', '140 bpm, phrygian dominant'],
         [('Intro', 4, ['distant sirens and a frame drum roll, then the zurna shrieks the signature riff alone'], []),
          ('Attack', 8, ['rock drums and frame drums together, the dirty guitar locks with the zurna riff, fast and aggressive'], ['zurna riff', 'rock drums', 'dirty guitar']),
          ('Run', 8, ['the guitar plays a new riff, the zurna answers in short bursts, the darbuka drives a double-time pattern'], ['new riff', 'zurna bursts', 'double-time darbuka']),
          ('Hide', 8, ['the band drops to a tense pulse: oud plucks and a low drone, footsteps-like percussion, the riff hinted'], ['tense', 'oud plucks', 'low drone']),
          ('Climax', 8, ['one step higher: the zurna and guitar riff in unison over the full drums, the sirens returning'], ['full force', 'higher key', 'unison riff']),
          ('Climax 2', 8, ['the riff in call and response between zurna and guitar, claps and shouts without words'], ['call and response', 'claps', 'shouts']),
          ('Ending', (4, 4.0), ['the riff ends on a hit', 'a natural ring-out; no fade-out'], ['final hit', 'natural ring-out'])],
         ref=REF_GLA, strength='low', extra_neg=ICON_NEG + ['lyrics', 'female vocals', 'western orchestra'])

plan_ref('russia_hammer_and_steel', 'russia', 'battle', 132,
         'Ícone da Rússia, batalha: a marcha industrial; o passo de botas no tempo como assinatura, o coro masculino gritando sem palavras, metal e metais pesados, a bigorna; pelo tema da facção (Iron Directive) de referência.',
         ['industrial metal march', 'marching boots stomping on every beat', 'deep male choir chanting wordless', 'heavy distorted guitars', 'heavy brass', 'anvil hits', '132 bpm, minor key'],
         [('Intro', 4, ['marching boots stomp in perfect time, an anvil strikes on beat four, a low choir hums the dark 4-note faction motif'], []),
          ('March', 8, ['heavy guitars and drums lock with the boots, the brass blasts the motif, the choir shouts a wordless chant on the backbeat'], ['boots and drums', 'brass motif', 'choir chant']),
          ('Chant', 8, ['the choir sings the motif in unison like an army, the guitars chug under it, anvils on every bar'], ['army choir unison', 'chugging guitars', 'anvils']),
          ('Steel', 8, ['the guitars play the motif as a riff, the drums double-time, the brass answers with stabs'], ['guitar riff', 'double-time', 'brass stabs']),
          ('Break', 8, ['only the boots, a distant air raid siren and a low bayan accordion playing the motif, cold, then a huge drum fill'], ['sparse', 'boots', 'bayan']),
          ('Climax', 8, ['one step higher: choir, brass and guitars in unison on the motif over the boots, maximum force'], ['maximum force', 'higher key', 'choir and brass unison']),
          ('Ending', (4, 4.0), ['the march stops on one giant unison hit', 'the choir chord and a low gong ring out naturally; no fade-out'], ['final hit', 'natural ring-out'])],
         ref=REF_RU, strength='medium', extra_neg=ICON_NEG + ['lyrics', 'female vocals'])

print('\n'.join(sorted(os.listdir(OUT))))
