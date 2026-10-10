# 金幣嘉年華 Coin Carnival — AI Passport 推銀機計劃 (PLAN)

> 第 4 隻 AI Passport 遊戲（之前：Pip's Passport、消防小隊 Fire Brigade、午夜神偷 Midnight Heist）。
> 好似冒險樂園 / 機舖入面嘅推銀機：落幣、推台推、幣跌落前面就係你嘅。
> The 4th AI Passport game: an original coin pusher like the ones in Hong Kong family arcades.
> **Virtual coins only — no real money, no purchases, no cash-out, no gambling.** Coins and tickets
> are free in-game points; the only things you can "buy" are new machine themes and a prize collection.

Status: prototype (39 headless checks) + ESP-IDF v6 firmware (host harness 33 checks) built.
Decisions (Oct 2026): payout like a real machine (~50 % long-run incl. bonuses) but never stuck; the daily
bonus is replaced by a **play-time reward**; name 金幣嘉年華 Coin Carnival (`ai-passport-coincarnival`);
HK prize names kept.

---

## 0. 資料搜集 Research（真推銀機點運作）

### 0.1 基本機構 Basic mechanism
- **推台 Pusher shelf**: a motor-driven plate slides forwards and back under a fixed back wall. Coins dropped
  on the plate are scraped off by the wall as the plate retracts; the plate's front face then shoves the
  bed of coins towards the front lip. Coins over the lip are paid out; most machines are two-tier
  (moving upper plate → fixed lower field). [Wikipedia: Coin pusher][wiki] [coinpusher.com][cp]
- **Friction & compaction**: coins in a pile lock together, so many pushes only compact the stack without
  moving the front row — the "one more coin and it'll avalanche" illusion. Coins only move when pushed. [cp]
- **Coin entry**: coins come down a chute / pin board ("pin perspex", pachinko-like pins) so the landing
  point is partly random; the chute is angled so new coins often land on top, not behind the "avalanche
  zone". [US 5752699][p5752699] [cp]
- **Side gutters / lose chutes**: troughs or holes along both sides skim off sideways-moving coins; operators
  size them to set profit. They take roughly 20–50 % of coins. [cp] [GB 2378906][gb2378906] [US 5752699][p5752699]
- **Payout knobs**: shelf stroke and speed, edge-plate angle (steeper lip = fewer coins fall), gutter size,
  heavy prizes that soak up momentum. Typical standalone payout is ~30–50 %, less for ticket versions. [cp] [JP 3105617][jp3105617]

### 0.2 獎勵機制 Bonus features (Japanese medal pushers & redemption pushers)
- **入賞口 / chucker**: a pocket on the field or back wall; a medal passing through it starts a lottery
  (a slot or roulette) for extra medals. [Chucker patent][chucker]
- **Jackpot detector changes the stroke**: in one patent a disk through a jackpot pocket makes the pusher's
  stroke longer ("small / medium / big hit") so more medals drop. [US 5713572][p5713572]
- **Roulette + scattering wheel**: a detected medal spins a roulette; the result sets how long a rotating
  plate scatters medals onto the field. [US 5507490][p5507490]
- **Ball lotteries (Konami Fortune Trinity)**: orbs/balls pushed off the field go to a "Trinity croon" (pick
  which jackpot), then a funnel where the ball circles, loses speed and drops into a pocket that stops a
  roulette: small payouts, or a JACKPOT CHANCE with its own lottery (bingo, step-up, ball-drum). Jackpots are
  progressive: they grow with medals inserted. [Konami fandom][ft] [ja.wikipedia FT][ftja] [medal guide][ft4]
- **Closed-loop balls & bonus lights (Palm Fun LARVA / COINTOPIA)**: a fixed number of special balls always
  sits on the pusher plate; each ball pushed off lands on a turntable for tickets / mini-games; lighting all
  channel lights or collecting B-O-N-U-S holes triggers a big coin dump or super wheel. [Larva][larva] [Larva 4P][larva4] [Cointopia][cointopia]
- **Shared progressive pool & coin dumps**: 4-player machines share one jackpot; "Mega Coin Drop" dumps
  coins onto the plate for a cascade. [Larva 4P][larva4] [Volley Coin][volley]

### 0.3 香港機舖 Hong Kong arcades
- 美國冒險樂園 Jumpin Gym USA (since 1994): token games, 推金幣機, 夾公仔, 掟彩虹; tokens ~HK$2 each, tickets
  stored on a member card and exchanged at the 禮品換領處 prize counter for snacks, plush toys, appliances.
  [Pioneer Centre][jg1] [V City][jg2] [Whampoa][jg3]. nobi nobi 油塘 has 推銀機 + a giant coin tower. [kkday][nobi]
- What we keep from the HK feel: the counter where tickets become prizes, the coin "wall" hanging over the lip,
  the chime of the payout tray, lucky-rainbow colours, local prize names (魚蛋、雞蛋仔、叮叮、天星小輪…).

### 0.4 對我哋嘅意思 What this means for the game
- Real machines are tuned for the house (30–50 %). **Ours pays like a real machine, ~50 % long-run**
  including bonuses, but never strands you: slow regen, a rescue when empty, and a play-time reward.
- The magic is the **pile physics** (coins only move when pushed; stacks; coins hanging over the edge) and
  **bonus anticipation** (slot spinning, ball creeping to the edge, jackpot funnel).
- We copy the *genre*, not any machine: no names, art, sounds or layouts from Konami, Sega, Palm Fun etc.

[wiki]: https://en.wikipedia.org/wiki/Coin_pusher
[cp]: https://coinpusher.com/how-do-coin-pushers-work/
[p5752699]: https://exa.ai/library/legal/patent/b02t00hdj6b
[gb2378906]: https://patents.google.com/patent/GB2378906
[jp3105617]: https://www.goveda.com/patent/JP-3105617-B2
[chucker]: https://trea.com/information/chucker-with-slits-and-medal-game-device/patentapplication/5bc0a51e-95d2-47c8-9987-9077689f2e64
[p5713572]: https://exa.ai/library/legal/patent/wnz61ck2xgd
[p5507490]: https://exa.ai/library/legal/patent/gkbzhqn9wfv
[ft]: https://konami.fandom.com/wiki/Fortune_Trinity:_Jik%C5%AB_no_Diamond
[ftja]: https://ja.wikipedia.org/wiki/FORTUNE_TRINITY
[ft4]: https://medaru.po-ka-ge-mu.com/ft4/
[larva]: https://www.palm-fun.com/product/larva-series-coin-pusher-ball-drop-arcade-ticket-redemption-machine.html
[larva4]: https://www.palm-fun.com/article/larva-4-player-arcade-pusher-the-next-big-hit-in-amusement.html
[cointopia]: https://www.yutogames.com/product/cointopia-coin-pusher-arcade-machine/
[volley]: https://www.palm-fun.com/product/volley-coin-pusher-arcade-ticket-redemption-machine.html
[jg1]: https://kidsouting.com/zh-HK/listings/jumpin-gym-usa-pioneer-centre
[jg2]: https://kidsouting.com/zh-HK/listings/jumpin-gym-usa-v-city
[jg3]: https://kidsouting.com/zh-HK/listings/jumpin-gym-usa-whampoa-garden
[nobi]: https://www.kkday.com/zh-tw/product/157815

---

## 1. 概念 Concept
一部放喺你手掌入面嘅推銀機。由落幣口揀位落幣，睇住推台將成堆金幣推到邊緣，一跌就係你嘅；
瞄準後牆左右走嘅 **CHANCE 閘** 就轉老虎機，推跌彩球就轉大轉盤，7-7-7 就有 **JACKPOT CHANCE**。
贏到嘅獎票（tickets）去 **換領處** 解鎖新機台（主題）同收集獎品。

A pocket coin pusher: aim the dropper, drop coins, watch the plate shove the pile until coins tumble over
the lip into your tray. Hit the moving CHANCE gate for slot spins, push the bonus balls off for the wheel,
line up 7-7-7 for the jackpot funnel. Spend tickets at the prize counter on new machines and collect prizes.

## 2. 核心循環 Core loop
1. 揀位（UP/DOWN）→ 落幣（OK）→ 幣經過針板落到推台上。
2. 推台退後時後牆刮幣落場；推台向前推，成堆幣慢慢向前；跌出前面 = 返入你銀包，跌落兩邊坑 = 冇咗。
3. 幣經過 CHANCE 閘 → 老虎機（可以儲 4 次 保留）；彩球跌落前面 → 大轉盤；獎品跌落 → 收藏 + 獎票。
4. 獎票 → 換領處：新機台、集齊一套獎品有額外獎票。
5. 幣唔夠？慢慢回復、空機救濟、玩夠 5 分鐘有獎——永遠唔會卡死。

## 3. 三粒掣控制 Controls (3 keys, one read at a time)
The keys share one resistor ladder: the board reads **one key at a time** (no combos). Same feel as the other games.

| Where | Key | Action |
|---|---|---|
| Machine | hold UP / DOWN | move the dropper left / right (slow for the first 0.15 s, so a tap nudges) |
| Machine | tap OK | drop one coin |
| Machine | hold OK > 0.3 s | rapid fire, 4 coins / s |
| Machine | hold the dropper against either end for 1.5 s | pause menu (a bar fills under the tray) |
| Menus | UP / DOWN / OK | move / choose (KEYS swaps UP/DOWN) |
| Title | RESET PROGRESS | needs a second OK; moving the cursor cancels |

Rejected: double-tap (adds 0.2–0.3 s delay to every tap); auto-swinging dropper (removes the skill of
leading the CHANCE gate).

## 4. 機制 Mechanics

### 4.1 場地 Field (field units: 1 unit = 1 px at the front lip)
- Field 200 wide × 150 deep. Back wall at depth 0, front lip at 150.
- Pusher plate: front face oscillates between depth 34 and 66, period 2.6 s (cosine ease). Plate top holds
  coins; the back wall stops them, so a retracting plate drops them in front of its face.
- Side walls from the back to depth 70; past that the sides are open **gutters** (lost coins).
- The front lip pays only in its middle 96 units; the corners of the lip are **lose holes** (as on many real
  machines). Tuning sweep: shorter walls alone bottom out at ~73 % payout; the narrower pay window brings it to
  ~50 % (PAY_W 140 → 63 %, 120 → 60 %, 100 → 56 %, 90 → 53 %; 96 chosen with slightly trimmed rewards).
- Coins: discs radius 8. Bonus balls radius 12 (mass 5). Prize capsules radius 11 (mass 6).

### 4.2 物理 Physics (quasi-static, fixed-point on the board)
- Coins only move when pushed (friction): **no velocities**, just positions.
- Per 1/30 s step: move plate coins with the plate (clamped by the back wall) → stacked coins ride on their
  support → 3 iterations of: plate face pushes field objects to `face + r`, pairwise overlap resolution
  (55 % of the overlap per iteration, split by mass), side walls → plate coins past the face fall onto the
  field → lip and gutters.
- **Stacks**: on the last iteration, two coins overlapping by > 42 % of their diameter make the rear coin
  ride up onto the front one (height 1, max one level). A stacked coin falls when nothing is under it.
  This is what gives "coins hanging over the edge" and compaction.
- Neighbour search: sort by x, sweep (array is nearly sorted each step, so insertion sort is O(n)).
- Board: positions in Q4 fixed point (1/16 unit, int16), squared distances in int32, `isqrt` only for pairs
  that overlap. The ESP32-C3 has **no FPU**, so no floats in the loop.
- Prototype numbers: 35 µs per step on a desktop browser for ~140 objects; pool max 141/160 over 20 min.

### 4.3 落幣 Drop
- A coin takes 0.45 s to fall through the pin board; pins add ±6 units of random spread; lands on the plate
  at a random depth.
- **CHANCE gate**: a 16-unit window slides along the back wall (sine, ~1.3 rad/s). A coin passing it halfway
  down its fall starts a slot spin. Random aim ≈ 8 % of coins; aiming ahead of the gate hits nearly always
  (prototype test: 6/6) — the skill element.

### 4.4 老虎機 Slot (3 reels, up to 4 pending spins 保留)
| Result | Chance | Reward |
|---|---|---|
| 🍒🍒🍒 | 15 % | +2 coins (coin shower onto the plate) |
| 🔔🔔🔔 | 7 % | +5 coins |
| ★★★ | 4 % | spin the big wheel |
| 7 7 7 | 1.5 % | **JACKPOT CHANCE** |
| 💰💰💰 | 6 % | +10 tickets |
| miss | 66.5 % | — (no "near-miss" engineering beyond normal random reels) |

### 4.5 彩球 + 大轉盤 Bonus balls + wheel
- 2 balls (3 on the candy machine) live on the field in a **closed loop**: a ball pushed off the front spins
  the wheel and re-enters at the back of the plate; a ball in a gutter just re-enters.
- Wheel, 12 segments: coins 3/4/2/6/4/10 and tickets 5/10/15/5/20/5. On the device the wheel is drawn
  edge-on: a strip of segments sliding under a pointer (cheap to draw, no trig per pixel).

### 4.6 JACKPOT CHANCE 漏斗
- The ball circles a funnel and drops into a pocket: 20 coins (45 %), 30 (30 %), 50 (18 %), **JP** (7 %).
- JP pays the progressive pool: starts at the machine's base (300–600) and grows 0.1 coin per coin dropped.

### 4.7 獎品 Prizes
- 2 prize capsules on the field at a time, from the current machine's set of 6 (24 in total).
- Pushed off the front: +1 in the collection, +5 tickets. Gutter: lost, a new one drops in later.
- Completing a machine's set of 6: +100 tickets once.

### 4.8 唔會卡死 Never stuck
- **Regen**: +1 coin every 30 s of play while the bank is under 50.
- **Rescue**: bank 0 and nothing in flight or spinning for 8 s → +20 coins.
- **Play-time reward** (replaces the daily bonus; the board has no clock): every **5 minutes of active
  play** (a key pressed within the last 20 s) gives **+30 coins**. Idle time does not count.
- **Payout ~50 % long-run, like a real machine** (incl. slot / wheel / jackpot). Measured with random aim and
  rapid fire: firmware host harness 1 h → 54.4 % (front lip 39 %, gutters + corner holes 72 % of coins dropped;
  20-min seeds 48 / 48 / 68 / 47 / 51 %, the 68 % run hit a jackpot); prototype 20 min seeds 46–59 %.
  A player who leads the CHANCE gate gets ~56 % (twice the slot spins).
- So at 4 coins/s a cup of 100 lasts ~50 s of rapid fire; at a relaxed pace with regen + the play reward it
  never runs dry for long.

## 5. 進度 Progression: 換領處 Prize counter
| Machine | Cost | Twist | Prize set |
|---|---|---|---|
| 海旁夜市 Harbour Night | free | — | 魚蛋、雞蛋仔、紅白藍袋、叮叮、天星小輪、霓虹燈 |
| 糖果工場 Candy Factory | 150 tickets | 3 bonus balls | 波板糖、棉花糖、朱古力、啫喱、薄荷糖、金幣朱古力 |
| 太空站 Space Station | 400 tickets | CHANCE gate 1.5× wider but 1.7× faster | 火箭、衛星、太空人、外星仔、月球、土星 |
| 龍宮寶藏 Dragon Palace | 900 tickets | JP base 600 | 珍珠、珊瑚、玉佩、金魚、龍珠、寶箱 |

Each machine has its own palette and lamp colours. The field (coins, balls, prizes) carries over when you
switch, like walking to another machine with your cup of coins. Collection screen: 4 × 6 grid with counts.

## 6. 美術 / 聲音 Art & audio (all original, procedural)
- Pseudo-3D: field drawn as a trapezoid (scale 0.72 at the back → 1.18 at the front, linear depth), objects
  sorted back to front, coins as gold ellipses with rim, face and highlight; stacked coins raised 3 px.
- Top display: 3 slot reels / the wheel through a window / the jackpot funnel; JP counter; chasing lamps.
- Back wall with the moving CHANCE window and 4 pending-spin lamps; dropper nozzle; payout tray that flashes.
- Sound: reuse the heist/fire-brigade square/triangle synth: coin drop click, payout clink (rate-limited),
  slot ticks, wheel clicks slowing down, jackpot fanfare. No sampled audio, no music from any machine.

## 7. 技術架構 Tech architecture
### 7.1 平台 Platform (same as the other three games)
- ESP32-C3 (RISC-V 160 MHz, **no FPU**, ~400 KB SRAM, no PSRAM), 320×240 SPI LCD, 3 keys on one ADC ladder.
- ESP-IDF v6, installs into **`ota_1`** (0x400000) next to the assistant in `ota_0`; `tools/flash.sh` only
  writes the app (copied from the heist). Never `idf.py flash`. BACK TO ASSISTANT in the pause menu.
- Reuse (copy, don't modify the other repos): board/display/input/gfx/sound/boot from ai-passport-heist.

### 7.2 30 fps 重點 Frame rate
- The heist measured a full 320×240 frame at **31–36 ms over 40 MHz SPI** — a full redraw cannot hold 30 fps.
  So the pusher redraws **only the bands that change**: the field (rows ~56–206, ~62 % of the screen) every
  frame (~19 ms SPI), the top display only while a reel/wheel/funnel moves, the HUD on change.
- Physics ~3–4k pair checks per step in integer math; target < 3 ms per step on the C3.
- Perspective in integer maths (scale 184..302 /256); ellipses from a precomputed half-width table for
  coin sizes (832 B) and an integer sqrt for the bigger balls and capsules.
- Built: the field window 288×158 (x 16–304, y 50–208) goes every frame = 88 KB, 18.2 ms at 40 MHz; the
  HUD + display strip (31 KB, 6.4 ms) only while a reel / wheel / funnel moves or a number changes; the tray
  (20 KB) on change. Worst case (bonus spinning) ≈ 25 ms of SPI per frame.
- **Coin pool fallback**: compile with `-DMAX_OBJ=120`, and at run time the game caps the pool at 120 by itself
  if step + render average more than 30 ms over 10 s (rewards then go to the bank instead of the plate).
- Fixed 1/30 s step with up to 3 catch-up steps; logs fps / late frames / heap every 10 s (as the heist).

### 7.3 記憶體 Memory (measured: `idf.py size`, see §12)
| Item | Size |
|---|---|
| Object pool 160 × 12 B (x, y, prev x, prev y int16 Q4; type, layer/stack, support idx, prize id) | 1.9 KB |
| Sort index + 3 work lists (uint8) | 0.6 KB |
| Drops in flight 32 × 6 B, popups 4 × 24 B, slot/wheel/jackpot/game state | 0.5 KB |
| Coin sprites, 8 sizes, pre-shaded RGB565 | ~4 KB |
| Depth LUT 150 × 4 B | 0.6 KB |
| Band buffers 2 × 320×40×2 (same as the other games) | 50 KB |
| **Game total (static)** | **~58 KB** of ~320 KB DRAM; no malloc after start-up |
| Code (estimate from the heist: 43 KB game code) | ~40–50 KB flash |

### 7.4 儲存 NVS namespace `pusher`
- Blob `prog` (~60 B): version, bank, tickets, jackpot ×10, theme, owned bits, set bits, sound/keys,
  regen timer, play-time timer, collection counts (24 × uint8). Written only when it changed.
- Blob `field` (8 B + 6 B per object, ~500–970 B): pusher phase + every object (type/stack/prize 1 B, x/y int16 Q4) → the pile is
  exactly where you left it after a reboot, like a real machine.
- NVS is the assistant's shared 48 KB partition: save `prog` on change at most every 10 s, `field` only on
  pause / title / back-to-assistant / every 2 min of play. Coins in flight are returned to the bank in the
  saved blob, so a power cut loses nothing. Never erase NVS.

## 8. 檔案結構 File layout
```
ai-passport-coincarnival/
├── PLAN.md
├── prototype/index.html      single-file browser prototype (done)
├── tools/proto_test.sh/.js   39 headless checks (done)
├── main/                     board/display/input/boot (copied from the heist), gfx.c (windowed bands),
│                             sound.c (new sfx + carnival loop), config.h, field.c (Q4 physics),
│                             machine.c (rules, controls, save blobs), art.c, game.c (states, NVS, redraw)
└── tools/flash.sh, tools/shots/ (host harness + screenshots), partitions.csv (assistant layout)
```

## 9. 原型 Prototype (done)
`prototype/index.html` — one file, no libraries, 320×240 canvas ×3. Contains: physics (plate, back wall,
stacks, gutters, lip), dropper with pin spread, rapid fire, CHANCE gate, slot with 4 pending spins, closed-loop
bonus balls + wheel, jackpot funnel with progressive pool, prizes + collection + set bonus, prize counter with
4 machines, regen / rescue / play-time reward, corner lose holes, pause by edge-hold, save/load to localStorage key `pusher`
(stand-in for NVS), reset with two OKs, simple WebAudio sound. `tools/proto_test.sh` runs 39 checks in
headless Chrome, including a 20-minute autoplay with a coin ledger that must balance exactly.

## 10. 里程碑 Milestones
1. **M0** research + plan + prototype + headless tests. ✅
2. **M1** firmware skeleton from the heist (display/input/sound/boot/flash.sh), field physics in fixed point,
   host harness that replays the prototype's checks (ledger, payout, pool). ✅
3. **M2** machine features: gate, slot, wheel, jackpot, prizes; partial-band renderer ✅; measure fps on device.
4. **M3** counter, collection, themes, NVS save of the pile, regen/rescue/play reward; sound.
5. **M4** tuning on the board (payout, feel), README + screenshots, flash with tools/flash.sh.

## 11. 未解決問題 Open questions
Decided: payout ~50 % (real-machine), play-time reward instead of daily, name 金幣嘉年華 Coin Carnival, HK
prize names. Still open:
1. **Device frame time**: the C3's step + render cost still has to be read from the serial log (fps, step avg,
   render avg, objs n/cap every 10 s). Host physics is 35 µs/step on a PC.
2. **SPI**: is 80 MHz LCD clock stable on this panel? It would halve the per-frame SPI time.
3. **Stacks deeper than 2**: worth the cost, or is one level enough visually?
4. **Tickets → anything outside the game?** Proposed: no. Tickets only unlock themes and the collection.
5. **Payout feel**: 50 % with 4 coins/s rapid fire drains a cup fast; if it feels stingy on the device, the
   knobs are `PAY_W` (pay window) and the slot / wheel tables in `main/config.h` / `machine.c` (the prototype
   has the same names).

## 12. 韌體 Firmware (built)
- Same pattern as the heist / fire brigade: ESP-IDF v6, `ota_1`, signed with the assistant's dev key,
  `tools/flash.sh` writes only the app (and with `--boot` switches the boot slot).
- Host harness `tools/shots/make_shots.sh` (cc + python3 only): 33 checks on the firmware's own code (controls,
  drop timing, ledger over 1 h, payout, bonuses, jackpot, regen / rescue / play reward, prizes and sets,
  themes, save / load, NVS sizes, pool fallback) + screenshots into `docs/`.
- `idf.py size` (box build): image 402,646 B (bin 0x71000, 89 % of the slot free); DRAM used 122,106 B of
  321,296 (.bss 62,232); game code (`libmain.a`) 28.5 KB code + 2.6 KB rodata + 57.9 KB RAM, of which 51.2 KB
  are the two band buffers and 2.7 KB the whole machine (`machine_t`). No malloc after start-up.
