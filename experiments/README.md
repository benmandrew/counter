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

The 2026-07-24 ablation + AuRUS head-to-head campaign is archived with the
paper, not here: `~/projects/writing/rumoga/data/ablation-2026-07/`.

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

## Commit attribution

Every campaign directory carries a `PROVENANCE.json`. For campaigns closed
before commit recording existed it is **reconstructed after the fact**, and
says so: `"attribution": "inferred"`. Campaigns run after that change record
their commit at run time instead, in the per-row CSV column and the per-host
manifest, and need none of what follows.

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
`binary_commit` is deliberately `null` everywhere rather than absent — the
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
