# Experiment campaigns

One dated subdirectory per closed campaign, `YYYY-MM-DD-<campaign>/`, each
holding its results CSV(s), run directories, configs, and an executed
`analyse.ipynb` tailored to that campaign's question. Active campaigns write
to this directory's top level (`run_experiments.py` / `merge_experiments.py`)
and are archived into a dated subdirectory when they close. Within a
subdirectory the CSV stem matches its run-dir folder name
(`results-muc.csv` ↔ `results-muc/`) so
`scripts/recompare.py --results <csv>` resolves run dirs directly.
Campaign narrative and findings live in `../EXPERIMENTS.md` (entries exist
from 2026-07-14 onwards).

| Directory | Campaign |
|---|---|
| `2026-07-10-fretish-sweeps` | Earliest archived sweeps: A (generations), B (population), C (fitness-weight presets) on the four FRETISH specs; the pre-engine-change "July-10 baseline". |
| `2026-07-13-fretish-sweeps` | Same design re-run post-engine-change; 130 of 889 repair rows have pruned run dirs (see vintage note). |
| `2026-07-14-factorial` | One-factor-at-a-time screening, sweeps A–J at gen10/pop200, ~100 seeds, FRETISH. |
| `2026-07-15-cj-large` | Weakening filter as a crossed factor at gen40/pop1000; killed at deadline, balanced ~78 seeds. |
| `2026-07-16-genpop-sweeps` | Capacity sweeps A/B on FRETISH + TLSF. `results-fretish.csv` run dirs no longer exist anywhere — its ideal-comparison columns are stale (pre-2026-07-24 ideals), lower bounds only. `results-tlsf-10s.csv` is a row-reordered duplicate of `results-tlsf.csv`. |
| `2026-07-16-metric` | Log vs direct semantic-similarity metric, nsga2, gen40/pop1000, 800 runs (pulled from av2+av3 2026-07-29). |
| `2026-07-17-tlsf-genpop` | TLSF A/B rerun testing the ltlsynt-timeout guard (`ltlsynt_timeout_ms=500`) later campaigns adopted. |
| `2026-07-21-muc` | Monolithic vs MUC-guided TLSF repair (sweep M). |
| `2026-07-22-padd` | `p_add_assumption` sweep (sweep P). |
| `2026-07-23-wellsep` | Well-separation × output assumptions on TLSF (sweep W). |
| `2026-07-23-arbiter-hp` | Sweep-W arms on the arbiter family — zero repairs in every cell (null result). |
| `2026-07-23-arbiter-padd` | p_add 0.1→0.8 × filter arms on arbiter — zero repairs in every cell; rules out p_add as the arbiter unlock. |
| `2026-07-24-ablation` | 2×2×2 factorial (selection × metric × Halstead) on FRETISH (4 specs, 40 seeds) + TLSF (19 families, 20 seeds), plus the AuRUS head-to-head (12 families, 30 repeats, ltlsynt-validated). First campaign with recorded rather than inferred provenance. Also archived with the paper (`~/projects/writing/counter-paper/data/ablation-2026-07/`). |
| `2026-07-31-replicate` | nsga2 vs nsga2-replicate selection: sweep R (both schemes × elitism 0/0.1, gen40/pop1000, 200 seeds) with its censored-row recap, a weakening-off cross on fsm/fsm-combined, a compute-matched nsga2 arm (sweep S, gen120 = 3.0× measured cost ratio), and a TLSF half on 5 specs — 7,600 rows. First campaign with per-row commit attribution in the CSVs (backfilled for the pre-column rows; see its `PROVENANCE.json`). Its source commits are not ancestors of `main`: the branch was split into reconstructed pull requests, so `provenance/replicate-campaign` is what holds them. **Its `arbiter` cell is a single-family result that did not generalise** — `2026-08-10-arbiter-probe` reproduced it at full strength on a current engine but found it in 1 of 9 arbitration families, so it is evidence about `examples/arbiter` and not about the scheme, and should not be cited as the latter. |
| `2026-08-03-libspot-soak` | 24-hour soak of the in-process libspot paths across av2 and av3, 2,656 runs, zero unexpected exits. Not a parameter sweep: it varies the deadline tier, not a search parameter, and asks whether the in-process path drifts, leaks or answers differently rather than whether anything scores better. Both arms are in-process, so it prices no boundary — it cannot say what running in process buys, only that it costs nothing visible. Includes the `repro-lift-*` phases that settled its two `lift` mismatches. Its commits are not ancestors of `main` and will not be: the archive merged on its own while `feat/profiling-harness` stayed unmerged, so `provenance/libspot-soak` is what holds them. |
| `2026-08-04-engine-comparison` | In-process libspot simplification against spawning `ltlfilt`, 416 runs across av2 and av3, every pair within-host and within-seed. This is the boundary the soak could not price, both of its arms having been in-process. The arms agree on all 174 uncapped pairs and complete identically at 174/208 each, so the whole of the difference is cost: 4.3% of total campaign wall time, against a peak resident set of 24.99 GB inside `counter` where the subprocess arm never passed 0.18 GB. That result is why `feat/profiling-harness` stays unmerged. Its driver `compare_engines.py` was never committed on that branch, so the vendored copy here is the only one. |
| `2026-08-07-elitism` | `genetic.elitism_rate` 0 against 0.1 as a shipped default, `nsga2-truncate` only: 1,200 FRETISH rows (4 examples, 150 seeds) and 1,594 TLSF rows (20 families, 40 seeds), with the decision rule pre-registered in its `PLAN.md`. Decision: keep 0.1, on the cost criterion alone — 0 runs 16.2% longer on TLSF against a 10% bound — while quality is non-inferior at 0 and TLSF yield is better there (0.746 against 0.714, McNemar p = 0.0002). The post-hoc triviality audit is vendored as `scripts/triviality_audit.py`; its non-zero count is the one criterion that says nothing about elitism, the 3 and 4 tautological repairs being `black` answering SAT on the negation of a valid weak-until formula, fixed on `main` by fb4c3ed. Its source commits are not ancestors of `main`: `provenance/elitism-campaign` is what holds them. |
| `2026-08-10-arbiter-probe` | **Decision: spec-specific — the unlock is `examples/arbiter`, not the arbitration family.** Asked whether `nsga2-apportion`'s yield advantage on `examples/arbiter` (120/120 against `nsga2-truncate`'s 0/120 in `2026-07-31-replicate`, the only place either NSGA-II scheme has separated on `found_repair`) is a family-level effect or a property of that one spec. 800 TLSF runs, 400 complete pairs, zero errors: 10 families × 40 seeds × both schemes at gen10/pop200 across av2 and av3 — the nine fixes-backed arbitration families the corpus now holds, plus `lily02` as the counter-signal control, apportion having cost it `implies_ideal` 1.00 → 0.44 in replicate. `arbiter` reproduces at full strength (0.000 against 0.950, 38 discordant pairs all one way, Holm-corrected p < 0.0001), so it is a property of the scheme rather than of replicate's engine vintage — but 1 of 9 families clears the bar against a pre-registered threshold of 3, so criterion 1 fails and criterion 2 passes with nothing reversing. The pattern is not arbitration-versus-not and should not be read that way: `load-balancer` went 8 against 0 and missed only on the Holm correction (raw p = 0.0078), while the `lily02` control moved *with* apportion on both yield (10 against 1) and quality (`implies_ideal` 0.525 → 0.925), the opposite of its replicate behaviour. Quality is a reported caveat rather than a gate and is mixed: `arbiter-aurus` loses (0.925 → 0.575) at unchanged yield, `load-balancer` and `lily02` gain. Cost is not the obstacle this time — pooled median paired wall ratio 1.13, aggregate 1.21, against the ~3× that failed replicate's bound — so breadth rather than cost is what keeps apportion opt-in. It carries no compute-matched arm and does not reopen the default; scored against replicate's four default criteria after the fact it passes two, fails the pooled `implies_ideal` margin, and leaves the compute-matched one untestable. First campaign archived under the post-2026-08-06 scheme spellings, and the first to run with well-separation folded into the status score (`b101ada`), so `run_well_separation` inherits the restored `false` default and the filter stage is absent by design. Carries `analysis-output.txt` in place of an executed `analyse.ipynb`. Two earlier executions of the same design completed and were discarded rather than archived: the first, at `536a3ae`, predated `d7733fc`, `fb4c3ed` and `b101ada`, the last of which rescales the status objective selection reads; the second, at `df66e44`, lost 66 of 800 runs to a defect in `fb4c3ed` this campaign was the first to exercise, where the new weak-until rewrite is the first path on which a SPOT-printed string reaches `black` and `black` rejects SPOT's `0` for false as a syntax error. That loss was neither random nor balanced — a run aborts only where the search reaches such a formula — so the unpaired rows would have biased the very comparison the decision rule reads. |
| `2026-08-14-aurus-h2h` | **Decision: AuRUS scores higher on the pre-registered repair-quality endpoint (`PLAN.md` §5 outcome 2), reported as measured with no re-run and no post-hoc arm.** A head-to-head against AuRUS, the baseline repair tool, on AuRUS's own 26-spec TLSF corpus with `humanoid-741` held out. The counter arm ran 20 seeds per family against AuRUS's 30 repeats, AuRUS pinned at `3f6f01f` and run at its pre-fork base rather than this project's fork. Per-family Wilcoxon signed-rank over 25 families puts counter higher on 5 and AuRUS on 14 with 6 tied, W+ = 34.5, p = 0.0127, mean rate 0.281 against 0.504; clustering to the 10 effective problem types per threat 7.10 gives 2, 7 and 1 tied, W+ = 3, p = 0.0195, so both reject at alpha 0.05 in the same direction and the clustered tie-breaker was never needed. The p-values are exact, enumerated over sign assignments, because k/20 against k/30 rates tie constantly and averaged ties make the ranks fractional. Amendment §10.1 restricts the primary rate to AuRUS repairs that are realizable and well-separated, counter's output gate rejecting an ill-separated survivor unconditionally, so leaving AuRUS's rate unfiltered would score the arms by different standards; a sweep of all 287,006 archived candidates found 113,958 of them (39.71%) not well-separated and none undecided, of which 113,283 (39.47%) sit among the repairs `realize` confirms and are what the filter actually removes, and the unfiltered rate rides along as `implies_genuine_all` at 0.501, essentially unchanged. The sharpest threat (7.1) is that the ideals are counter's curated set, descended from AuRUS's own `genuine/` repairs then gated, renamed, pruned and in places replaced by hand-written weakenings, and it applies to this outcome as much as to any other. The diversity axis, measured 2026-08-19 after the decision, puts both arms through counter's TLSF maximality filter. AuRUS medians 480 distinct candidates, 45 maximal and 40 equivalence classes, against counter's 4, 4 and 3, so the raw candidate advantage of 317 against 4 becomes 45 against 4 as maximal specifications and 40 against 3 as classes, dropping by roughly an order of magnitude and surviving. Both counts are upper bounds, and neither can move the decision, scoring being existential and implication transitive. 499 of 500 counter runs completed; `load-balancer-aurus` seed 3 on av2 aborted at the final realizability gate on SPOT's 32-acceptance-set compile-time limit, and the abort is deterministic, so recovering that row needs SPOT rebuilt. `humanoid-742` carries no diversity number in either arm, every one of its measurements having hit the wall cap. The control exception is counter's own output, which should already be maximal and is on 436 of 446 runs; the 10 exceptions are a budget mismatch rather than a filter defect — the campaign ran `black_timeout_ms = 1000` while `maximal` and `compare` use 20,000 ms, and an undecided implication check counts as no implication — so counter's reported repair counts are a superset of the maximal antichain wherever the 1 s budget bites. `provenance/aurus-h2h-arm` holds `dea51b7`, the AuRUS arm's pre-rebase tip, which no branch reaches; the campaign's own commits sit on `feat/aurus-h2h` and are expected to merge, so they need no tag of their own. Its vendored `scripts/` holds `maximal.cpp`, the first C++ source in any campaign archive. |

## Scoring vintage

All CSVs were rescored on 2026-07-29 (`recompare.py --all`) against the
frozen final ideals set (`examples/*/fixes` as of commit d02892e, binaries
from main @ eb362fb), so every campaign scores against the same ideals as the
ablation archive. Rows whose run dirs no longer exist keep campaign-time
scores: 6,471/45,690 in factorial, 2,536/19,644 in cj-large, 45/348 in
17-07-tlsf, 130/889 in 13-07 — each notebook derives and reports the exact
mask. In factorial and cj-large the stale rows turned out to be the sweeps'
shared baseline cells (aliased copies of rescored default-config runs), so the
mixed vintage is essentially inert; bounds are stated per notebook. The rescore moved takeoff only (e.g. factorial 0.455→0.538,
cj-large 0.949→0.999); fsm was unaffected in these campaigns and every TLSF
campaign was invariant. Pre-rescore snapshots are kept as
`*.pre-final-ideals.bak.csv`. Per-machine pre-merge CSVs are kept as
`av2-*`/`av3-*` (or `av2.*`/`av3.*`).

## Config vintage

`gen_configs.py` writes a key only when a sweep overrides it, so an archived
config states a fraction of the settings its campaign ran under and inherits
the rest from the binary. That makes a changed C++ default a silent change to
what an archived config means. Two such changes have happened, both in the
commit that closed `2026-08-06-wellsep-timing`: `allow_output_assumptions` and
`run_well_separation` each defaulted to `false` for every campaign archived
here and now default to `true`. Re-running an archived config therefore needs
both keys written in as `false` explicitly. Without the first, the run admits
output atoms into assumptions where the campaign did not; without the second it
also drops the candidates that draw produces, so the two omissions do not
cancel — they compound into a search neither the old nor the new default
describes. The arms that set the keys themselves (the wellsep and ablation
factorials, and wellsep-timing's own three levels) are unaffected, since they
state them either way.

`run_well_separation` has now crossed the line a second time, in the other direction: it defaults to `false` again from the commit that folded well-separation into the status score. It is the only key to have moved twice, so an archived campaign needs it written in explicitly at whichever value it ran under — `false` for the campaigns archived under the pre-2026-08-06 default named above, `true` for anything that inherited the default between the two commits. That makes three keys to restore by hand alongside `allow_output_assumptions`, not two.

`genetic.selection_scheme` crossed the line on 2026-08-14, moving from `nsga2-truncate` to `nsga2-apportion` on the `2026-08-11-selection-default` campaign. Almost nothing here is exposed: 73,467 of the 73,531 archived run configs state the key themselves, so a current binary reads them exactly as their campaign ran them. The 64 that do not are all under `2026-07-13-fretish-sweeps`, every one a `sweep_B_pop50_takeoff_*`.

Those 64 need `selection_scheme = "weighted"` written in, not the scheme this change replaced. The default has moved three times, and only the last is this campaign's doing: `WeightedAverage` until 2026-07-15 (`587a5b6`), `Nsga2` from there until the 2026-08-06 rename to `Nsga2Truncate` (`96c7633`), and `Nsga2Apportion` from 2026-08-14. That campaign is anchored at `4d76d53`, which predates the first of those, so its unstated configs ran under `weighted` and have been misread since 2026-07-15 — a crossing this note did not record at the time. Re-running one against a current binary otherwise gets a scheme two changes removed from the one it measured.

The same commit changed what the status *scale* means rather than what a config says, which no amount of writing keys back repairs. `k_status_realizable` (1.0) now requires a candidate to be well-separated as well as realizable, and one the system can satisfy only by forcing its own assumptions to fail scores `k_status_unrealizable` (0.5) — level with a candidate no strategy exists for, where it previously scored the top tier. Status is a selection objective on both paths, so status-derived numbers and every quality figure downstream of selection are on different scales either side of this commit and do not compare across it.

`fitness.status_grading` crossed the line on 2026-08-11, and it is the widest crossing so far because the key did not exist before that date. Every campaign archived here ran the three-point `tiered` scale, none of them can state a key their binary had never heard of, and the default is now `mrs` — so reproducing any of them means writing `status_grading = "tiered"` into the config by hand. Omitting it does not shift a threshold, it swaps the status objective for a different function: `tiered` scores every unrealizable candidate 0.5, where `mrs` spreads that same region over the fraction of guarantee-side parts a greedy sweep keeps. Selection then sees a gradient the campaign never had. `2026-08-11-status-grading` is the one exception, and only because sweep G writes the key into both arms.

That campaign is also what moved the default, and it measured the size of the difference rather than assuming it: paired over 20 TLSF specifications and 24 seeds, `mrs` yielded 410/480 repairs against `tiered`'s 367/480 (50 `mrs`-only pairs to 7, sign test p < 0.0001) at a median paired wall cost of 1.15x. Almost all of the gain is on two specifications `tiered` grades as one flat value — `arbiter` 0/24 → 22/24 and `rg1` 7/24 → 24/24 — which is the reason a zero-yield row from an archived TLSF campaign on those families measures the scale rather than the search.

`fitness.mrs_admission_order` crossed the line on 2026-08-14, and like `status_grading` it does so from a standing start: the key did not exist before that date, so no archived config can state it. It is read only under `status_grading = "mrs"`, and every campaign archived here ran `tiered`, so reproducing one already means writing `status_grading = "tiered"` in by hand — and with that written, `mrs_admission_order` is not read at all and needs nothing. `2026-08-11-status-grading` is the exception that matters. Sweep G writes `status_grading` into both arms, so its `mrs` arm does read the new default and needs `mrs_admission_order = "spec"` written in explicitly to reproduce. Omitting it there does not shift a threshold. The greedy walk admits guarantee-side parts in a different order, and greedy returns a maximal subset rather than a maximum one, so the order decides the score. Over populations of mutants across six TLSF specifications the two orders score 0.587 against 0.529, and on `detector` the original specification scores 6/7 under `degree` against 1/7 under `spec`. The default moved on that population measurement rather than on a paired campaign, unlike `status_grading`, so a campaign is still owed. `gen_configs.py --pin-vintage` now writes the key, so campaigns archived from here on state it.

Three more moved in the commit that narrowed the defaults for push-button use:
`ltlsynt_timeout_ms` `0` → `500`, `ltl2tgba_timeout_ms` `0` → `60000` and
`max_scoring_failure_rate` `0.05` → `0.15`. Every TLSF campaign archived here
already set all three to those exact values, so the TLSF archives are
unaffected and reproduce as they stand. The FRETISH campaigns state none of
them, and so now run under bounded tools where they ran unbounded; write the
three in as `0`, `0` and `0.05` to reproduce one.

`mutation.p_remove_guarantee` crossed the line on 2026-08-13, and like `status_grading` it does so from a standing start: the key did not exist before that date, so no archived config can state it, and it arrives defaulting to `0.05` rather than to the `0` that describes every campaign here. It adds a mutation operator rather than shifting a threshold — the search can now delete a guarantee outright, which is a move none of these campaigns could make — so reproducing any of them means writing `p_remove_guarantee = 0.0` into the config by hand. That value is exact rather than approximate: the operator's probability is tested before the RNG is drawn, so at zero it costs no draw and the whole breeding stream is byte-identical to what it was before the operator existed. At any other value it is not, and no archived seed reproduces.

A third change is sharper than a shifted default, because it stops an archived
config from running at all. On 2026-08-06 the two NSGA-II selection schemes were
renamed: `nsga2` became `nsga2-truncate` and `nsga2-replicate` became
`nsga2-apportion`. The old spellings are **rejected**, not aliased, so every
config archived here — all of which pin one of them — fails against a current
binary with an error naming the replacement. This is deliberate. Reproducing a
campaign means building the commit its `PROVENANCE.json` names and running the
vendored `scripts/` beside it, not editing the archived config to suit a newer
binary; a config edited to run under today's defaults is no longer the record of
what ran.

The archived **results** are unaffected and stay readable. Their `selection`
column still says `nsga2`, and `canonical_scheme()` in `run_experiments.py` and
`merge_experiments.py` maps that onto `nsga2-truncate` wherever the column is
read, so archived rows still join new ones for resume, merge and cross-campaign
comparison. Nothing under `experiments/` was rewritten to match the new names,
and nothing should be: the column records the name the campaign ran under.

A fourth change keeps both its key and its value and moves only what they mean. The per-generation false-condition filter has been folded into the *vacuity* filter, which has also gained a semantic screen: a specification is rejected if any one of its guarantees is *valid*, tested one guarantee at a time by asking whether that guarantee's negation is satisfiable. Previously `filters.run_vacuity` gated a satisfiability check on the conjoined assumption side and nothing else, while the false-condition screen — a syntactically `false` condition on any assumption or guarantee — ran unconditionally, outside the flag entirely. All three screens are now one filter under `run_vacuity` alone.

The default did not move: `filters.run_vacuity` was `true` and still is. No archived config sets it, so every campaign archived here ran the filter on, under the older and weaker meaning of the same word. That puts this change outside both cases above. Unlike the shifted defaults there is no key to write back, the archived value being the one a current binary reads; unlike the rename the config still runs, because nothing about it is rejected. It is the quietest of the four. An affected campaign re-run today raises no error and no warning, and simply searches under a stricter filter than the campaign did, discarding tautological guarantees the campaign was free to keep. The remedy is the one the rename already prescribes: build the commit the campaign's `PROVENANCE.json` names and run the vendored `scripts/` beside it.

One consequence reaches forward rather than back. Setting `run_vacuity = false` now also switches off the false-condition screen, which used to run whichever way the flag was set. No archived config sets the key, so no archive here is affected, but an ablation that turns vacuity off is no longer isolating the assumption satisfiability check — it drops three screens and prices them as one. A flag that gains screens gains them for its off position too, which is the half of a merge that is easy to leave unstated.

A fifth change moves no key, no value and no default, and still stops archived numbers from comparing. From this commit the gate that collects the realizable survivors applies the well-separation check to every one of them, whatever `filters.run_well_separation` says. Before it that check ran per generation alone, so a not-well-separated specification still reached the output whenever it arrived by a route the offspring filters never cover — an elite carried through unchanged, or a copy of the seed population. Those repairs are now rejected at the gate, so written-repair yield falls from this commit on and a campaign's repair counts do not compare across it. Which candidates are bred and scored is unchanged; only which survivors are written out is.

A sixth change removes a key outright, which is the sharper form of the rename above: `fitness.weight_halstead` went on 2026-08-13 with the Halstead objective it weighted. An archived config that sets it is **rejected** rather than reinterpreted, and every config archived here states it, so none of them runs against a current binary. Sweep C's `no-halstead` level goes in the same commit, and with it the Halstead ablation the `ablate-fret` and `ablate-tlsf` arms crossed as their third factor: the level set the weight to `0` against a default of `0.1`, so with the key gone it says nothing the `default` level does not. Both arms stand as 2x2 factorials of selection scheme by similarity metric from here on. The archived results are unaffected and stay readable, their `level_name` column still recording `no-halstead`, and the campaigns that ran the ablation reproduce the way the rename already prescribes: build the commit their `PROVENANCE.json` names and run the vendored `scripts/` beside it.

`filters.run_weakening` moved from `true` to `false` on 2026-08-20, and it is the first crossing recorded here that exposes nothing at all. All 1,464 archived run configs under `experiments/` state the key explicitly, because `gen_configs.py` pins it: the weakening screen is a crossed factor and `weakening` is one of `merge_experiments.KEY_FIELDS`, so a config that omitted it could not be joined back to the row it produced. Every one of them therefore reads on a current binary exactly as its campaign ran it, and no archived config is silently reinterpreted. `gen_configs.py` keeps its own pin at `True` and must not follow the binary default. The emitted value has to keep matching the recorded CSV column, so moving the pin would put new rows at odds with archived ones on a key field rather than track anything useful.

A seventh change removes a key outright rather than shifting what it means. `genetic.repaired_operators` went on 2026-08-20 with the gating it provided, and the repaired mutation and crossover grammar is unconditional from that commit on. This is the sharper case the "Commit provenance" section of `CLAUDE.md` describes: an archived config that **sets** a removed key is rejected rather than reinterpreted. The three campaigns archived that day — `2026-08-20-ops-pilot`, `2026-08-20-ops-grammar` and `2026-08-20-ops-weakening` — set it in every one of their configs, on both arms, so none of them runs against a current binary. Sweep O retires with the key, along with the `ops-fret`, `ops-tlsf`, `ops-pilot`, `opswk-fret` and `opswk-tlsf` runner profiles, following the precedent where `[filters.intervals]` retired the `wellsep-timing` profile and TLSF sweep V. The `PROFILE_CSVS` entries in `merge_experiments.py` are deliberately kept, so the archived CSVs stay mergeable after the profiles that wrote them are gone. Reproduce those three campaigns at the commits their `PROVENANCE.json` files name, through the vendored `scripts/` beside them.

## Commit attribution

Every campaign directory carries a `PROVENANCE.json`. For campaigns closed
before commit recording existed it is **reconstructed after the fact**, and
says so: `"attribution": "inferred"`. Campaigns run after that change record
their commit at run time instead, in the per-row CSV column and the per-host
manifest, and need none of what follows. `2026-07-24-ablation` sits between
the two: it predates commit recording in the binary but was launched and
closed under live session logging, so its file says `"attribution":
"recorded"` — with a non-null `binary_commit` — and needs none of the
reconstruction either.

The reconstruction anchors on the fact that each campaign is a uniquely-named
profile introduced by exactly one commit, and that `configs-<profile>/` was
written locally by `gen_configs.py` on the same clock as `git commit` — so the
configs' mtime dates the campaign against that commit with no av2/av3 skew in
play. Seven of the ten campaigns that have a configs directory land within
three minutes; `metric` (+8), `tlsf-genpop` (+26) and `genpop-sweeps` (−10
against an unmerged branch) are looser. Where a clock cannot
settle it, content does: `arbiter-hp` has two candidate commits eight minutes
apart, and the `generations = 100` / `population_size = 10000` in its checked-in
config picks out the calibration commit regardless of any timestamp.

`profile_commit` is not a claim about which binary produced the numbers, and
`binary_commit` is deliberately `null` in every inferred file rather than absent — the
field is present and empty to record that it was considered and found
un-inferable. Binaries on av2/av3 lag main by an unbounded, undocumented
amount. `62bbc6f` is the standing proof: committed 2026-07-20 14:16, squarely
inside the muc run window, and `EXPERIMENTS.md` records it as *not* in that
campaign's binary. Any commit falling inside a run window is therefore evidence
of nothing. Where a binary fact is actually known it comes from `EXPERIMENTS.md`
prose, and is recorded under `recorded_binary_facts` with the quote that
establishes it and a polarity of `present`, `absent`, or `mid-campaign`.

Two campaigns are not single-revision and their files say so in `caveats`:
`wellsep` had its lift cap raised 45 minutes into a 14-hour run (`6f0dfbe`), and
`factorial`'s sweep C rows predate the profile commit and come from an earlier,
unattributed binary. Two more are weak by construction: `2026-07-10` has no
profile mechanism, no configs directory and no narrative entry, so it carries
only a lower bound; `2026-07-13` is anchored by a capability boundary (all 1380
of its run dirs have a `config.toml`, which is written only at `jobs > 1`)
rather than by a commit. Their files exist anyway, recording that attribution
was attempted and how far it got — an absent file would be indistinguishable
from never having looked.

## Driver scripts

Each campaign directory carries a `scripts/` holding the three drivers that
produced it — `gen_configs.py`, `run_experiments.py` and `merge_experiments.py`
— copied verbatim from the revision that ran (`2026-07-24-ablation` adds two
more, `aurus_campaign.py` and `aurus_validate.py`, for its AuRUS arm).
Recording the commit alone was
enough to recover them with `git show`, but only for a reader who still has the
history; the copies make a campaign directory reproduce on its own, which
matters when one is lifted out and shipped beside a paper. 35 files across the
12 retrofitted campaigns, and `merge_experiments.py` is missing from `2026-07-10` because
it did not exist yet, which is also why that campaign has a single `results.csv`
and no per-host split.

`vendored_scripts` in each `PROVENANCE.json` records, per file, the commit it
came from, its git *blob* sha, and the basis for the attribution. The blob makes
every copy checkable against the history it claims to come from:

```sh
git hash-object experiments/2026-07-21-muc/scripts/run_experiments.py
```

Attribution defaults to `profile_commit`, and that basis is an attribution
rather than a record: the anchor dates the campaign's *configs*, so a long
campaign resumed under a later revision would not show up in it. 25 of the 36
retrofitted entries rest on it; 10 across five campaigns are settled by campaign content,
which is stronger evidence and overrides the anchor wherever the two disagree,
and the last is the `merge_experiments.py` that did not exist yet.

The first fingerprint is the CSV header. `CSV_FIELDS` in `run_experiments.py`
grew a column at a time — `timed_out`, then `selection`, `n_dropped`,
`weakening`, `metric`, `repair_mode` — so a results CSV names the class of
runner revisions that could have written it. That alone is necessary but not
sufficient, and taking it as sufficient gives the wrong answer twice: a runner
also has to define the *profile* the campaign ran, and the seed budget, spec set
and timeout caps in that profile are further fingerprints the campaign's own
rows can check.

Four campaigns come out against their anchor. `factorial` carries `n_dropped`,
a 15th column added 6 minutes after its profile commit, so its runner is
`52167cb` and not `f51adeb`. `wellsep` splits seeds 0-159 and 160-319 across the
two hosts, which is `f64507ca`'s 320-seed design rather than the profile
commit's 160, and all 11 of its `lift` timeouts sit at 600s rather than 180s,
which is the cap `6f0dfbe` raised 45 minutes into the run.

`genpop-sweeps` and `tlsf-genpop` are the awkward pair, because neither ran from
`main` at all. Both come off the unmerged TLSF branch, which survives only as
*dangling* objects, and both anchors are the rebased forms — the rebase pulled
`cd825ae4`'s `repair_mode` crossing underneath them, so the anchors yield
18-column runners where the CSVs have 17. No 17-column revision on `main`
defines a TLSF profile at all, which is the check that catches the mistake.
`tlsf-genpop` resolves cleanly to `8b52ea2`, whose `gen_configs.py` regenerates
all 8 of its configs byte-identically under `--tlsf`. `genpop-sweeps` splits
across two commits: its grid carries no `ltlsynt_timeout_ms`, so it was written
by `d6e8e9d`, 22 minutes before `fe3aab5b` added a 30000 ms default, while its
rows run to seed 35 and so need the 60-seed budget `fe3aab5b` set. Grid first,
seed budget raised, launch 5 minutes later.

Six of the 36 retrofitted entries therefore name commits that are not ancestors of `main`,
flagged by `reachable_from_main: false`. They are held reachable by the
annotated tag `provenance/tlsf-branch`, which points at that branch's tip —
every one of the six is an ancestor of it, so a single ref pins them all.
Without that tag they are dangling objects, `git gc` collects them, and the
shas stop resolving. The tag has to be pushed for that to hold for anyone else;
a clone that lacks it still has the vendored files, since those live in the
commit, but loses the ability to check them against the history they name. That
is the case where vendoring earns its keep rather than merely saving a
`git show`.

The same reasoning closes `2026-07-10`, whose `profile_commit` is null because
the profile mechanism did not exist. Its `run.log` files cannot predate
`cacd979`, which introduced them, and its CSV holds sweep-A gen40 rows for
`fsm-timing` (29) and `fsm-combined` (6) that `d7cb33f`'s `SKIP_COMBOS` would
have excluded — a bound from each side, with exactly one runner revision
between them. The truncated gen40 counts are equally consistent with the run
having continued under `d7cb33f` afterwards, so late rows may come from a later
revision.

None of this touches `binary_commit`, which stays null in every inferred file
for the reasons above; `2026-07-24-ablation` is the one non-null, because its
binary was recorded at launch rather than inferred. A vendored driver says
what orchestrated the runs, not what `counter` binary they invoked.
