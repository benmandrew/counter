# 2026-09-04-aurus-rematch

Pre-registered 2026-09-04, before any row of this campaign existed. `2026-08-29-aurus-matched` registered no rule and every p-value it reports is post-hoc by its own declaration. This one registers a rule, and the reason it needs one is that it runs after a campaign that read null: a second head-to-head arriving in that position is exactly where a reader looks for a design chosen once the first answer was known.

## 1. Why this campaign exists

`2026-08-29-aurus-matched` killed 315 of 3000 runs (10.5%) at the 7200 s cap, and those kills consumed 54.7% of its 1152.1 core-hours. A kill scores `implies_ideal = 0`. The kills concentrate in the families AuRUS wins: `humanoid-742` was killed on all 120 runs and scored 0.000 against AuRUS's 0.933, and the archive's own report concedes that nothing in it can say what counter finds there.

`1330f13` gives the final filter stage its own satisfiability checker with the `ltlfilt --simplify` pass off. Measured on `humanoid-531` at 6 generations of 100, seed 0, that stage was 264 s of a 312 s run and 96.7% of its solver time was the simplify pass. So a large part of what the cap was truncating was a pass that settled nothing the `--satisfiable` decision could not.

Re-running is therefore motivated by a measured defect in the previous measurement. It is not motivated by the previous result, and section 5 is what holds that distinction to something checkable.

## 2. The engine moved, so this is a new measurement

Six commits between `ec0abe0` and this campaign's base move the search path: `0ef4d47` (FRETISH scopes in the requirement model), `20d83e6` (scope and condition type in syntactic similarity), `9d70449` (the Always weakening operator), `ed3b5dd` (fitness scheduling), `720b4a9` (the `aurus` status ladder, a new option whose default is unchanged) and `332a621` (accumulator instrumentation). `ed3b5dd` carries a determinism check in its own message, byte-identical repair output over 12 paired runs and no draw from the `RandomSource`, so it is the one of the six that can be argued inert. The others cannot.

`4ad290d` also collapses mutually equivalent specifications in the implication filter, which moves `n_repairs` and `n_implies` on their own and bumps `run.json` to schema 23.

Two consequences, both registered rather than discovered:

- **Nothing here pairs against the `aurus-matched` rows.** Not on `(spec, seed)`, not on anything. That archive is a different engine and its rows are reported beside these, never merged with them.
- **No result here is attributable to the filter speedup.** The campaign changes one binary for another and the binary differs by nine commits. A reader who wants the speedup isolated needs a different campaign, and this one does not pretend to be it.

## 3. Design

The `aurus-matched` 2x2 unchanged: `selection_scheme` (`nsga2-apportion`, `weighted`) against `fitness.status_grading` (`mrs`, `aurus`), over the 25 `H2H_TLSF_READY` families at seeds 0-29, 750 runs a cell and 3000 in all, at `termination = "individuals"`, `max_individuals = 1000`, population 100, `runtime.parallel = 1`, weights 0.1/0.2/0.7, weakening screen off, 7200 s cap, `compare_timeout = 1800`.

The generator line is `aurus-matched`'s with the output directory changed, so the configs differ from that campaign's in nothing at all. Verify that before launching: the emitted `sweep_G_*.toml` should be byte-identical to `experiments/configs-matched/`'s, and a difference means a default moved under `--pin-vintage` and has to be explained before the campaign runs.

The AuRUS arm is re-run rather than re-used, for three reasons. Its archived rows were taken a week apart from counter's on hosts whose load was uncontrolled; 173 of its 780 runs (22.2%) were lost at the cap and are unrecoverable without a re-run; and its rates in the archive are unfiltered by the well-separation screen because those inputs were lost.

## 4. Primary endpoint

Per-family `implies_ideal`, counter's `nsga2-apportion`/`mrs` cell against the AuRUS arm, over the 25 families, read with an exact two-sided *Wilcoxon signed-rank* test at alpha 0.05.

The primary read counts a killed or capped run as a failure on **both** sides. That is the question the paper asks — what the tool delivers inside a fixed budget — and it is the read that does not flatter counter, whose kill rate is the lower of the two.

Registered secondary, reported always and never substituted for the primary: the same test over the families scorable on both sides, which conditions on completion and so isolates search quality from speed. Both reads are reported together whether or not they agree, with the sign and tie counts beside each.

`nsga2-apportion`/`mrs` is named as counter's arm here, in advance, because it is the shipped configuration. Picking the best of four cells after seeing them is four looks at one endpoint reported as one.

## 5. Decision rule

- **Outcome 1, counter higher on the primary at p < 0.05.** Reported as counter ahead, with the margin explicitly labelled as including counter's speed advantage at equal wall budget, and with the secondary read given beside it. If the secondary is null, the paper says the advantage is throughput rather than search.
- **Outcome 2, AuRUS higher on the primary at p < 0.05.** Reported as measured.
- **Outcome 3, null.** Reported as parity, with the discordant counts.

In every outcome the paper reports this campaign **and** `2026-08-29-aurus-matched`, names the engine change as the reason for the re-run, and states that the earlier campaign read null. A supersession row goes in `experiments/README.md`. No outcome licenses reporting this campaign alone.

The 2x2 contrasts and every per-family number are secondary and carry no rule. They describe this sample.

## 6. Registered hazards

- **The corpus limits what any of this can detect.** In the archive, 8 of 25 families sat at the ceiling on `implies_ideal` in all four cells and 10 at the floor; seven carried the contrast. If the speedup lifts the floor families, that changes, and if it does not, a null here is mostly a fact about the corpus.
- **The archive's ablation is confounded by its own kill rate** and this campaign is the test of that. Kills ran 5.2% to 17.7% across the four cells, worst in the slowest, and a kill scores zero, so the reported `nsga2` advantage is partly a speed artefact. If the speedup compresses the kill rates together and the selection contrast shrinks, that is a finding about the archive and gets reported as one.
- **AuRUS cannot be sped up the way counter was.** Its 22.2% censoring stands. Equal wall budget is the fair comparison and is what section 3 runs, but the asymmetry is stated in the paper rather than left for a reader to find.
- **`--pin-vintage` is load-bearing** and nine commits have landed since the archive. A default that moved and is not pinned would enter as an undeclared factor. Section 3's config diff is the check.

## 7. Calibration gate

No campaign has run at this engine, so the archive's 1152.1 core-hours is an upper bound of unknown tightness and the main phase is not launched on it. The `calib` phase runs 48 runs first: six families at 2 seeds across all four cells, chosen on the archive's measured cost and on nothing else, spanning `humanoid-742` (60.0 core-hours in the archive's control cell), `humanoid-531` (57.8), `pcar-v2-888` (41.4), `full-arbiter-aurus` (23.3), and `minepump` and `rg2` for the light half.

The gate: extrapolate the calibration's mean wall per family over 25 families, 30 seeds and 4 cells. Launch the main phase at or below 900 core-hours. Above that, cut seeds from 30 to 20 before cutting anything else, because the corpus and the cells are what the endpoints rest on and the seed count is the only free parameter that costs nothing but precision.

Those six were 183.6 of the archive control cell's 253.4 core-hours, 72.5% of its bill, and span a mean run of 39 s (`rg2`) to 7200 s (`humanoid-742`). At 2 seeds across 4 cells the calibration's own worst case, priced at the old engine and assuming the speedup buys nothing, is about 49 core-hours, roughly 1.6 h of wall clock. It is cheap because the families that make it expensive are capped either way.

Record what the calibration measures for the kill rate as well as the cost. If `humanoid-742` still caps on 8 of 8, the speedup did not reach the family this campaign exists to recover, and that is worth knowing before spending the rest.

### Amendment, 2026-09-04, before any row existed

The campaign was launched unattended, so **the gate above was not enforced and is recorded here as unenforced**. `campaign.py` runs a declaration's phases in order without pausing between them, and both phases were queued together, so `main` follows `calib` whatever the calibration says. The cost ceiling this gives up is bounded by the archive: 1152.1 core-hours at the old engine on the same design, about 37 h of wall clock over the two hosts, and the speedup can only reduce it.

What the calibration still buys, and what to read first when the campaign is collected: whether `humanoid-742` and `humanoid-531` still cap, which is the question the whole campaign turns on, and whether the realised cost matches the archive or falls well under it. Both are answerable from `results-rematch-calib.csv` alone, and neither needed the gate to be useful. If the calibration shows the caps unmoved, the `main` rows will have been spent measuring a corpus the speedup did not reach, and that is a cost this amendment accepted in advance rather than a surprise to be explained afterwards.

Nothing about the endpoint, the test, the decision rule or the corpus is changed by this amendment. It records a deviation in execution, not in analysis.

## 8. The maximality pass is a second campaign and is budgeted here

`maximal_solutions` and `maximal_ideal_solutions` are not produced by the run. They come from an offline pass, `scripts/score_curves.py --maximality`, over every run directory, and in `2026-08-29-aurus-matched` that pass cost **716.2 worker-hours** after its rebuild alone, against 1152.1 core-hours for the campaign itself. It is not a rounding item and it is not optional: the RQ3 result — that the weighted arms' candidate surplus does not survive the maximal antichain — is entirely this pass.

Planned invocation, one per run directory, 8 workers a host on a smallest-first queue, each worker pinned to 4 cores:

```
python scripts/score_curves.py <run-dir> --maximality --cuts 20 --jobs 4 \
    --maximal-timeout 900 --compare-timeout 600 --deadline-s 4500 \
    --out <curves-out>/<run>.csv
```

Four things this campaign does differently from the archive:

1. **One scoring binary for all 3000 curves.** The archive scored 1956 curves with the pre-2026-09-03 `maximal` and 1044 with the rebuilt one, and had to argue the two agreed over a 10-run sample. `main` carries the rebuild (`1330f13` sets `set_simplify(false)` and gives SPOT `black`'s budget in `src/maximal.cpp`), so every curve here is scored once by one binary. Record its commit in `PROVENANCE.json` and do not rebuild mid-pass.
2. **Expect the pass to be much cheaper and do not assume it.** The measured basis is a 40-file batch going 304 s to 30 s without the simplify pass, with the same survivors, and the pass being 95.6% of solver wall time on these queries. Calibrate it on the 48 calibration runs before committing the other 2952, the same way section 7 gates the search.
3. **The 280 partial curves are the thing to watch.** The archive left 9.3% of curves partial past the 900 s per-cut budget, reaching 2 to 17 of 20 cuts, median 6, concentrated in the families that accumulate most, which thinned the late cuts of exactly the arms that accumulate most. If the calibration shows partials surviving at the fast binary, raise `--maximal-timeout` rather than reporting a thinned late cut.
4. **`4ad290d` changes what the pass sees.** Collapsing mutually equivalent specifications in the implication filter shrinks the candidate set before scoring. Curve values here are not comparable to the archive's, and the report says so rather than putting the two in one table.

The `compare` cap is the other inherited defect: 205 of the archive's 3000 runs had no ideal metric at any cut because `compare` exceeded 600 s. `11305f9` is the fix and is on `main`. If the calibration still shows undecided runs at 600 s, raise `--compare-timeout` before the main pass rather than reporting lower bounds again.

## 9. The AuRUS arm

Not a campaign phase — `campaign.py` rejects a phase whose profile is not in `run_experiments.PROFILES`, and AuRUS has none. Launched by hand in the same window as the counter arm, split by repeat the way the counter arm is split by seed:

```
# av2
python scripts/aurus_campaign.py --aurus-root ~/projects/tools/aurus \
    --repeats 15 --repeat-offset 0 --gato 7200 --concurrency 10 \
    --out-root <aurus-out> \
    --spot-bin ~/projects/counter/build-release/third_party/spot/bin
# av3: --repeats 15 --repeat-offset 15, otherwise identical
python scripts/aurus_validate.py --aurus-out <aurus-out> \
    --out-csv experiments/results-rematch-aurus.csv
```

AuRUS at `3f6f01f`, its published settings, `-Max=1000 -Gen=1000 -Pop=100 -k=20 -addA -geneNUM=0 -factors=0.7,0.1,0.2`, with `-onlyInputsA` on the nine families `ONLY_INPUTS_A` names. `-k=20` is the published bound and the July arm's `-k=10` is the misconfiguration that inverted that campaign's verdict; check the archived settings banner says 20 before trusting a row. `GATO` is 7200 with a 300 s kill grace, matching counter's cap. AuRUS is not seedable, so repeats are its only replicate dimension and the ordering is repeat-major, which keeps the design balanced if the arm is cut short.

Budget from the archive: 780 runs took about 47.9 machine-hours, 605 completing, 173 capped and 2 OOM-killed. Nothing about AuRUS has changed, so that estimate carries.

**Not launched with the counter arm.** `aurus_campaign.py` cannot be a campaign phase and needs a hand at a terminal, so the unattended launch of 2026-09-04 started the counter arm alone. Until the AuRUS arm runs, this campaign has no primary endpoint: section 4's test is counter against AuRUS and half of it does not exist yet. Run it before collecting, and run it on the same hosts. The counter rows keep in the meantime, the archive's AuRUS rows are not a substitute, and section 3's three reasons for re-running the arm are unaffected by the delay.

Run the well-separation screen over its output this time and keep the inputs. The archive lost them, which is why its AuRUS rates are unfiltered while counter's output gate rejects an ill-separated survivor unconditionally, and why the two sides are currently scored by different standards.

## 10. Total budget

| Phase | Basis | Estimate |
|---|---|---|
| `calib`, 48 runs | archive cost of the six families | prices the rest |
| `rematch`, 3000 runs | 1152.1 core-hours at the old engine, upper bound | gated at 900 core-hours |
| AuRUS arm, 750 runs | 47.9 machine-hours measured | 48 machine-hours |
| Maximality pass, 3000 curves | 716.2 worker-hours at the old binary, upper bound | calibrate on the 48 |

The maximality pass is the item most likely to be underestimated, and in the archive it was over half the cost of the campaign it scored.
