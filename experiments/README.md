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
| `2026-08-07-elitism` | Whether the shipped `genetic.elitism_rate` default should be 0.0 or 0.1 — sweep R alone, `nsga2-truncate` only, 2,794 paired runs over 24 spec families across av2 and av3, decision rule pre-registered in its `PLAN.md`. **0.1 is kept, on cost alone.** Quality and yield are flat or lean *against* it (FRETISH `implies_ideal` 0.593 at 0 vs 0.570 at 0.1; TLSF `found_repair` 0.746 vs 0.714, interval excluding zero), so the trade is +3.3pp TLSF yield for +16% wall time and criterion 4 is what decides. The `n=1` lily02 anecdote that used to justify the default is retired: tautological repairs appeared in *both* arms (3 at 0, 4 at 0.1), and were black answering validity wrong on weak-until, fixed in `fb4c3ed` after the campaign ran. Two things limit it — the TLSF quality pass is carried by 14 of 20 families that score 0 in both arms, and the FRETISH quality column existed only after `compare` stopped choking on `run.json` (`2ebe3a9`), which had zeroed all 927 scored rows. Its commits are not ancestors of `main`; `provenance/elitism-campaign` holds them. |
| `2026-08-10-arbiter-probe` | **Decision: spec-specific — the unlock is `examples/arbiter`, not the arbitration family.** `arbiter` reproduces at full strength (0.000 against 0.950 over 40 pairs, 38 discordant all one way, Holm-corrected p < 0.0001), but only 1 of 9 families clears the bar against a pre-registered threshold of 3, so criterion 1 fails; criterion 2 passes, nothing reversing. The pattern is not arbitration-versus-not and should not be read that way: `load-balancer` went 8 against 0 and missed only on the correction (raw p = 0.0078), and `lily02` — the counter-signal control — moved *with* apportion on yield (10 against 1) and on quality (`implies_ideal` 0.525 → 0.925), the opposite of what it did in `2026-07-31-replicate`. Quality is mixed and stated both ways per §7: `arbiter-aurus` loses (0.925 → 0.575) at unchanged yield, `load-balancer` and `lily02` gain. Cost is not the obstacle this time — pooled median paired wall ratio 1.13, against the ~3× that failed replicate's bound. Whether `nsga2-apportion`'s yield advantage on `examples/arbiter` — 120/120 against `nsga2-truncate`'s 0/120 in `2026-07-31-replicate`, the only place either NSGA-II scheme has separated on `found_repair` — is a family-level effect or a property of that one spec. 800 TLSF runs (10 families, 40 seeds, gen10/pop200, both schemes) across av2 and av3, over the nine fixes-backed arbitration families the corpus now holds plus `lily02` as the counter-signal control, apportion having cost it `implies_ideal` 1.00 → 0.44. It carries no compute-matched arm: `2026-07-31-replicate` failed its decision rule on cost and on that control, and this probe does not reopen the default. The decision rule is pre-registered in its `PLAN.md` with a named deliverable for each of its three outcomes. First campaign archived under the post-2026-08-06 scheme spellings, and the first to run with well-separation folded into the status score (`b101ada`) — so `run_well_separation` inherits the restored `false` default and the filter stage is absent by design. Two earlier executions of the same design completed and were discarded rather than archived. The first, at `536a3ae`, predated `d7733fc`, `fb4c3ed` and `b101ada`, the last of which rescales the status objective that selection reads, putting its numbers on the wrong side of that scale. The second, at `df66e44`, lost 66 of its 800 runs to a defect in `fb4c3ed` that this campaign was the first to exercise: the new weak-until rewrite is the first path on which a SPOT-printed string reaches `black`, and `black` rejects SPOT's `0` for false as a syntax error. A run aborts only where the search reaches such a formula, so the loss is neither random nor balanced — 38 apportion against 28 truncate, with `round-robin-arbiter` down from 40 paired seeds to 23 — and dropping the unpaired rows would bias the paired comparison the decision rule reads. `b093374` fixes it and is the commit the campaign now runs at. |

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

The same commit changed what the status *scale* means rather than what a config says, which no amount of writing keys back repairs. `k_status_realizable` (1.0) now requires a candidate to be well-separated as well as realizable, and one the system can satisfy only by forcing its own assumptions to fail scores `k_status_unrealizable` (0.5) — level with a candidate no strategy exists for, where it previously scored the top tier. Status is a selection objective on both paths, so status-derived numbers and every quality figure downstream of selection are on different scales either side of this commit and do not compare across it.

Three more moved in the commit that narrowed the defaults for push-button use:
`ltlsynt_timeout_ms` `0` → `500`, `ltl2tgba_timeout_ms` `0` → `60000` and
`max_scoring_failure_rate` `0.05` → `0.15`. Every TLSF campaign archived here
already set all three to those exact values, so the TLSF archives are
unaffected and reproduce as they stand. The FRETISH campaigns state none of
them, and so now run under bounded tools where they ran unbounded; write the
three in as `0`, `0` and `0.05` to reproduce one.

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
