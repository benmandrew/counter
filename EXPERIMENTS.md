# Experiments

A dated log of `counter` parameter experiments, newest first. Each entry records
**what changed**, **why**, and **what it found**. Raw data lives in the matching
gitignored `experiments-DD-MM-YYYY/` directory; analysis is `scripts/analyse.ipynb`.

The outcome metric throughout is `implies_ideal` — the fraction of runs producing
at least one repair *equivalent to* or *stronger than* an ideal.

---

## 2026-07-23 — Well-separation × output assumptions on TLSF

**What changed.** Sweep W crosses two new switches — `run_well_separation` (the
*well-separation* filter, which drops any candidate whose assumptions the system
can force false) and `allow_output_assumptions` (admitting output atoms into the
assumption-mutation pool, previously input-only) — each on and off, a 2×2 over
five unrealizable TLSF specs. NSGA-II, generations=10 / population_size=200.
humanoid-531 dropped (~13 min per run, no metric headroom). **Why:** a
specification is *not* well-separated exactly when `(assumptions) → false` is
realizable — the system can drive its own assumptions false and satisfy the spec
vacuously. The filter (`fe9c636`) was found inert on the FRETISH corpus: FRETISH
assumptions are input-only, hence well-separated by construction, so nothing is
ever dropped ([[well-separation-inert-on-fretish]]). The filter only *does*
anything once output atoms can appear in assumptions (`c968211`), and the two
switches are coupled — output assumptions without the filter admit the vacuous
self-falsifying repairs the filter exists to reject. `arbiter`, a two-client
mutex with transient requests and no `ASSUME` section, was built as the
discriminating case: guarantee-only repair cannot make it realizable, an
output-referencing assumption can, and only *some* such assumptions are
well-separated. The question is whether the filter cleanly separates the genuine
reactive-environment repairs from the vacuous ones.

**Run.** Seed-major across av2 (seeds 0–159) and av3 (160–319), **6,399 runs**
(3,200 + 3,199), completed 2026-07-23. Against a 12 h-per-box budget both ran
slightly over — av2 745 min, av3 811 min — but to completion, so all 320 seeds
are present rather than a truncated design. `jobs=1` per machine; timeout caps
per spec (lift 600 s, gyro-var1 120, the rest 60). The lift cap was raised
mid-campaign from 180 s (6f0dfbe) after a 3-seed calibration missed lift's heavy
tail; even at 600 s a handful of extreme lift seeds still censor. Yield: **17
timeouts** (0.27%, all lift's tail) and **one crash** — gyro-var1 seed 180, an
uncaught-exception abort, one run in 6,400, leaving 6,399 rows. Data:
`experiments/results-wellsep.csv`. The design is **paired** — all four arms share
the (spec, seed) — so every test below is matched McNemar.

### Result: a precise guard, inert until output assumptions fire

The four arms are nearly identical on `found_repair` except for one that jumps to
100%, and the entire difference sits on a single spec.

| arm (well-sep / output-assum) | `found_repair` | `arbiter` `found_repair` |
|---|---|---|
| off / off (baseline) | 0.799 | **0.00** |
| off / **on** | **1.000** | **1.00** |
| **on** / off | 0.799 | 0.00 |
| **on** / **on** | 0.799 | 0.00 |

Every other spec is `found_repair` = 1.00 in all four arms; only `arbiter` moves.
Admitting output assumptions makes `arbiter` repairable on *every* run (0/320 →
320/320, McNemar p≈9e-97) — and turning the well-separation filter on removes
*every one* of those repairs again (320/320 → 0/320, p≈9e-97, all 320 discordant
one way). The filter's `n_dropped` averages 0.04 across the corpus and its
wall-time cost is ~1 s at the median (6.2 s → 7.7 s): it touches nothing except
the output-assumption candidates it was built to reject, and on `arbiter` it
rejects all of them. Not one of the 320 output-assumption repairs `arbiter` finds
at this operating point is well-separated — each is the system forcing its own
assumption false.

### The filter also repays the tax output assumptions levy on lily02

`implies_ideal` is carried entirely by **lily02** (every other spec sits at 0 in
all four arms — the ideal is out of reach at gen10/pop200 regardless). There,
admitting output assumptions is a small quality *loss*, and the filter reverses
it.

| arm | lily02 `implies_ideal` |
|---|---|
| off / off (baseline) | 319/320 |
| off / **on** | 301/320 |
| **on** / off | 319/320 |
| **on** / **on** | 316/320 |

Output-assumptions-on drops lily02 by 18 net (319 → 301, discordance +1/−19,
p=4e-5): the extra vacuous variants dilute the population and cost coverage.
Turning the filter on recovers 15 of them (301 → 316, +18/−3, p=0.0015), leaving
the full configuration statistically indistinguishable from baseline (319 vs 316,
+4/−1, p=0.38, ns). The filter makes output assumptions *safe* — it neutralises
both effects they introduce, the false-positive inflation on `arbiter` and the
coverage tax on `lily02`.

### What this campaign cannot answer

- **The genuine repair was never reached.** `arbiter`'s well-separated repair
  (`G(r → F g)`, a reactive-environment assumption the filter would *keep*) does
  not appear at gen10/pop200 — the search only ever finds the vacuous cheats. So
  the campaign shows the filter correctly rejecting bad repairs, not yet keeping a
  good one. A higher-budget `arbiter` run is the follow-up.
- **No corpus headroom for the upside.** None of the five specs' ideals require an
  output-referencing assumption, so output assumptions unlock no `implies_ideal`
  here; their value would show only on a spec whose ideal is reactive-shaped.
- **NSGA-II, gen10/pop200 only.**

### Method notes worth keeping

- **The two switches must be read as a pair.** `allow_output_assumptions` on its
  own is a false-positive machine (`found_repair` 0.80 → 1.00 with junk repairs);
  it is only sound with the well-separation filter on. Never ship the first
  without the second.
- **A filter inert on one input class is not inert on another.** Well-separation
  drops nothing on FRETISH (input-only assumptions) and is decisive on TLSF once
  output assumptions are admitted — the same code, opposite verdicts, decided by
  whether assumptions can reference an output ([[well-separation-inert-on-fretish]]).
- **Calibrate the timeout on the slow spec's tail, not its median.** lift's 3-seed
  calibration set a 180 s cap that then censored the heavy tail; 600 s cut it to
  0.27% but did not eliminate it ([[calibration-saturation-bias]]).

### Scripts and launch

```sh
python scripts/gen_configs.py --tlsf --sweeps W \
    --out-dir experiments/configs-wellsep
# av2                                                # av3
… --profile wellsep --jobs 1 --seeds $(seq 0 159)   … --seeds $(seq 160 319)
python scripts/merge_experiments.py --profile wellsep av2 av3   # python3 on av2
```

**Verdict: the well-separation filter is a correct, targeted safeguard, and
output assumptions are unsafe without it.** On the current corpus it is pure
insurance — it rejects the vacuous repairs output assumptions open up (all 320 on
`arbiter`) and repays their coverage tax (`lily02` back to baseline), while
touching nothing else. Enable both together or neither; whether the filter ever
*keeps* a genuine reactive-environment repair is the higher-budget `arbiter`
question, not one this operating point can reach.

---

## 2026-07-22 — `p_add_assumption`: the lever for MUC's ideal-hit rate

**What changed.** Sweep P: `p_add_assumption` ∈ {0.05, 0.15, 0.3, 0.5} — the
fixed-rate structural mutation that *creates* an assumption — crossed with
`repair_mode` (monolithic vs muc) over five TLSF specs. humanoid-531 was dropped:
it has no metric headroom in either arm and costs ~13 min per run. NSGA-II,
generations=10 / population_size=200. **Why:** the 2026-07-21 muc campaign traced
MUC's ideal-*misses* to guarantee-weakening dominating the search landscape, and
singled out `p_add_assumption` — held at its default 0.05 throughout that run — as
the one lever that could push MUC toward the ideal, since arbiter's ideal is
reachable only by *adding* the two fairness assumptions `G F r0` and `G F r1`. The
question here is direct: does raising the rate lift the ideal-hit rate, and what
does it cost?

**Run.** Seed-major across av2 (seeds 0–79) and av3 (seeds 80–159), **6,399 runs**
(3,200 + 3,199), completed 2026-07-22 in ~13 h — inside a 20 h cap that never
fired. `jobs=1` per machine; timeout caps per spec (lift 600 s, gyro-var1 300, the
rest 120). The counting-path leak fix — an `ltl2tgba` timeout plus
`PR_SET_PDEATHSIG` on every subprocess (62bbc6f) — was deployed for this run:
**zero orphaned `ltl2tgba` across the whole campaign**, free memory steady at
~121 GB per box, versus the ~93 GB of orphans the prior multi-day run accumulated.
One run dropped (gyro-var1 seed 142 at muc/padd0.3, a spec pinned to zero anyway).
Data: `experiments/results-padd.csv`. The design is **paired** — both repair modes
and all four levels share the seed — so the tests below are matched: McNemar's
exact test on the extreme levels, Cochran-Armitage for the dose trend.

### Result: a real but weak lever, and a spec-dependent tradeoff

Raising `p_add_assumption` does what the muc campaign predicted — it lifts arbiter
toward the ideal — but the gain is modest, never becomes the dominant outcome, and
degrades the one spec monolithic solves cleanly.

| metric | padd0.05 | padd0.15 | padd0.3 | padd0.5 |
|---|---|---|---|---|
| **arbiter** muc `implies_ideal` | 15/160 | 25/160 | 26/160 | **31/160** |
| **lily02** muc `implies_ideal` | 48/160 | 41/160 | 44/160 | **32/160** |
| **lily02** mono `implies_ideal` | 160/160 | 154/160 | 150/160 | **141/160** |

arbiter's muc ideal-hit rate climbs monotonically, 9.4% → 19.4%, a 2.1× rise. The
trend is significant (Cochran-Armitage z=2.31, p=0.021; extreme-level McNemar
0.05-vs-0.5 p=0.011, 26 seeds gained against 10 lost). But 0.5 still leaves 80% of
arbiter's runs short of the ideal, and pushing the rate higher trades against every
spec whose ideal is already met by weaker mutation.

### The mechanism is exactly incomparable → equivalent

The muc campaign's diagnosis was that MUC reaches realizability by weakening a
guarantee (an easily-reached basin) far more often than by adding the assumptions
the ideal needs (a narrow target). If `p_add_assumption` is the lever, raising it
should convert *incomparable* repairs (guarantee-weakenings) into *equivalent* ones
(the ideal) and nothing else. That is exactly what the `best_relation` decomposition
shows on arbiter muc:

| level | incomparable | equivalent | strictly weaker |
|---|---|---|---|
| padd0.05 | 139 | 15 | 6 |
| padd0.15 | 134 | 25 | 1 |
| padd0.3 | 132 | 26 | 2 |
| padd0.5 | **122** | **31** | 7 |

Equivalent rises as incomparable falls, roughly one for one; strictly-stronger never
appears (the ideal is the ceiling). The lever acts on the mechanism the earlier
campaign named — assumption *creation* competing with guarantee weakening — not on
some unrelated path.

### The cost is lily02, and it is lost coverage

More assumption-adding hurts lily02 in both arms. On monolithic — where every found
repair is otherwise ideal — the ideal-hit rate falls 100% → 88% (160 → 141, McNemar
p=3.8e-6), and the loss is **entirely lost coverage**: the `found_repair`
discordance is identical (+0/-19), so the spurious assumptions do not redirect
repairs off-ideal, they make ~12% of runs find no realizable repair at all. On muc,
lily02's ideal-hit drifts 30% → 20% (+25/-41, p=0.064 — a decline, not quite
significant against muc's already-scattered baseline). arbiter and lily02 pull in
opposite directions under the same knob.

### Coverage otherwise holds; four specs stay floored

MUC's `found_repair` sits at ~100% across every spec and level (arbiter 160/160
throughout, the rest ≥ 156/160), so the knob breaks coverage nowhere except mono
lily02. gyro-var1, lift and minepump remain at `implies_ideal` = 0 in both arms at
every level: their unrealizability is not addressable by adding assumptions at this
operating point, and `p_add_assumption` moves them not at all. The lever is real
but it acts on exactly one spec in the corpus.

### What this campaign cannot answer

- **There is no global optimum.** arbiter wants the rate high, lily02 wants it low,
  three specs do not care. ~0.15 captures most of arbiter's gain (→16%) at little
  lily02 cost (mono →96%); 0.5 buys the rest of arbiter (→19%) but bleeds lily02
  (mono →88%). The right value is per-spec, which a single global parameter cannot
  express.
- **Still gen10/pop200.** The four floored specs have no headroom here; whether the
  lever matters at an operating point where their ideal is reachable is unknown.
- **arbiter tops out at ~19%.** Even at 0.5 the guarantee-weakening basin dominates;
  reaching the ideal reliably would need more than nudging this one rate.

### Method notes worth keeping

- **Paired trend, not just extremes.** With all four levels sharing the seed, the
  Cochran-Armitage linear-trend test reads the monotone dose-response across levels
  (z=2.31), and McNemar the matched extreme pair; both avoid the power loss of
  treating the levels as independent groups. (No SciPy on the box — both by hand.)
- **Identical discordance is the tell.** mono lily02's `implies_ideal` and
  `found_repair` McNemar tables are the same (+0/-19), which is what pins the loss to
  coverage rather than an off-ideal redirection — worth checking whenever a quality
  metric and its coverage metric move together.
- **The leak fix held in production.** Zero orphaned `ltl2tgba` over ~13 h on both
  boxes with the counting-path timeout and `PR_SET_PDEATHSIG` in the binary
  (62bbc6f) — the mitigation janitor from the muc campaign was not needed.
  [[ltlfilt-simplify-blowup]]

### Scripts and launch

```sh
python scripts/gen_configs.py --tlsf --sweeps P --repair both \
    --out-dir experiments/configs-padd
# av2                                              # av3
… --profile padd --jobs 1 --seeds $(seq 0 79)      … --seeds $(seq 80 159)
python scripts/merge_experiments.py --profile padd av2 av3   # python3 on av2
```

**Verdict: `p_add_assumption` is arbiter's lever and lily02's tax.** Raising it
converts MUC's guarantee-weakenings into ideal repairs on `arbiter` — the mechanism
the muc campaign predicted, confirmed as a monotone incomparable→equivalent shift —
but only to ~19%, at a measurable coverage cost to `lily02` and no effect on the
rest. Keep the default (0.05) as the global value; treat a higher rate as a per-spec
knob for specs like `arbiter` whose ideal is assumption-shaped.

---

## 2026-07-21 — Monolithic vs MUC-guided TLSF repair

**What changed.** The first run of the `muc` profile: `tlsf.repair_mode` as a
crossed factor (`monolithic` vs `muc`) over the six unrealizable TLSF specs,
crossed in turn with a new TLSF assumption/guarantee *mutation split* (sweep M:
`p_guarantee` ∈ {0.3, 0.5, 0.7, 0.9}, `p_assumption` its complement). NSGA-II,
generations=10 / population_size=200. `repair_mode` is a no-op on the FRETISH
specs, so it is crossed only over the TLSF corpus. **Why:** on the prior TLSF
sweeps monolithic sat at `implies_ideal`≈0 on five of six specs — finding *a*
repair but never one equivalent-or-stronger than the ideal — and never repaired
`arbiter` at all, while `humanoid-531` cost ~13 min per run (768 s mean at this
operating point). MUC extraction focuses the search and the per-candidate ltlsynt
cost on the minimal unrealizable core, so the hypothesis was that it would reach
the ideal where the whole-spec search cannot, and cut the runtime on the heavy
specs. The two responses — `implies_ideal` and `wall_time_s` — are read co-equally.

**Run.** Launched seed-major across av2 (seeds 0–30) and av3 (31–60), **2,928
runs** (1,488 + 1,440), completed 2026-07-21 after ~67 h — past the 60 h budget,
but run to completion, so all 61 seeds are present rather than a truncated design.
`jobs=1` per machine (one counter process using the full thread pool, keeping the
per-process ltlsynt RAM cap machine-wide). Timeout caps were sized to the *slow*
monolithic arm and applied identically to both arms (humanoid 2400 s, lift 600,
gyro 300, the rest 120), so the fast arm is never the reason a cell is censored.
Data: `experiments/results-muc.csv`. The design is **paired** — monolithic and
MUC run at the same seed and the same mutation level — so every comparison below
is matched: McNemar's exact test for the binary outcomes, Wilcoxon signed-rank
for wall-time.

### Result: MUC trades ideal-strength for coverage

MUC is not a general improvement. It rescues the one spec monolithic cannot
repair and degrades a spec monolithic solves perfectly, and does little else.

| Specification | metric | mono | muc | McNemar p |
|---|---|---|---|---|
| **arbiter** | `found_repair` | 0.00 | **1.00** | 7e-74 |
| **arbiter** | `implies_ideal` | 0.00 | **0.14** | 6e-11 |
| **lily02** | `implies_ideal` | **1.00** | 0.34 | 7e-49 |
| humanoid-531 | `found_repair` | 0.81 | 0.79 | 0.73 (ns) |
| humanoid-531 | `implies_ideal` | 0.01 | 0.00 | 0.50 (ns) |
| gyro-var1 / lift / minepump | `implies_ideal` | 0 | 0 | 1.0 (ns) |

Pooled, `implies_ideal` *falls* under MUC (0.174 → 0.084, driven almost entirely
by lily02) while `found_repair` *rises* (0.798 → 0.956, driven entirely by
arbiter). The two headline effects point in opposite directions.

### The metric has no headroom on four of six specs

`implies_ideal` sits at ≈0 for gyro-var1, lift, humanoid-531 and minepump in
*both* arms. The genetic search reliably finds valid repairs on these specs, but
they are *incomparable to* or *strictly weaker than* the ideal, never stronger —
the ideal is out of this search's reach at gen10/pop200, regardless of repair
mode. The mono-vs-MUC quality question therefore lives only on **arbiter** and
**lily02**; elsewhere both arms are pinned to the floor and the comparison is
uninformative.

### The `best_relation` decomposition: MUC's signature is "incomparable"

Breaking each arm's repairs down by their logical relation to the ideal (as a
fraction of 244 matched pairs) shows the mechanism.

| Spec | arm | stronger | equiv | incomparable | weaker | none/TO |
|---|---|---|---|---|---|---|
| arbiter | mono | – | – | – | – | **100%** |
| | muc | – | 14% | **82%** | 4% | – |
| lily02 | mono | **100%** | – | – | – | – |
| | muc | 34% | – | **48%** | 18% | – |
| lift | mono | – | – | – | 98% | 2% |
| | muc | – | – | 13% | 83% | 5% |
| humanoid-531 | mono | 1% | – | 80% | – | 19% |
| | muc | – | – | 79% | – | 21% |
| gyro-var1 | mono | – | – | 99% | 1% | – |
| | muc | – | – | 99% | – | 1% |
| minepump | mono | – | – | 14% | 86% | – |
| | muc | – | – | 11% | 89% | – |

Wherever MUC moves the distribution, it moves mass *toward incomparable*: arbiter
0→82%, lift's strictly-weaker 98%→83% (+13% incomparable), lily02's
strictly-stronger 100%→34% (+48% incomparable). Core-focused evolution repairs
the minimal sub-problem and, after `reintegrate`, lands on a structurally
*different* fix — valid, but off the ideal's logical axis. It is not simply
weakening guarantees toward triviality: the strictly-weaker share stays modest
(arbiter 4%, lily02 18%).

### The two real effects are mirror images

**arbiter — a coverage rescue.** Monolithic never finds a repair (0/244): the
whole-spec search cannot escape the unrealizable region. MUC finds one on every
seed and level (244/244, p≈7e-74), and 14% are *equivalent* to the ideal
(p≈6e-11). This is MUC's reason to exist — when monolithic has nothing, MUC's
focus on the core finds something.

**lily02 — a quality regression.** Monolithic lands strictly-stronger-than-ideal
on every run (`implies_ideal` = 1.00). MUC still repairs every run
(`found_repair` = 1.00, unchanged), but scatters: 34% strictly stronger, 48%
incomparable, 18% weaker (p≈7e-49). The loss is not lost coverage — it is the same
coverage aimed at a different, off-ideal destination.

Both effects are the same operation (MUC → incomparable); their *value* flips on
whether monolithic had anything to begin with.

### The incomparable repairs are guarantee-weakenings, not missed valid fixes

A natural hope is that the incomparable repairs are alternative *valid* fixes the
hand-written ideals missed. They are not. Comparing each incomparable repair's
guarantees to the original semantically (via `ltlfilt`), **117 of 118 on lily02
and all 199 on arbiter also mutate a guarantee**, and the mutation is a
*weakening* — the original guarantee set implies the repaired one. MUC reaches
realizability by dropping the spec's own obligations: most often the liveness
guarantees (arbiter's `G F g0`/`G F g1`, replaced by tautologies), sometimes the
safety `g -> r`. The one apparent "clean fairness assumption" (`G F go` on lily02)
turned out, on inspection, to sit alongside a weakened guarantee. Pure
assumption-only incomparable repairs number **1 of 118 on lily02, 0 of 199 on
arbiter** — there is essentially nothing to mine.

### MUC does add assumptions — the ideal is just a small target

This is not an inability to add assumptions: the environment side is live in the
MUC core and is used. Of arbiter's 244 MUC repairs, **35 (14%) add both `G F r0`
and `G F r1` with guarantees intact — reaching the ideal exactly** — and another
~120 add one of the two. The dominance of guarantee-weakening (199 incomparable vs
35 equivalent) is a *search-landscape* effect, not a capability limit. Reaching
the ideal needs the fixed-rate `p_add_assumption` structural mutation (0.05) to
fire twice and pick both request atoms while nothing weakens a guarantee — a
narrow target — whereas nudging any guarantee toward a tautology is a large,
easily-reached basin. The sweep-M split does not move this: arbiter's ideal-hit
rate is flat across `pg` levels (10/8/6/11 as `p_assumption` falls 0.7→0.1),
because arbiter starts with no assumptions, so `p_assumption`-weighted rolls fall
through to the guarantee side and assumption *creation* rests entirely on
`p_add_assumption`, held at its default here. **The lever for pushing MUC toward
the ideal is therefore `p_add_assumption`, not the mutation split** — the subject
of the 2026-07-22 follow-up.

### humanoid-531 is null, once the pairing corrects for censoring

The marginal medians make MUC look slower on humanoid (326 s vs 400 s among
completed runs), but that is an artefact of differential timeout-censoring. On
matched pairs there is no significant difference on any response: `found_repair`
(p=0.73), `implies_ideal` (p=0.50, both ≈0), wall-time (Wilcoxon p=0.45), or
timeout rate (47 vs 51 of 244, p=0.73). MUC neither helps coverage, reaches the
ideal, nor changes the runtime on the spec it was most hoped to help.

### Wall-time: MUC is uniformly a little slower, never faster where it matters

On both-completed pairs, MUC's per-call overhead — it re-runs the GA and the
synthesis check once per extracted core — shows as a small but highly significant
slowdown on the cheap specs: arbiter +2.2 s, gyro-var1 +3.4 s, lift +9.9 s,
lily02 +0.9 s (all p < 1e-5), a marginal *speed-up* on minepump (Δ≈0, p=5e-6),
and no significant difference on humanoid (p=0.45). The hoped-for runtime win on
the heavy specs did not appear.

### What this campaign cannot answer

- **Four specs have no headroom.** With `implies_ideal`≈0 in both arms on
  gyro-var1, lift, humanoid-531 and minepump, the campaign cannot say whether MUC
  would help at an operating point where the ideal is reachable — only that it does
  not at gen10/pop200. A higher operating point is the obvious follow-up, but it
  multiplies the already-heavy humanoid cost.
- **humanoid is ~20% timeout-censored** (the 2400 s cap bit in both arms). The
  quality comparison filters those false zeros and the paired wall-time test uses
  both-completed pairs, but the censoring caps how precisely runtime can be read —
  "no faster", not a clean distribution.
- **The mutation-split (sweep M) main effect is not analysed here.** `repair_mode`
  is the reported factor; whether any `p_guarantee` level narrows lily02's loss or
  lifts arbiter is a separate cut of the same data.
- **NSGA-II, gen10/pop200 only.**

### Method notes worth keeping

- **The comparison is paired, so use paired tests.** mono and muc share the seed
  and mutation level, giving 244 matched pairs per spec. McNemar (exact binomial on
  the discordant pairs) and Wilcoxon signed-rank are correct here; an unpaired
  Fisher/Mann-Whitney would discard the pairing and lose power. (No SciPy on the
  analysis box — both tests were implemented by hand.)
- **Timeouts are false zeros; filter before reading quality.** `implies_ideal` for
  a timed-out run is 0 by construction. The McNemar on `implies_ideal` excludes any
  pair where either arm timed out; the Wilcoxon uses both-completed pairs only.
  humanoid's marginal-median "slowdown" that the pairing dissolves is the
  cautionary example.
- **The counting path leaked processes over the multi-day run.** `ltl2tgba` on the
  model-counting path had no timeout, and its `-D` determinization blows up
  super-exponentially on the deeply nested formulae the search builds; a hung
  multi-GB process is then orphaned (reparented to PID 1) when the run is torn
  down. Over ~3 days av2 accumulated ~93 GB of such orphans, near-OOM. A 2-hourly
  janitor (`kill` on `ppid==1 && comm==ltl2tgba && etime>1h`) kept it contained.
  The fix — a counting-path timeout plus `PR_SET_PDEATHSIG` on every subprocess —
  landed after this campaign (not in the binary it ran), so the humanoid timing
  carries some leak noise. [[ltlfilt-simplify-blowup]]

### Scripts and launch

```sh
python scripts/gen_configs.py --tlsf --sweeps M --repair both \
    --out-dir experiments/configs-muc
# av2                                              # av3
… --profile muc --jobs 1 --seeds $(seq 0 30)       … --seeds $(seq 31 60)
python scripts/merge_experiments.py --profile muc av2 av3   # python3 on av2
```

**Verdict: MUC is a fallback, not a default.** It converts "no repair" into an
incomparable repair — rescuing `arbiter`, the one spec monolithic cannot touch —
and "ideal repair" into an incomparable one — degrading `lily02` — while leaving
the four specs with no metric headroom (including `humanoid-531`) statistically
unchanged, at a small, consistent wall-time cost. Reach for MUC-guided repair on
specifications where monolithic returns nothing; keep monolithic as the default
everywhere it already finds a repair.

---

## 2026-07-15 — Weakening filter as a crossed factor

**What changed.** `run_weakening` moved from a one-level sweep (the factorial's
sweep J) to a *crossed factor*: sweeps C, D, E, F and I ran at generations=40 /
population_size=1000 under NSGA-II, every level executed once with the weakening
filter on and once off. **Why:** the 2026-07-14 factorial's largest C–J effect
was the weakening filter, but it was read only at gen10/pop200 — the worst point
on both the *generations* and *population* curves — with no evidence it survived
at scale or how it interacted with the other parameters.

**Run.** Launched seed-major across av2 (seeds 0–44) and av3 (45–89) against an
18-hour budget, killed at the deadline (2026-07-16 12:00). Yield: **78 complete
seeds** (0–39, 45–82), 19,344 rows, 0 timeouts. Seed-major ordering makes the
survivors a *balanced* design — every cell sampled at the same 78 seeds — rather
than a ragged one.

### Result: disabling the filter raises `implies_ideal`

| Specification | off | on | Δ | Fisher p |
|---|---|---|---|---|
| **fsm** | 0.565 | 0.356 | **+0.208** | 7e-48 |
| **fsm-combined** | 0.096 | 0.063 | +0.033 | 3e-5 |
| fsm-timing | 0.958 | 0.955 | +0.003 | 0.62 (ns) |
| takeoff | 0.951 | 0.947 | +0.004 | 0.65 (ns) |

Pooled, 0.642 off against 0.580 on. The effect concentrates where there is
headroom; `fsm-timing` and `takeoff` sit near ceiling and do not move.

The mechanism is direct. `implies_ideal` counts repairs equivalent to or stronger
than the ideal, and the filter makes guarantees weaker — pushing them into the
*strictly weaker* class, the one class that fails the test. On `fsm`,
`best_relation` is `strictly weaker` for 62.9% of weakening-on runs against 5.1%
off, and `strictly stronger` for 38.3% off against 2.7% on. The filter does
exactly what it is named, and that is the whole effect.

### It replicates across the scale and engine change

The 2026-07-14 factorial, at gen10/pop200 and a *different* model-counting engine
(before `4ba4c91`), showed the same sign on `fsm`: 0.340 off against 0.030 on
(p≈7e-9). A 4×/5× change in generations and population, plus an engine change,
leave the direction identical. This is a structural property of the filter, not
an artefact of the operating point.

### The cost is speed, not success

| Specification | `found_repair` off/on | median `wall_time_s` off/on |
|---|---|---|
| fsm | 1.00 / 1.00 | 27.5 / 17.8 |
| fsm-timing | 1.00 / 1.00 | 36.5 / 25.1 |
| fsm-combined | 1.00 / 1.00 | 63.9 / 50.0 |
| takeoff | 1.00 / 1.00 | 11.9 / 11.8 |

Weakening off never fails to find a repair (100% `found_repair` in both arms) and
returns more repairs per run; its only cost is 30–55% more wall-clock. The filter
is purely a speed optimisation, trading ~20 percentage points of `fsm`
`implies_ideal` for ~30% faster runs. The caveat: all four specifications reach
100% `found_repair` without it, so weakening's intended job — rescuing
realizability when strong guarantees over-constrain — is never exercised here. On
this benchmark it is all cost.

### The surprise: a sign-flipping interaction with `status-only`

The `status-only` fitness preset, which weights only the status objective, is the
largest interaction in the data, and it flips sign. On `fsm` it scores 0.872 with
weakening off (best of any configuration) and 0.013 with it on (worst), p≈3e-9,
against a default of 0.615/0.372. With only the status objective active, nothing
penalises weakening the guarantee, so the filter weakens it to nothing; with the
filter off, the same narrow objective is the most effective preset tried.
`status-only` is viable *only* with weakening off.

### The SPOT bug fired more than expected, but does not bias the contrast

The pre-launch spot check found ~3 dropped individuals in a small sample; the full
campaign dropped **9,805**, up to 70 in a single run, touching 9.8% of rows. It is
spec-specific — `takeoff` never triggers it, the fsm family does — and stays
contained: 0 timeouts, 100% `found_repair`, no run aborted by the circuit breaker.
It also correlates weakly with the arm (11.75% of off rows affected against 7.95%
on), exactly the bias the design flagged. But the direction is reassuring: dropped
rows carry *lower* `implies_ideal` in both arms, and the off arm has more of them,
so the imbalance works against the measured off-over-on gap. The weakening
advantage is if anything understated.

### `fsm` and `fsm-combined` came off the floor

Baseline (weakening on) `implies_ideal` rose from 0.030 to 0.372 for `fsm` and
from 0 to 0.064 for `fsm-combined` against the 2026-07-14 factorial. The jump
confounds three changes — the larger operating point, the ideal-realizability fix,
and `4ba4c91`'s bound raise — so it cannot be attributed to any one, but the
magnitude is large.

### What this campaign cannot answer

- **Interactions are underpowered.** A per-level interaction carries a ±11-point
  95% interval at 78 seeds — wider than the effect it would explain. Sweep-level
  *trends* (pooling five levels) are readable; a single level that appears to flip
  is noise until a targeted follow-up says otherwise.
- **NSGA-II only.** Every conclusion is conditional on it; the factorial already
  settled the scheme comparison, and weighted is not the production scheme.
- **No comparison with the 2026-07-14 factorial.** `4ba4c91` changed the engine,
  so any cross-run difference confounds operating point with engine version
  ([[stale-results-after-engine-change]]). The scale-jump figures above are read
  in that light.
- **G (bound) and H (crossover) dropped** — ~28% of budget on parameters that
  showed a combined 2.6-point spread at pop200. The risk is that crossover may
  wake at pop1000, and H would have given the weakening × crossover interaction
  (both govern population diversity). Revisit this first.

### Method notes worth keeping

- **Cost was calibrated under a saturated queue** (32 jobs / 4 workers). A 4-job
  batch underestimated it by 27%, because short specs finish early and hand their
  cores to long ones — a bias absent at gen10/pop200, where the specs differ by
  ~2 s ([[calibration-saturation-bias]]).
- **Seed-major ordering** turns the deadline kill into a balanced design; the
  natural config-major order would leave the last levels at zero seeds.
- **Stale-binary hazard.** `build-release/counter` must be checked against
  fix-commit mtimes, not its checkout's HEAD; a binary three commits behind faked
  SIGSEGVs and hung `signal_tracer` processes ([[stale-binary-trap]]).
- **Isolation.** The profile writes its own `results_dir`, `configs_dir` and CSV,
  and `run_id` includes the weakening state, so the two arms never read each
  other's `repair_*.json`.
- **Timeouts are false zeros** (`implies_ideal=0`, `timed_out=1`); filter on
  `timed_out` before analysing. None fired this run.

### Scripts and launch

`gen_configs.py` gained `--generations`, `--population-size`, `--schemes`,
`--sweeps`, `--weakening` and `--out-dir` (defaults reproduce prior output
byte-identically). `run_experiments.py` gained the `cj-large` profile, a
`weakening` CSV column modelled on `selection`, per-profile directories and
baseline aliasing. `merge_experiments.py` gained the profile and — critically —
`weakening` in its merge key, without which the two arms collapse onto one key.

```sh
python scripts/gen_configs.py --generations 40 --population-size 1000 \
    --schemes nsga2 --weakening both --sweeps C D E F I \
    --out-dir experiments/configs-cj-large
# av2                                          # av3
… --profile cj-large --jobs 4 --seeds $(seq 0 44)   … --seeds $(seq 45 89)
python scripts/merge_experiments.py --profile cj-large av2 av3
```

**Verdict: disable the weakening filter for quality-focused repair.** It degrades
`implies_ideal` by making guarantees strictly weaker than the ideal, costs nothing
in repair success, and buys only runtime — on a benchmark that never puts it in
the situation it was designed for.

---

## 2026-07-14 — Factorial sweep A–J

**What changed.** A one-factor-at-a-time factorial: sweeps A (*generations*) and
B (*population size*), plus C–J holding everything else at gen10/pop200, each
level run under both NSGA-II and weighted selection, 100 seeds. Data:
`experiments-14-07-2026/results-factorial.csv` (25,196 NSGA-II rows).

**What it found.**

- **A and B climb monotonically** and are still rising at their top level
  (A: 0.480 → 0.576 over gen5–gen80; B: 0.348 → 0.527 over pop50–pop1500). So
  every C–J reading sits near the bottom of both curves.
- **C–J split three ways.** *Real signal* — J (weakening) 0.575 off vs 0.500 on,
  I (*mutation rate*) 0.280 → 0.500. *Signal only at degenerate extremes* — D at
  ptrig1.0 and F at ptim0.0 collapse to 0.007, each locking out a mutation
  dimension the repair needs. *No signal* — G (*bound*) 0.492–0.505 over a 32×
  range, H (*crossover*) 0.495–0.512.
- **NSGA-II resolved the earlier `implies_ideal` regression** ([[implies-ideal-regression]]);
  the weighted scheme converges prematurely and is kept only for comparison.

**Why it matters.** The weakening result — a filter improving the metric by being
*disabled*, measured at the worst operating point — motivated the 2026-07-15
crossed-factor campaign above.
