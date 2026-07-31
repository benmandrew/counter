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

## Driver scripts

Each campaign directory carries a `scripts/` holding the three drivers that
produced it — `gen_configs.py`, `run_experiments.py` and `merge_experiments.py`
— copied verbatim from the revision that ran. Recording the commit alone was
enough to recover them with `git show`, but only for a reader who still has the
history; the copies make a campaign directory reproduce on its own, which
matters when one is lifted out and shipped beside a paper. 35 files across the
12 campaigns, and `merge_experiments.py` is missing from `2026-07-10` because
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
entries rest on it; 10 across five campaigns are settled by campaign content,
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

Six of the 36 entries therefore name commits that are not ancestors of `main`,
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

None of this touches `binary_commit`, which stays null everywhere for the
reasons above. A vendored driver says what orchestrated the runs, not what
`counter` binary they invoked.
