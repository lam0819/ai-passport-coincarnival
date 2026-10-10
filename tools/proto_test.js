// Headless rule checks for prototype/index.html (run tools/proto_test.sh).
// Writes its report into #out; the last line is "all checks passed" on success.
(function () {
  const out = [], errs = [];
  let fails = 0;
  const ok = (c, m) => { out.push((c ? 'ok   ' : 'FAIL ') + m); if (!c) fails++; };
  window.addEventListener('error', (e) => errs.push(e.message));
  const H = window.__CP;
  try {
    H.stop();
    const S = H.S, C = H.C;
    const st = (n) => { for (let i = 0; i < n; i++) H.step(C.DT); };
    const tap = (k) => { H.setKey(k, true); H.step(C.DT); H.setKey(k, false); H.step(C.DT); };
    const count = (t) => S.objs.filter((o) => o.t === t).length;

    H.render();
    ok(S.mode === 'title', 'title screen renders');
    ok(count(H.T_COIN) >= 80 && count(H.T_BALL) === 2 && count(H.T_PRIZE) === 2,
       'fresh machine: ' + count(H.T_COIN) + ' coins, 2 bonus balls, 2 prizes on the field');
    ok(S.bank === C.START_BANK, 'starts with ' + C.START_BANK + ' virtual coins');

    tap('ok');
    ok(S.mode === 'play', 'OK on PLAY starts the machine');
    let b0 = S.bank, d0 = S.stats.dropped;
    tap('ok');
    ok(S.stats.dropped === d0 + 1 && S.drops.length === 1, 'tap OK drops exactly one coin');
    st(15);
    ok(S.drops.length === 0, 'the coin lands after ' + C.DROP_TIME + ' s');

    d0 = S.stats.dropped;
    H.setKey('ok', true); st(60); H.setKey('ok', false); st(1);
    const rapid = S.stats.dropped - d0;
    ok(rapid >= 6 && rapid <= 8, 'hold OK 2 s: rapid fire (' + rapid + ' coins, 4 per second after 0.3 s)');

    const a0 = S.aim;
    H.setKey('down', true); st(15); H.setKey('down', false); st(1);
    ok(S.aim > a0 + 20, 'hold DOWN moves the dropper right (' + a0.toFixed(0) + ' -> ' + S.aim.toFixed(0) + ')');
    const a1 = S.aim;
    tap('up');
    ok(S.aim < a1 && a1 - S.aim < 3, 'a short tap nudges the dropper (fine aim)');

    H.setKey('up', true); st(30 * 4); H.setKey('up', false); st(1);
    ok(S.mode === 'pause', 'holding the dropper against the edge for 1.5 s opens the pause menu');
    tap('ok');
    ok(S.mode === 'play', 'RESUME returns to the machine');

    // gate: aim where the gate will be when the coin passes it
    let hits = 0;
    for (let i = 0; i < 6; i++) {
      st(10);
      const save = S.gateT;
      S.gateT += C.DROP_TIME * 0.5 * C.GATE_SPEED * H.THEMES[S.theme].gateSpeed + C.DT * C.GATE_SPEED;
      const target = H.gateX(); S.gateT = save;
      S.aim = target;
      const q = S.slotQ + S.stats.slot;
      H.dropCoin(); st(10);
      if (S.slotQ + S.stats.slot > q) hits++;
    }
    ok(hits >= 4, 'aiming ahead of the moving CHANCE gate starts the slot (' + hits + '/6)');
    st(30 * 3);

    // physics invariants after normal play
    let bad = 0, nan = 0;
    for (const o of S.objs) {
      if (!isFinite(o.x) || !isFinite(o.y)) nan++;
      if (o.layer === 0 && o.h === 0 && o.y < S.pushY + o.r - 0.01) bad++;
    }
    ok(nan === 0 && bad === 0, 'no object behind the plate face, no NaN positions');

    // long run: autoplay 20 game-minutes, aiming at random and dropping 4 per second
    H.seed(42);
    S.bank = 5000;   // a big budget so the run measures the payout rather than an empty bank
    const st0 = JSON.parse(JSON.stringify(S.stats)), bank0 = S.bank, mach0 = H.coinsInMachine();
    let maxObj = 0, maxOv = 0, ovSum = 0, ovN = 0, t0 = performance.now(), aimT = 0;
    const steps = 30 * 60 * 20;
    for (let i = 0; i < steps; i++) {
      if (i % 30 === 0) aimT = 15 + Math.random() * (C.FW - 30);
      S.aim += Math.max(-4, Math.min(4, aimT - S.aim));
      if (i % 8 === 0 && S.bank > 0) H.dropCoin();
      H.step(C.DT);
      if (S.objs.length > maxObj) maxObj = S.objs.length;
      if (i % 300 === 0) {
        const f = S.objs.filter((o) => o.layer === 0 && o.h === 0);
        for (let a = 0; a < f.length; a++) for (let b = a + 1; b < f.length; b++) {
          const ov = f[a].r + f[b].r - Math.hypot(f[a].x - f[b].x, f[a].y - f[b].y);
          if (ov > 0) { ovSum += ov; ovN++; if (ov > maxOv) maxOv = ov; }
        }
      }
    }
    const ms = (performance.now() - t0) / steps;
    const ds = (k) => S.stats[k] - st0[k];
    const dropped = ds('dropped'), won = ds('won'), lost = ds('lost');
    ok(dropped > 3000, '20 min autoplay: ' + dropped + ' coins dropped');
    const lhs = S.bank + H.coinsInMachine() + lost, rhs = bank0 + mach0 + ds('minted') + ds('gifts');
    ok(lhs === rhs, 'coin ledger balances (bank + machine + gutters = start + rewards + gifts: ' + lhs + ' = ' + rhs + ')');
    const rtp = (won + ds('minted')) / dropped;
    ok(won / dropped > 0.25 && won / dropped < 0.55, 'front lip pays '  + (100 * won / dropped).toFixed(0) + '% of coins dropped; gutters and corner holes take ' + (100 * lost / dropped).toFixed(0) + '%');
    ok(rtp > 0.4 && rtp < 0.6, 'long-run payout incl. slot / wheel / jackpot: ' + (100 * rtp).toFixed(0) + '% of coins dropped (target ~50%, like a real machine)');
    ok(ds('slot') > 10 && ds('roul') > 3 && ds('prizes') > 1,
       'bonuses fire: ' + ds('slot') + ' slot spins, ' + ds('roul') + ' wheel spins (' + ds('ballsOff') + ' balls), ' + ds('jpc') + ' jackpot chances, ' + ds('prizes') + ' prizes');
    ok(maxObj <= C.MAX_OBJ, 'object pool never exceeds ' + C.MAX_OBJ + ' (max ' + maxObj + ')');
    ok(ovN === 0 || ovSum / ovN < 3, 'field coins mostly separated (mean overlap ' + (ovN ? (ovSum / ovN).toFixed(2) : 0) + ' units, max ' + maxOv.toFixed(1) + ')');
    ok(count(H.T_BALL) + S.ballWait.length + S.drops.filter((d) => d.t === H.T_BALL).length === H.THEMES[S.theme].balls,
       'bonus balls run in a closed loop (still ' + H.THEMES[S.theme].balls + ')');
    out.push('info host physics ' + (ms * 1000).toFixed(0) + ' µs per step with up to ' + maxObj + ' objects');

    // jackpot
    const calm = () => { S.slot = S.roul = S.jpc = null; S.slotQ = 0; S.shower = 0; S.drops.length = 0; S.ballWait.length = 0;
      for (const o of S.objs) { o.y = Math.min(o.y, 40); o.layer = 1; } };   // nothing about to fall
    calm();
    S.jp = 512.3;
    H.startJpc(); S.jpc.pocket = 3;
    const jw = S.stats.jpwin, m0 = S.stats.minted;
    st(30 * 4);
    ok(S.stats.jpwin === jw + 1 && S.stats.minted - m0 === 512 && S.jp === H.THEMES[S.theme].jp, 'jackpot pays the pool (512) and resets it');

    // never stuck: regen, rescue, daily bonus
    calm();
    S.bank = 10; S.regenT = 0; const g0 = S.stats.gifts;
    st(30 * 31);
    ok(S.bank >= 11 && S.stats.gifts > g0, 'coins refill slowly while below ' + C.REGEN_CAP + ' (one per ' + C.REGEN_S + ' s)');
    calm(); S.bank = 0; S.objs = S.objs.filter((o) => o.t !== H.T_COIN);   // an empty machine
    let rescued = false;
    for (let i = 0; i < 30 * 12 && !rescued; i++) { H.step(C.DT); if (S.bank >= C.RESCUE) rescued = true; }
    ok(rescued, 'an empty bank with nothing in play gets a rescue of ' + C.RESCUE + ' coins');
    // play-time reward: every 5 min of active play
    calm(); S.bank = 500; S.playT = 0; const pr = S.stats.playRewards;
    for (let i = 0; i < 30 * 301; i++) { if (i % 150 === 0) { H.setKey('down', true); H.step(C.DT); H.setKey('down', false); } H.step(C.DT); }
    ok(S.stats.playRewards === pr + 1, 'every ' + C.PLAY_REWARD_S / 60 + ' min of active play gives +' + C.PLAY_REWARD + ' coins');
    S.playT = 0;
    st(30 * 320);
    ok(S.stats.playRewards === pr + 1, 'idle time does not count (no key for ' + C.ACTIVE_S + ' s)');
    // save / load (NVS 'pusher' on the board)
    S.tickets = 37;
    H.save();
    const snap = { bank: S.bank, tickets: S.tickets, n: S.objs.length, jp: Math.round(S.jp) };
    H.load();
    ok(S.bank === snap.bank && S.tickets === 37 && S.objs.length === snap.n && Math.round(S.jp) === snap.jp,
       'save + reload keeps coins, tickets, jackpot and the whole field (' + snap.n + ' objects)');
    ok(S.mode === 'title', 'reload starts on the title');

    // prize counter: unlock with tickets, locked stays locked
    S.tickets = 200;
    S.menu = 1; tap('ok');
    ok(S.mode === 'counter', 'PRIZE COUNTER opens');
    S.menu = 2; tap('ok');
    ok(!(S.owned & 4) && S.tickets === 200 && S.msg.startsWith('NEED'), 'a machine you cannot afford stays locked');
    S.menu = 1; tap('ok');
    ok((S.owned & 2) && S.tickets === 50 && S.theme === 1, 'CANDY FACTORY unlocked for 150 tickets and selected');
    S.menu = H.menuItems().length - 1; tap('ok');
    S.menu = 0; tap('ok');
    st(30 * 3);
    S.roul = null;
    st(30 * 2);
    ok(count(H.T_BALL) + S.ballWait.length + S.drops.filter((d) => d.t === H.T_BALL).length === 3, 'candy machine plays with 3 bonus balls');
    H.render();

    // collection
    const t0c = S.tickets;
    for (let i = 0; i < 6; i++) H.collect(6 + i);
    ok(S.setsDone & 2 && S.tickets === t0c + 6 * 5 + 100, 'a full prize set gives +100 tickets once');
    H.collect(6);
    ok(S.tickets === t0c + 7 * 5 + 100, '...and not again');

    // reset needs two OKs
    H.setKey('up', true); st(30 * 4); H.setKey('up', false); st(1);
    S.menu = 1; tap('ok');
    ok(S.mode === 'title', 'pause → TITLE');
    S.menu = 5; tap('ok');
    ok(S.armed && S.owned === 3, 'RESET asks for a second OK');
    tap('ok');
    ok(S.owned === 1 && S.tickets === 0 && S.bank === C.START_BANK && S.coll.every((c) => c === 0), 'second OK resets coins, tickets, machines and collection');

    for (const m of ['title', 'pause', 'counter', 'coll', 'play']) { S.mode = m; H.render(); }
    ok(true, 'every screen renders');
  } catch (e) {
    fails++; out.push('FAIL exception: ' + (e && e.stack || e));
  }
  ok(errs.length === 0, 'no JavaScript errors' + (errs.length ? ': ' + errs.join(' | ') : ''));
  out.push(fails ? 'SOME CHECKS FAILED' : 'all checks passed');
  document.getElementById('out').textContent = out.join('\n');
})();
