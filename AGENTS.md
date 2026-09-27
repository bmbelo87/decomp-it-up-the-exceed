# 🚨 REGRA DE URGÊNCIA MÁXIMA - SEMPRE VERIFICAR PRIMEIRO 🚨

**"Desativar" significa:** Envolver o bloco da função em `/*` `*/` (comentário). **NUNCA apague ou delete código** a menos que o usuário explicitly diga "delete" ou "apague".

Sempre que o usuário pedir para desativar algo, apenas comente o bloco. Preserve o código original intacto dentro do comentário.

---

# OpenCode Instructions

## Project Objective

This project is a reverse engineering and reconstruction effort.
**The objective is NOT to redesign, modernize or improve the original game.**
The objective is to reproduce the original executable as faithfully as possible.

### Priority order:

1. Original behavior
2. Binary compatibility whenever possible
3. Equivalent logic
4. Equivalent rendering
5. Equivalent timing
6. Code readability (only when it does not alter behavior)

> **Note:** If there is a conflict between cleaner code and compatibility with the original executable, compatibility always wins.

---

## Language

* Always respond in Português (Brasil).
* Use technical terminology whenever appropriate.

---

## Thinking Process

Before answering:

1. Observe
2. Analyze
3. Build hypotheses
4. Search for evidence
5. Reach a conclusion

**Never** skip the verification step.

---

## Confidence Levels

Every conclusion should implicitly fit one of these categories:

* Confirmed
* Highly Probable
* Probable
* Hypothesis

**Never** present assumptions as facts. Whenever evidence is insufficient, clearly state the uncertainty.

---

## Development Philosophy

* **Never** refactor simply because the code looks old.
* **Never** modernize APIs.
* **Never** replace legacy algorithms without evidence.
* **Never** optimize code unless explicitly requested.
* Prefer preserving original behavior over writing cleaner code.

---

## Programming Languages

**Preferred languages:**

* C
* Python
* PowerShell

Lua may be used when appropriate. Avoid introducing unnecessary C++ abstractions.

---

## Coding Style

* Prefer procedural code.
* Avoid unnecessary abstractions.
* Avoid unnecessary classes.
* Avoid templates.
* Avoid macros unless already used.
* Prefer small isolated patches.
* **Never** rewrite an entire subsystem to solve a localized issue.

---

## Reverse Engineering

Always prioritize evidence. Decompiler output is **NOT** authoritative. Assembly always has higher priority than decompiled C.

When analyzing unknown behavior, follow this order:

1. Strings
2. Imports
3. Cross References
4. Global Variables
5. Structures
6. Assembly
7. Decompiled C

Avoid jumping directly into decompiled code.

---

## Decompiled Code

Decompiler generated symbols such as:

* `FUN_xxxxxxxx`
* `DAT_xxxxxxxx`
* `PTR_xxxxxxxx`
* `LAB_xxxxxxxx`

...should be preserved unless explicitly requested.

* **Never** rename functions automatically.
* **Never** infer semantics solely from variable names.

---

## Binary Analysis

When investigating binary formats always identify:

* Magic
* Version
* Header Size
* Flags
* Endianness
* Alignment
* Padding
* Offset Tables
* Pointer Tables
* Checksums
* Compression
* Encryption

**Never** stop after identifying only the header.

---

## Compression

**Never** assume encryption first. Always verify:

* RLE
* LZ77
* LZSS
* Deflate
* Zlib
* LZO
* Huffman
* XOR

Only after excluding common compression methods should encryption be considered.

---

## Structures

**Never** modify:

* Structure packing
* Offsets
* Field ordering
* Integer sizes
* Alignment

...unless there is concrete evidence that they are incorrect. Always preserve binary compatibility.

---

## OpenGL

* Preserve original rendering order.
* Avoid changing draw order.
* Avoid introducing state changes unnecessarily.
* **Never** assume OpenGL state.
* Preserve original rendering pipeline whenever possible.

---

## Legacy Windows

Assume the original executable targets:

* Windows 95
* Win32 API
* OpenGL
* DirectSound
* Visual C

Timing differences between Windows 95 and modern Windows should always be considered.

---

## Ghidra

When using Ghidra:

1. Prefer Strings.
2. Then Imports.
3. Then Xrefs.
4. Only inspect functions relevant to the current task.
5. Request only one new function at a time.
* Avoid recursive exploration.
* Avoid mass decompilation.
* **Never** request the entire executable.

---

## Context Usage

* Minimize token consumption.
* Do not request information already analyzed.
* Do not request files already provided.
* Prefer summaries over duplicated code.
* Reference functions by address whenever possible.

---

## Code Modification Workflow

Before modifying any code:

1. Explain what the code currently does.
2. Explain why it should change.
3. Explain possible side effects.
4. Only then propose modifications.

### Regra de Leitura de Funções

Antes de chamar qualquer função de renderização/posicionamento (ou qualquer função que receba coordenadas), **ler a implementação primeiro** — não assumir o que os parâmetros significam. Verificar:

- O que `x, y` representam (canto? centro? topo? base?)
- Qual convenção de eixos (Y-down? Y-up?)
- Como a função calcula posições internamente

**NUNCA** chamar uma função de posicionamento sem ter lido o código dela antes.

---

## Data Analysis

When comparing binary files:

* Identify common regions.
* Identify changed regions.
* Identify pointer differences.
* Identify checksum changes.
* Identify alignment changes.

**Never** classify changed bytes as random without evidence.

---

## Python

Python scripts should:

* Accept command-line arguments.
* Support recursive processing when appropriate.
* Avoid unnecessary dependencies.
* Print meaningful diagnostics.
* Prefer standard library.

---

## PowerShell

* Always assume a Windows environment.
* **Never** generate Linux shell commands.
* **Never** generate Bash scripts unless explicitly requested.
* Prefer native PowerShell commands.

---

## Documentation

Separate information into:

* Facts
* Evidence
* Hypotheses
* Open Questions

Avoid mixing speculation with confirmed information.

---

## Response Style

* Prefer technical explanations.
* Avoid generic advice.
* Avoid motivational language.
* Avoid repeating previous explanations.
* If multiple interpretations exist, explain each.

---

## Build Process

**Compilation + copy:**

```sh
cmake --build build --target Pumpy --config Debug -- /m; if ($?) { Copy-Item -LiteralPath "build\Debug\Pumpy.exe" "E:\Pumps\PREX3-Original\PUMPYTESTE.EXE" -Force }
```

Assets are copied automatically. O target `Pumpy` evita buildar as ferramentas (dump_bga2, etc.).

**After build and copy process, always ASK if I want to continue another thing.**

---

## Common Pitfalls

### Divisão inteira no centro de sprites

Quando for calcular o centro de um tile de SPR para `Sprite_DrawTileUV`, **nunca** use:

```c
// ERRADO: sw/2 trunca pra int se sw for int
Sprite_DrawTileUV(idx, (float)(sx + sw/2), ...);
```

Sempre converta pra `float` **antes** da divisão:

```c
// CERTO
float sx = (float)g_game.sprTiles[idx].srcX;
float sw = (float)g_game.sprTiles[idx].srcW;
Sprite_DrawTileUV(idx, sx + sw / 2.0f, ...);
```

Sprites com largura/altura ímpar (ex: 133x63) sofrem deslocamento de 0.5px se usar divisão inteira.

### Posição de sprites da 00.DAT

**Regra:** renderizar SEMPRE na posição natural do `srcX`/`srcY` do tile definido no `.spr`. Única exceção é o offset de +320px no eixo X para o P2 (definido pelo Ghidra em `FUN_0040d960`).

**NUNCA** adicione offsets manuais (tipo `ox=28`, `oy=-12`) — a posição correta já está no arquivo `.spr`.

### Tile count do SPR

Sempre verifique o `NUM` na segunda linha do arquivo `.spr` antes de assumir quantos tiles um SPR tem. Use a função `sprTileCount(startIdx)` que conta automaticamente pelo padrão de nome (`nome_N`):

```c
int cnt = sprTileCount(g_fontSprXX);
for (int t = cnt - 1; t >= 0; t--) { ... }
```

**NUNCA** hardcode `for (int t = 1; t >= 0; t--)` — se o SPR tiver `NUM 1`, o `t=1` renderiza um tile de outro SPR (o próximo na ordem de carregamento).

---

## Logging

Use:

```c
Log_Print()
```

Log location: `root/pumpy.log`

---

## Original Executable

Original executable characteristics:

* **System:** Windows 95 (32-bit)
* **Compiler:** Microsoft Visual C 13.00
* **Language:** C
* **Graphics:** OpenGL
* **Audio:** DirectSound
* **Protection:** PELock 2.x

> **Note:** Decompiler output may contain artifacts caused by PELock.

---

## Rendering

* **Projection:** Y-UP
* **External API:** Y-DOWN

**V NÃO é invertido** (desde o port SDL2, `bb82a22`). Todas as texturas são PNG e o
decodificador de `texture.c` não inverte linhas: **V=0 é o topo da imagem**, a mesma
convenção dos `.SPR`. Passe V direto. **NUNCA** use `256 - v`, `th - 1 - v` ou `1 - v` —
foi isso que quebrou o preenchimento da lifebar do HalfDouble (corrigido em 25/09/2026).
Ver "SPR Y/UV Convention".

### Global Color Overlay (Render_EndScene)

- `Render_SetGlobalColor(r, g, b, a)` controla o overlay fullscreen desenhado por `Render_EndScene`
- O overlay só aparece quando `globalColorA > 0.0f`
- **NUNCA** chame `Render_SetGlobalColor(1,1,1,1)` em telas novas — isso produz overlay branco opaco que tampa tudo (`alpha=1` = opaco)
- Ao criar uma nova tela, omita `Render_SetGlobalColor` ou passe `alpha=0` para não sobrescrever o overlay
- **IMPORTANTE:** Se você setar `alpha=1`, o overlay preto tapa TUDO. A cada tela, verifique se o `alpha` está sendo atualizado corretamente ao longo dos frames (fade-in/fade-out). Caso contrário a tela fica preta e parece que o conteúdo não carregou.

---

## BGA Parsing

**Supported formats:** `BGA`, `BGA2`
Detect version through magic bytes.

Shared parsing structures must use:

```c
#pragma pack(push,1)
```

---

## Project Conventions

* Shared structures belong in: `include/pumpy.h`
* Every STATE screen should default to: `RGBA(0,0,0,1)` unless the original executable behaves differently.
* Whenever `.tga` resources are referenced, project assets should use `.png`.
* All DAT resources are already extracted inside: `BGA_extracted`
* **Never** decrypt any DAT file — all are already extracted in `BGA_extracted`
* **Never** propose extracting DAT files again unless explicitly requested.
* `81.DAT` → Somente animação do LOGO, não mexer
* `82W.DAT` → Animações da tela de menu, não mexer
* SPR files contain sprite coordinate definitions, not textures.
* Use `C:\Users\bruno\AppData\Local\Temp\parse_bga3.py` to analyze entry names and structure of BGA files. It parses the fixed-record format correctly (64-byte name fields, count, keyframes).

---

## Critical Rules

- **Always verify in GhidraMCP** before implementing any format fix — never base corrections on format hypotheses alone. The decompiled original always has priority.
- When analyzing the original, follow the data flow: what does it READ from the file/buffer? The read pattern reveals the actual format (e.g., fixed 64-byte records vs variable-length strings).

## Reverse Engineering Rules

**Never** request:

* The entire executable
* Every function
* All globals
* Large unrelated code blocks

Only request information strictly necessary for the current objective. Always justify why additional information is needed.

---

## Resource Mapping

**NUNCA** assuma qual arquivo DAT/BGA/AUD/WAV corresponde a qual tela ou recurso. As numerações nem sempre são óbvias (ex: Song Select é `099.DAT`, não `SELECT.DAT`). Sempre pergunte ao usuário antes de tirar conclusões sobre nomes de arquivos ou mapeamentos de recursos.

## Investigation Checklist

Before concluding any investigation verify:

- [ ] Original behavior identified
- [ ] Related globals identified
- [ ] Related structures identified
- [ ] Related strings identified
- [ ] Related xrefs identified
- [ ] Related rendering identified
- [ ] Compression checked
- [ ] Encryption checked
- [ ] Side effects evaluated

Only after completing this checklist should a final conclusion be presented.

---

## Session Summary — Staff Screen (DL 2x)

### Goal

Implementar tela de Staff (créditos) ativada via DL 2x no menu.

### Done

- `src/staff.c` criado com `Staff_Enter()` e `Staff_Update(float dt)`
- `Staff_Enter()`: `Resource_LoadBGADirect("BGA\\STAFF.DAT")` carrega RES com Staff.bga + S.SPR, S00.spr + staff01~08.png; `BGM_LoadAUDDirect` toca 84.AUD; `bgaLoop=false`, `bgaFrame=0`, `frameCounter=0`
- `Staff_Update()`: incrementa `frameCounter` até 1920, sincroniza `bgaFrame`. ESC/CENTER ou fim → `BGM_Stop()` + `Menu_ResetState()` + `Game_ChangeState(STATE_MENU_ENTER)`
- `LoadBGAForState`: `STATE_STAFF_ENTER/STAFF/STAFF_END` → `bgaName = ""` (limpa BGA)
- `Game_Update`: cases `STATE_STAFF_ENTER` (Staff_Enter), `STATE_STAFF` (Staff_Update), `STATE_STAFF_END` (volta menu). `manualBGA` inclui `STATE_STAFF`. ESC handler trata STAFF.
- `Game_Render`: `STATE_STAFF` usa `BGA_Render` padrão (não excluído)
- `menu.c`: DL 2x confirma → fade → `STATE_STAFF_ENTER`
- `CMakeLists.txt`: adicionado `src/staff.c`
- `resource.c`: parser BGA2 aceita `.png`/`.PNG` além de `.spr`/`.sp2`/`.tga`/`.TGA`
- Removido `Render_SetGlobalColor(1,1,1,1)` de Staff_Enter (causava overlay branco)

### Key Fixes

1. **Tela branca**: causada por `Render_SetGlobalColor(1,1,1,1)` em Staff_Enter — overlay branco full-alpha tampava o BGA
2. **File path**: `Resource_LoadBGADirect("BGA\\STAFF.DAT")` funciona do diretório raiz do jogo (`E:\Pumps\PREX3-Original\`) sem precisar de fallback `assets/`

### Known

- RES entries usam XOR contínuo (key 0xEF, step 0x4F) sem reinício entre entries — decodificação funciona corretamente
- `loadBGAFromRES` (usado por `Resource_LoadBGAByName`) NÃO funciona pra Staff.bga (offset 16 não tem filenames); `Resource_LoadBGADirect` (scan de extensões) funciona
- Staff.BGA2 tem 2 layers: S00.spr (frames 0-325) e S.spr (frames 115-1891)
- S.SPR: 10 tiles, S00.spr: 2 tiles, todos referenciando `staffXX.tga` resolvido via `loadTextureFromRES` para `.png`
- Texturas staff01~08.png: 256x256 cada

## SPR Y/UV Convention

SPR files armazenam **V com 0 no topo**. O decodificador de PNG (`texture.c`) carrega a
imagem **sem inverter linhas**, então no OpenGL o V=0 também é o topo — as duas convenções
coincidem e **nenhuma inversão é necessária** (verificado em 25/09/2026):

- `drawTileQuad()` / `renderSPTile()` (`bga.c`): `glTexCoord2f(tile->u1, tile->v1)` direto.
- `Sprite_DrawTileUV()` (`util.c`): converte V normalizado → pixels (`v * th`), sem flip.
- `Texture_DrawUV()` (`texture.c`): recebe V **em pixels**, `v1` vai no vértice de cima,
  `v2` no de baixo; usa `v / altura` direto.
- `loadSPRFromRES()` (`resource.c`): armazena UV normalizado sem flip.

Coordenadas fixas de atlas (ex.: linhas 144–158 do `ST02.PNG`) entram **direto** como `v1`/`v2`.

> Histórico: antes do port SDL2 as texturas vinham por WIC/TGA com linhas invertidas e o
> projeto aplicava `1 - v` / `256 - v`. O port removeu essas inversões (ver `git diff
> 0a5ac4b bb82a22` em `song_select.c`, `result.c`, `resource.c`). Qualquer resquício dessa
> regra é bug.

## Judge & Combo Positioning Corrections (22/06/2026)

### Goal
- Correct positions of combo numbers, combo sprite, and popup effect to match original executable

### Problem Identified
- **Y=80 in Ghidra is RELATIVE to container, not absolute screen position**
- Original uses `glPushMatrix()`/`glPopMatrix()` to establish coordinate system
- Our hardcoded Y=80 was treating it as absolute screen position

### Solution
- Calculate position based on screen proportion: Y = 350.0f (~70% of screen height)
- Original layout (640x480): Lifebar → Receptor → Judge → Combo → ComboSprite
- Combo should appear below judge, not near receptor

### Corrections Made
1. **Combo Numbers**: 
   - Changed from `centerY + 20` to `receptorY + 42.0f`
   - File: `src/gameplay.c:1382`

2. **Combo Sprite ("combo_")**:
   - Changed from `centerY + 55 + 50` to `receptorY + 42.0f`  
   - File: `src/gameplay.c:1393`

3. **Popup Effect**:
   - Changed from `Y = 38.0f` to `receptorY + 42.0f`
   - Updated all `popupCreate` calls: `src/gameplay.c:409, 562, 609`

### Original Coordinates (from Ghidra)
- Combo numbers: `Font_DrawNumberP1(..., 0x50, ...)` - Y = 0x50 = 80 (relative to container)
- Judge sprites: BGA event layer positioning (maintained as `centerY + 55 - 50`)
- Receptor position: Y = 38 (our system) vs unknown (Ghidra)
- Combo position: Y = 80 (Ghidra) = receptorY + 42 (our calculation)

### Files Modified
- `src/gameplay.c`: Position corrections for combo rendering relative to receptor
- `PUMPYTESTE_COMBO_POSITION.EXE`: Test executable with corrected positioning

## Original Combo Rendering System Implementation (22/06/2026)

### Goal
Implement original combo rendering system from PUMPY.EXE using Combo_DrawMain (0x00411b40), Combo_DrawSprite (0x00411a90) and Combo_DrawDigit (0x004119d0)

### Implementation Details

#### 1. Added Combo Comparison Variables
- Added `combo_0` and `combo_1` to `GameplayStats` structure for player comparison logic
- File: `include/pumpy.h:167-168`

#### 2. Implemented Original Combo Functions
- **Combo_DrawMain()** (0x00411b40): Main combo rendering function with special cases for 1000, 2000, 3000
- **Combo_DrawSprite()** (0x00411a90): Special combo sprite rendering function ("COMBO", "MAX COMBO")
- **Combo_DrawDigit()** (0x004119d0): Individual digit rendering function using 6x4 grid texture coordinates
- File: `src/gameplay.c:1519-1625`

#### 3. Updated Combo Logic
- Modified `Gameplay_UpdateCombo()` to update both `g_game.stats.combo[p]` and comparison variables
- Added combo comparison logic using original special values (1000=P1 higher, 2000=equal, 3000=P2 higher)
- File: `src/gameplay.c:1025-1035`

#### 4. Texture Binding Fix
- Changed from `Font_BindTexture()` to `Texture_Bind(g_fontTexId)` for proper OpenGL texture binding
- Fixed compilation issues with modulo operation on double and void pointer division
- File: `src/gameplay.c:1521,1569`

#### 5. Original Rendering Features
- Special cases for combo values 1000, 2000, 3000 showing comparison sprites
- Dynamic Y positioning using original timing formula: `(DAT_00da2264 % 0x3c) / 10 + offset`
- OpenGL glBegin(GL_QUADS) for direct sprite rendering as in original
- Original texture coordinate calculations for digit sprites (6x4 grid)
- File: `src/gameplay.c:1401,1550-1574`

### Key Technical Details
- **Texture System**: Uses `g_fontTexId` (font.tga) for digit rendering
- **Sprite System**: Uses `g_fontArrow542` for combo sprite indices
- **Comparison Logic**: 1000=P1 higher, 2000=equal, 3000=P2 higher
- **Positioning**: Dynamic Y calculated from song timing, relative to receptor position
- **Rendering**: Direct OpenGL quad rendering with original texture coordinates

### Testing
- **Build**: Successful compilation with fixed texture binding and modulo operations
- **Executable**: `PUMPYTESTE_COMBO_POSITION.EXE` contains complete original combo rendering system
- **Next Steps**: Test special cases (1000, 2000, 3000) and verify combo comparison logic

**Sempre que carregar/desenhar SPRs, verificar se Y/V não está invertido.** Se um tile aparecer de ponta-cabeça ou mostrar outro sprite, procure uma inversão de V sobrando (`1 - v`, `256 - v`, `th - 1 - v`) e **remova-a** — V entra direto (ver "SPR Y/UV Convention").

---

## 🚨 REGRA ABSOLUTA — Git ⚠️

- **Claude (Claude Code)** pode executar `git add`, `git commit` e `git push` **somente quando as duas condições valerem**:
  1. o usuário pediu o commit/push explicitamente na conversa; **e**
  2. as mudanças incluídas foram discutidas e o usuário concordou com elas.
  Antes de commitar, mostrar o que entra (`git status`) e confirmar o escopo em caso de dúvida.
- **Qualquer outro agente/IA:** **nunca** execute `git commit`, `git push`, `git pull`, `git add`, ou qualquer outro subcomando que modifique o repositório. Permitido apenas `git diff HEAD` (consulta visual, sem alterações).
- **Para todos, sempre proibido:** `git push --force`, `git reset --hard`, `git rebase`, reescrever histórico ou apagar branches, a menos que o usuário peça exatamente isso.

---

## Stage System

### Grade (calcGrade em result.c)

Fórmula do original, extraída do fim de `Gameplay_ProcessJudgment` (`0x0041042c`):

```
razao = (perfect + great×0.9 + good×0.6 − bad×0.5 − miss + maxCombo×0.03) / total
```

O termo `maxCombo×0.03` só entra fora do modo EVENT (`svcGameMode != 1`).

Escada de `DanceGradeDisplay` (`0x00415330..0x004153ac`):
- S ≥ 1.0 **e** miss == 0 · A ≥ 0.9 · B ≥ 0.8 · C ≥ 0.7 · D ≥ 0.6 · F < 0.6
- S=0, A=1, B=2, C=3, D=4, F=5

> **Cuidado:** a decompilação do Ghidra mostra o resultado sendo gravado com cast
> `(int)` em `[0x00da22d0]`, o que sugere um inteiro. É erro de tipagem —
> `DanceGradeDisplay` lê o mesmo endereço com `FLD float ptr`. É float.

> Até 09/2026 o projeto usava `(perfect×10 + great×7 + good×5 + bad×2) / (total×10)`
> com cortes 0.95/0.85/0.75/0.60/0.40. Estava errado: os pesos divergiam, o BAD
> **somava** em vez de subtrair, o MISS era ignorado e faltava o bônus de combo.

### Stage Flow (g_game.stageCount)
- `Menu_ResetState()` define: stageCount=3, bonusStage=true, isBonusSong=false
- Loading.Enter decrementa stageCount (3→2→1→0); se stageCount==0, seta isBonusSong=true
- Stage 1: stageCount=2 durante gameplay → M01.SPR
- Stage 2: stageCount=1 → M02.SPR
- Stage 3/Final: stageCount=0 → M04.SPR
- Bonus: isBonusSong=true → M05.SPR

### Grade Result (Result_GetNextState)
- grade == 5 (F) → GameOver direto
- grade >= 2 (B, C, D) → `bonusStage = false`
- `isBonusSong == true` → GameOver direto (bonus já foi)
- `stageCount > 0` → Stage Transition (LT01)
- `bonusStage == true && stageCount == 0` → Stage Transition (LT03) → Song Select (bonus)
- Senão → GameOver

### Mapeamento Menu → Stage
1. Menu_ENTER → Menu_ResetState() seta stageCount=3
2. Song Select → Loading.Enter decrementa
3. Gameplay → mostra Mxx.SPR
4. Grade → decisão
5. Stage Transition (60 frames) → volta ao Song Select

O cursor do Song Select **volta pra última música jogada** (`g_game.songSelectHighlighted = g_game.selectedSongIndex` em loading.c).

---

## 🚨 REGRA ABSOLUTA — SPR vs SP2

**SP2 = `SPR_LoadSP2()`** — u2/v2 são **offsets** (largura/altura) a partir de u1/v1. Negativo = flip.

**SPR = `SPR_LoadSPR()`** — u2/v2 são **absolutos** (Left/Top → Right/Bottom no formato TGA).

NUNCA confunda os dois. Verifique a extensão do arquivo antes de chamar a função:
- `.sp2` → `SPR_LoadSP2`
- `.spr` → `SPR_LoadSPR`

---

## Exceed — Song Select (análise do exceed.exe, 26/09/2026)

Lista de músicas completa em `exceed_songlist.txt` (raiz). Nada disso vem do `stage.cfg`: está tudo compilado no `.exe`.

### Fatos (confirmados no assembly)

- **Tabela de músicas** `0x004551E0`: 105 registros × 56 bytes, terminados por `id=0`.
  `+0x00 u32 id (hex → nome de arquivo via %X)`, `+0x04/+0x08 artista KR/EN`, `+0x0C/+0x10 título KR/EN` (ponteiros para `.data1`, CP949),
  `+0x14 u32 0`, `+0x18 double BPM`, `+0x20..+0x30 5×s32 níveis NORMAL/HARD/CRAZY/FREESTYLE(=Double no dado)/NIGHTMARE (-1 = inexistente)`,
  `+0x34 byte visível`, `+0x35 byte oculta`. Na init (`0x416474`): `vis = (oculta == 0)`.
- **Canais** `int[3][50]` em `0x004568E0` (linha 0xC8 bytes, lista terminada em 0). É a ordem de exibição.
  0 = BANYA (46), 1 = K-POP (31), 2 = POP (28). As 105 músicas aparecem uma vez cada. Nome do canal por índice: altamente provável (pelo conteúdo).
- **Montagem da lista visível** `0x4192F0` → `0x563A00`. Oculta só entra se o jogador tiver o bit `0x2000` em `+0x184`.
- **Arquivos por ID**: `STEP\%X.STX`, `AUDIO\%X.AUD`, banner `%X.TGA` dentro de `BGA\90.DAT`.
- **Banners** (`0x416206..0x41624F`): abre `90.DAT`, carrega `%X.TGA` para cada uma das 105 músicas e grava o handle em `0x456B40[i]+4` (`{id, tex}`, tex inicial -1).
- **SELECT.DAT → `[obj+4]`, SELECT2.DAT → `[obj+8]`**: os dois BGAs ficam carregados ao mesmo tempo (`0x4161E6`). Os assets são idênticos; só o `.bga` muda.
  O SELECT2 é usado por slot: `0x41F55C(frame, slot)` posiciona o slot e `0x41F754(i, 1.0)` define intensidade (0.5 = apagado).
- **Troca de canal**: `[+0x5C]` = canal, `[+0x60]` = canal anterior, `[+0x64]` = direção.
  - P1 `0x08` / P2 `0x800` → `0x417F87`: dir=1, canal-1 (0→2→1→0).
  - P1 `0x10` / P2 `0x1000` → `0x41803E`: dir=2, canal+1.
  - Animação (`0x416C1D`), 30 frames (t=0..29):
    - dir=1 → slots 6,7,10,11 (`Xrot1`, `Xrot1t`, `Xrot2`, `Xrot2t`, scaleX +1) no frame `480+t`.
    - dir=2 → slots 8,9,12,13 (os mesmos SPRs espelhados, scaleX -1) no frame `360+t`.
    - Fim ou canal igual → dir=0 e slot 24 no frame 30.
  - `Xrot1` (xrotR, 512 px) + `Xrot2` (xrotL, 128 px em x=512) = tela inteira, mesmo esquema de `X.spr` + `xs.spr`. O sufixo `t` = quadros finais 14–21.
- **Numeração de slots do BGA2**: conta **todos** os registros, inclusive os vazios (nome vazio e count 0). Isso bate com os índices usados pelo `.exe`.
- **Ícones de modificador** (SELECT2):
  - Slots 11–19 = **P1** (x=14); slots 1–9 = **P2** (x=585). A rotina `0x419B2C` testa o bit 2 (P2) e usa o jogador 1. (Corrigido em 26/09/2026: antes estava invertido.)
  - Bits em `+0x184`: `0x10`→x8 (slot 2), `0x08`→x4 (3), `0x04`→x3 (4), `0x02`→x2 (5).
  - Sem velocidade: `0x400` → rv (1); caso contrário, x2 apagado.
  - `0x80` → slot 6 (`r.spr`) aceso/apagado.
- **Painel de dificuldade** (SELECT2, slots 31–49):
  - Tabela `0x455194` = slots {44 normal, 45 hard, 46 crazy, 49 Battle}, com bits em `0x4551A4` {0x10, 0x20, 0x400, 0x40}.
  - Tabela `0x4551B4` = slots {44..48 normal, hard, crazy, free, night}, com bits em `0x4551C8` {0x10, 0x20, 0x400, 0x200, 0x800}.

### Hipóteses

- UL (vermelho esquerdo) = bit `0x08` e UR = `0x10`, por analogia com o Prex3 (`pumpy.h:375`). Isso não foi confirmado no Exceed.
- O bit `0x2000` do jogador = código de desbloqueio.
- **CUIDADO:** o `0x80` do Exceed acende o `r.spr`; no Prex3 o `pumpy.h` diz `0x80 = Non-Step`. Não reaproveitar os bits de modificador do Prex3 sem verificar.

### Em aberto

- Bits de m, v e ns (slots 7–9 do SELECT2).
- O que escolhe entre a tabela com Battle e a tabela com Freestyle/Nightmare.
- Posição e tamanho do banner no draw (`0x41972A` em diante).
- O que o slot 24 (`main_s.spr`) no frame 30 representa no repouso.

### Select implementada (`src/exceed_select.c`, 26/09/2026, confirmada visualmente)

- 3D do S3D = espaço do GL do projeto: **Y para cima, câmera olhando -Z**. `S3DSetProjection(43.603)` equivale a um frustum com câmera a 600 do plano z=0, que fica 1:1 com 640x480. Coordenadas e TexCoords do original entram direto, sem flip.
- **ERRADO (não repetir):** tratar o 3D como Y para baixo com +Z afastando (D3D8). Isso curva a roda para cima e deixa o banner central grande demais.
- Ponteiros S3D: `a6da7c` Begin, `a6da80` End, `a6da8c` Vertex3f, `a6da94` TexCoord2f, `a6da9c` Push, `a6daa0` Pop, `a6daa4` Translatef, `a6daa8` Rotatef, `a6dad0` SetOrtho, `a6dad4` SetProjection. A tabela de strings aparece deslocada em 1 em relação a esses ponteiros.

### Códigos de comando da Select (0x455140..0x455192, verificador 0x4155AC P1 / 0x415958 P2)

Buffers por jogador: 9, 5 e 6 botões. Quando algo muda, toca `2-1.WAV`. Bits em `+0x184` do jogador:
- `UL UR UL UR C`: velocidade x1→x2(0x2)→x3(0x4)→x4(0x8)→x8(0x10)→rv(0x400)→x1
- `UL UR DL DR C`: 0 → 0x20 (v) → 0x100 (ns) → 0x120 → 0
- `UL UR UL UR UL UR UL UR C`: ^0x400 (rv), limpa 0x2/0x4/0x8
- `DR DL UR UL DR DL UR UL C`: ^0x40 (m)
- `UL UR UL UR DL DR DL DR C`: ^0x80 (r), limpa 0x200
- `UL DL UR DR DR UL UR DL C`: ^0x800 (sem ícone, efeito não identificado)
- `DR DL UR UL DR UR DL UL C`: ^0x1000, limpa velocidade e rv (sem ícone, efeito não identificado)
- `UR UR DL UL DR UR UL UR UR`: |0x2000 = **desbloqueia as ocultas** (A03, A26, A27) + 0x419424
- `DL UR DL UR DR UL DR UL C`: [0x568FF4] ^= 0x8000 = **X-MODE** (flag global; identificado pelo usuário pela sequência, 26/09/2026)
- `DL DR DL DR DL DR`: zera todos os modificadores

Ícones do SELECT2: slot 1 rv 0x400 · 2..5 x8/x4/x3/x2 · 6 r 0x80 · 7 m 0x40 · 8 v 0x20 · 9 ns 0x100.
Nomes por inicial (Random/Mirror/Vanish/Non-Step): provável, não confirmado no gameplay.
