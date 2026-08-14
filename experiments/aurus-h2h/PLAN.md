# counter against AuRUS: head-to-head on the AuRUS paper's 26 unrealizable TLSF specifications

Pre-registered 2026-08-14, before any row at the campaign commit was collected. The decision rule in §5 is fixed here so it stays checkable against the result it was meant to bind.

## 1. Question

AuRUS (Brizzio et al.) repairs unrealizable *linear temporal logic* (LTL) specifications by genetic search, over the same *Temporal Logic Synthesis Format* (TLSF) inputs counter reads. Both tools return a set of candidate repairs for one unrealizable specification, so a repair-quality rate over a shared corpus compares them directly.

The question is whether counter's per-family repair-quality rate differs from AuRUS's on the 26 specifications the AuRUS paper evaluates. counter can run 11 of the 26 today; the remaining 15 are named in §3.1 rather than dropped from the scope, and the AuRUS arm runs all 26 regardless.

A first pass at this ran in `experiments/2026-07-24-ablation/`. Its AuRUS arm sat at three settings away from the configuration Brizzio et al. publish, its corpus was 12 families chosen by what counter could already parse, and its counter arm sat at a per-run cap of 600 s against AuRUS's 3600 s. §2 corrects all of that, and the July numbers are not carried forward (§7.5).

## 2. What changed from the 2026-07 campaign

Three deviations from AuRUS's published configuration were found in July's run. `experiments/2026-07-24-ablation/REPORT.md` records them; two were real and are corrected here.

**`-k`, the model-counter bound.** July ran 10 against the paper's 20, while counter's own `model_counting.default_bound` is 20. July therefore counted traces to half the depth on AuRUS's side of the same comparison. It is 20 here, in `BASE_FLAGS` in `scripts/aurus_campaign.py`.

**`-GATO`, AuRUS's own timeout.** July ran 3600 s against the paper's 2 hours. It is 7200 s here.

**`GA_GENE_MUTATION_RATE=0`.** July recorded this as an unresolved deviation. It is the paper's setting. `SpecificationMutator` computes `MR = max(1, ((100 - rate) / 100) * n)`, so a rate of 0 gives `MR = n`, and `GeneralFormulaMutator` gates each gene on `nextInt(MR) == 0`, a probability of 1/N. The 0 is a sentinel for the default, and July matched the paper here.

counter's per-run cap moves from 600 s to 7200 s, which removes the budget asymmetry July reported as favouring the baseline. §7.3 states what a matched cap does and does not buy.

The corpus also moves, from the 12 families July ran to the 26 rows of the paper's own evaluation tables (§3).

### 2.1 Where the AuRUS configuration comes from

The flag set is read off the authors' own drivers rather than the paper's prose. `scripts/legacy/run-all-together.sh`, `run-all-sensitivity-syntcomp.sh` and `run-spectra-icse2019.sh` are the record of how the published numbers were produced, and all three agree on `-Max=1000 -Gen=1000 -Pop=100 -k=20 -GATO=7200 -addA -geneNUM=0`. That settles both corrections above independently of the prose: `-k=20` and `-GATO=7200` are what the drivers pass. `-geneNUM=0` restates the shipped default and is a no-op kept for fidelity to the record. `-removeGuarantees` appears in none of the three, so AuRUS never deletes a guarantee (§7.6).

The drivers disagree on two flags, handled differently.

**`-onlyInputsA` is matched per group.** It restricts generated assumptions to input variables, and `run-all-together.sh` passes it for the nine specifications it drives: the five literature ones plus GyroV1, GyroV2, Humanoid458 and Humanoid531. Neither of the other two drivers passes it, so the other 17 run without it. The archived settings string confirms the split from the far side, reading `only_inputs_in_assumptions=true` for `rg1` and `false` for `ltl2dba27`.

**`-factors` is a deliberate departure.** The four weights are STATUS, SYN, MC_strengthen and MC_weaken, and the drivers do not agree on them: `run-all-together.sh` omits the flag and takes the shipped `0.7,0.1,0.1,0.1`, `run-spectra-icse2019.sh` passes an equivalent `0.7,0.1,0.2` under an older three-value CLI, and `run-all-sensitivity-syntcomp.sh` passes `1,0,0` — status alone, with no similarity pressure at all. Running that last one would set counter, which weights syntactic and semantic similarity, against an AuRUS told to ignore both across half the corpus, and would flatter counter on repair quality for a reason unrelated to search. All 26 therefore run at `0.7,0.1,0.1,0.1`, stated explicitly rather than left to the default, so the archived settings string records the choice. §7.8 states what it costs.

The legacy three-value spellings also do not run against the current build. `-factors` now requires four values, and a three-value argument prints usage and exits, so the drivers cannot be copied verbatim.

## 3. Design

Two arms over one declared corpus of 26 specifications, run and scored separately, compared at the family level.

| | counter arm | AuRUS arm |
|---|---|---|
| corpus | 11 of the 26 (§3.1) | all 26 |
| driver | `campaign.py` phase, profile `aurus-h2h` | `scripts/aurus_campaign.py` |
| operating point | gen 10 / pop 200 | `-Max=1000 -Gen=1000 -Pop=100 -k=20 -addA -geneNUM=0 -factors=0.7,0.1,0.1,0.1`, plus `-onlyInputsA` on nine |
| selection | `nsga2-truncate` | n/a |
| metric | `log` | n/a |
| sweep level | C default | n/a |
| replicates | 20 seeds (av2 0–9, av3 10–19) | 30 repeats (av2 0–14, av3 15–29) |
| runs | 11 × 20 = 220 | 26 × 30 = 780 |
| jobs | 1 | `--concurrency` 10 |
| cap | 7200 s wall kill-switch | `-GATO=7200`, killed at +300 s |

The 26 are the rows of the paper's evaluation tables, in three groups.

| group | specifications |
|---|---|
| Literature (5) | Arbiter, MinePump, RG1, RG2, Lift |
| SYNTCOMP (13) | Detector, Full Arbiter, Lily02, Lily11, Lily15, Lily16, Load Balancer, ltl2dba_R_2, ltl2dba_theta_2, ltl2dba27, Prioritized Arbiter, Round-Robin, Simple Arbiter |
| SYNTECH15 (8) | GyroV1, GyroV2, Humanoid458, Humanoid531, Humanoid503, Humanoid741, Humanoid742, PCarV2-888 |

Seventeen of the 26 carry upstream reference repairs and 9 carry none. The mapping from each campaign name to its TLSF file is `SPEC_TLSF` in `scripts/aurus_campaign.py`, and every path there is root-relative to the AuRUS checkout, the tree spanning both `case-studies/` and the loose `examples/` root. Six names carry an `-aurus` suffix because counter already holds a family of that name at a different SYNTCOMP parameter instance, and reusing the bare name would assert a correspondence that does not hold.

**Three of July's families leave the head-to-head.** `amba`, `codesample-un1` and `codesample-un2` are not rows in the paper's tables. Each keeps its counter family and its ideals, which serve the ablation corpus; `amba`'s shipped ideal failed `lint-ideals` on both `separated` and `reachable`, and was rewritten this session as `add-lock-implies-request.tlsf`, which passes. `ltl2dba27` is a paper row and stays, imported from `case-studies/syntcomp-unreal/`.

**The AuRUS arm is not a `campaign.py` phase.** Phases invoke `run_experiments.py` with a profile, and AuRUS runs through its own script, so `experiments/aurus-h2h/campaign.toml` declares the counter phase alone.

**The AuRUS arm runs first, on its own.** counter is still under change, and the AuRUS side is not. Every repeat's `out.txt` is archived, so a family imported into counter later re-scores AuRUS runs that already exist rather than needing new ones, and the two arms need not be in flight together.

**AuRUS is not seedable.** Its randomness is `Math.random()` with no command-line override, so the 30 repeats are independent runs rather than seeds and cannot be paired with counter's. The split across hosts is therefore by repeat index, through `--repeat-offset`: av2 takes `--repeats 15 --repeat-offset 0` and av3 `--repeats 15 --repeat-offset 15`. Without the offset both hosts number their repeats 0–14 and half the machine time produces duplicate indices.

**`jobs = 1` on the counter arm.** `ltlsynt` runs multi-GB per call on these specifications and `max_concurrent_realizability` is a per-process cap, so a second concurrent counter would double the memory ceiling.

### 3.1 The counter arm is staged, and the gap is named

`H2H_TLSF_SPECS` in `scripts/run_experiments.py` is the declared scope of 26. `H2H_PENDING_IMPORT` is the 15 counter cannot yet run, and `H2H_TLSF_READY` is the remaining 11 the counter arm runs today: `arbiter-aurus`, `lift`, `minepump`, `rg1`, `rg2`, `lily02`, `ltl2dba27`, `gyro-var1`, `gyro-var2`, `humanoid-458` and `humanoid-531`. `test_experiment_paths.py` asserts that ready and pending partition the corpus exactly, and that no pending family already has an `examples/` directory, so importing one fails the tests until it is struck off the pending list.

Two obstacles account for the 15, neither structural. Seven parse today and have no upstream reference repair at all, so each needs a hand-written weakening ideal: `lily11`, `lily15`, `lily16`, `humanoid-503`, `humanoid-741`, `humanoid-742` and `pcar-v2-888`. The other eight are full-format TLSF that counter's parser rejects on the GLOBAL block; `syfco -f basic` lowers them to a form it parses, and AuRUS ships a working `syfco` at `lib/syfco`. Six of those eight carry upstream references, but AuRUS references replace rather than weaken as a rule, so each still needs validating with `lint-ideals` before it can serve as an ideal. The realistic range is 9 to 15 ideals to hand-author.

### 3.2 Ideal set changes made for this campaign

The ideals in `examples/<spec>/fixes/` are the scoring target for both arms (§4), so every change to them is recorded here before launch.

**`codesample-un2/fixes/codesamples-v1a-forklift.tlsf` removed.** It is over a different signature: the input `emgoff` is renamed to `atstation` and the outputs gain an unused `lift_0` and `lift_1`. It passes the weakening check vacuously, by dropping the one guarantee that mentions `emgoff`. The family is out of this corpus, so the removal bears on the ablation corpus alone.

**Nine ideals removed as redundant.** `lint-ideals` reports each as strictly stronger than a surviving sibling, or equivalent to one: `arbiter/weaken-liveness-gr1`, `lift/add-liveness`, `lily02/lilydemo04`, `lily02/lilydemo05`, `lily02/lilydemo06`, `minepump/fixed1`, `minepump/fixed2`, `rg2/weaken-gf-not-cancel`, `simple-arbiter/weaken-sequential-grants`. None of them can change `implies_ideal` or `n_implies` for either tool. They can change `best_relation` from Equivalent to Stronger, where a repair matched a removed ideal exactly.

Every family retains at least one ideal.

## 4. How scoring works

There is one scoring mechanism. `scripts/aurus_validate.py` scores AuRUS's repairs against counter's `examples/<spec>/fixes/`, through the same `compare` binary counter's own runs go through, importing that tool's output parser rather than copying it. counter's `implies_ideal` and AuRUS's `implies_genuine` are therefore the same statistic computed over two repair populations.

A run scores 1 when at least one of its repairs is equivalent to, or strictly stronger than, at least one ideal for that family, under *assume-guarantee contravariance*. It scores 0 otherwise, including when the run returns nothing.

The scorable set is `H2H_TLSF_READY`, not the whole corpus: a family with no `examples/` directory has no ideals to score either tool against. AuRUS's runs on the other 15 are archived unscored and become scorable as families land.

AuRUS's own `case-studies/*/genuine/` directories are the historical provenance of the files in `examples/*/fixes/` and are never read at scoring time. §7.1 states what that provenance costs the comparison.

## 5. Decision rule

Fixed before launch.

**Primary.** The per-family repair-quality rate: counter's `implies_ideal` over its 20 seeds against AuRUS's `implies_genuine` over its 30 repeats, giving one rate per tool per family. The two vectors of rates are compared by a two-sided *Wilcoxon signed-rank test* over families, alpha 0.05. The family is the unit of analysis, so the unequal replicate counts enter only through the precision of each rate.

The test runs over the families that have both rates when it is run, which is 11 at launch and at most 26. Adding a family means re-running the test over the larger set, and the pre-registered rule is the same rule at either size.

Three outcomes, all reportable:

1. **counter higher.** p < 0.05 with counter's rates ranking above AuRUS's. Reported as a measured advantage on this corpus at this operating point, qualified by every threat in §7.
2. **AuRUS higher.** p < 0.05 in the other direction. Reported as measured, with no re-run and no post-hoc arm added to recover it.
3. **No separation.** p ≥ 0.05. The two tools are not distinguished by this design on this corpus.

**A null result is a result.** Outcome 3 is reportable as it stands and is not grounds for re-running the campaign with more seeds, more repeats, or a different operating point. Importing a pending family is the one thing that changes the input to the test, and it changes it by widening the corpus rather than by re-reading the same one.

**Power is bounded by the family count.** The test reads only the families whose two rates differ, so a design where fewer than 6 families separate cannot reach alpha 0.05 in either direction: six all-same-sign pairs give a two-sided p of 2/2^6 = 0.031, and five give 2/2^5 = 0.063. At 11 scored families that bound is materially tighter than at 26 — better than half the scored families must separate, and all in one direction, for any p under 0.05 to exist. If fewer than 6 families show a non-zero difference, the outcome is 3 by construction and is reported that way rather than as an absence of evidence about the tools.

## 6. Secondary measures

Reported, not gating. None of these can move the outcome in §5.

**Solutions returned per run.** July measured an AuRUS median of 291 solutions against counter's median of 4 maximal repairs. The two counts mean different things — counter reports the maximal specifications surviving its implication filter — and the figure is recorded for the record rather than compared.

**Timeout rate per tool.** The fraction of runs hitting the 7200 s cap, per family and per arm.

**Re-validation rate of AuRUS's claimed solutions.** `ltlsynt` is re-run over every solution AuRUS reports. July re-validated 66,372 of 66,374, with both disagreements on `rg1`.

## 7. Threats to validity

Recorded before the fact, so none is discovered afterwards in the shape of whichever result arrives.

**7.1 The ideals are counter's curated set.** They descend from AuRUS's `genuine/` repairs, then were gated on `realize`, renamed, pruned (§3.2), and in places replaced by hand-written weakenings. Both tools are scored against the same files by the same predicate, and the files were curated by one side of the comparison. This is the sharpest threat in the list, and it applies to every outcome in §5 including the null one. It grows with each pending import, since 9 of the 26 have no upstream reference to descend from at all.

**7.2 Several families rest partly or wholly on hand-derived ideals.** After commits `2e3969d` and `3833dd9`, `implies_ideal` on `humanoid-458`, `humanoid-531` and `arbiter-aurus` rests partly or wholly on hand-derived weakenings rather than verbatim AuRUS repairs. `ltl2dba27` rests wholly on an ideal hand-written for this campaign: its two upstream "genuine" repairs are other Acacia+ benchmark instances, `G p <-> G F acc` and `F p <-> G F acc` against the specification's `F G !p <-> G F acc`, and taking `p` always true and `acc` always false makes the original hold while the repair does not.

**7.3 The matched cap is not matched compute.** 7200 s is not counter's termination criterion: counter stops after `generations` rounds and the cap is a kill-switch. The matched cap means neither tool is censored by the clock. It does not mean the two consumed equal compute, and a genuinely compute-matched arm would raise counter's `generations` until its wall time met AuRUS's. That arm is not run here.

**7.4 The AuRUS replicates are not seeds.** They cannot be paired with counter's, so every comparison in §5 is between two independent rates rather than within a pair, and host effects do not cancel.

**7.5 Numbers do not compare to the 2026-07 campaign.** Four engine changes since then move counter's search: `status_grading` defaults to `mrs` rather than `tiered`, well-separation folded into the status score, `p_remove_guarantee` arrived at 0.05, and `weight_halstead` was removed with the objective it weighted. The ideal set and the corpus also changed (§3, §3.2). Cost figures additionally differ by the cap change and by `ltlsynt_timeout_ms` moving from July's 500 ms to 10000 ms, which is material on its own: July's `amba` zero-yield was attributed to the 500 ms budget rather than to the search.

**7.6 The two tools do not have the same operator set.** `-removeGuarantees` appears in none of the authors' drivers and is not passed, so AuRUS never deletes a guarantee. counter deletes one at `p_remove_guarantee = 0.05` by default since 2026-08-13, and the root `CLAUDE.md` records that some repairs are reachable no other way — every `drop-*` ideal in `examples/` deletes a guarantee. A rate difference on a family whose only ideal is a deletion is a difference in operators before it is a difference in search.

Symmetry was available and was declined before launch: writing `p_remove_guarantee = 0.0` into this profile's config would have matched AuRUS's operator set exactly, at the cost of running counter in a configuration it does not ship. The decision is to leave counter at its shipped default and carry the asymmetry as a stated threat, so both tools run as their authors configure them. Any family whose ideals are all deletions should be read with that in mind rather than as evidence about search quality.

**7.7 Well-separation is screened on one side only.** The 17 specifications that run without `-onlyInputsA` let AuRUS assume over its own outputs, so it may return a repair the system satisfies by defeating its own assumptions. counter's output gate rejects an ill-separated survivor unconditionally. `aurus_validate.py` re-checks realizability alone, so the size of that gap is currently unmeasured; a well-separation column is planned and needs a small flag on the `realize` binary, no CLI exposing that check today.

**7.8 The `-factors` departure is ours.** Uniform weights of `0.7,0.1,0.1,0.1` across all 26 are a deviation from what the SYNTCOMP and spectra drivers record, made to keep the objective comparable between the tools (§2.1). It means the AuRUS arm is not a reproduction of the paper's SYNTCOMP numbers, and any row that disagrees with a published one has this as its first candidate explanation.

**7.9 A staged counter arm is not a random subset.** The 11 ready families are the ones counter could already parse and already had ideals for, which correlates with their being smaller and better studied. The 15 pending skew towards full-format SYNTCOMP specifications and towards cases with no upstream reference. A result over the ready subset is a result about that subset, and is not an estimate of the result over all 26.

## 8. Launch

The AuRUS arm goes first and stands alone, split by repeat index across the two hosts.

```sh
# av2
python scripts/aurus_campaign.py --aurus-root ~/projects/tools/aurus \
    --repeats 15 --repeat-offset 0 --gato 7200 --out-root <aurus-out> \
    --spot-bin ~/projects/counter/build-release/third_party/spot/bin

# av3
python scripts/aurus_campaign.py --aurus-root ~/projects/tools/aurus \
    --repeats 15 --repeat-offset 15 --gato 7200 --out-root <aurus-out> \
    --spot-bin ~/projects/counter/build-release/third_party/spot/bin

python scripts/aurus_validate.py --aurus-out <aurus-out> \
    --out-csv experiments/aurus-h2h/results-aurus.csv
```

**Prerequisite: AuRUS must be present and built on each host.** It needs Java 18 or later, Spot's `ltl2tgba` and `autfilt` on `PATH` via `--spot-bin`, and an `ant compile` that reaches the network for its dependencies. Its presence on av2 and av3 has not been re-verified since July, so check both before launching rather than reading a failed run back out of the logs.

The counter arm goes through `campaign.py`, off `experiments/aurus-h2h/campaign.toml`, which holds the only copy of the seed split. It launches when the AuRUS arm is done, and is re-run per family as pending imports land.

```sh
python scripts/campaign.py stage aurus-h2h
python scripts/campaign.py enqueue aurus-h2h
python scripts/campaign.py status
python scripts/campaign.py collect --profile aurus-h2h
```

The two arms must not co-schedule: wall time and timeout rate are reported measures on both (§6). Fresh results CSV and results directory on the counter arm, since resume skips by CSV key and never cleans output directories, so a stale row survives an engine change. Run the `ltl2tgba` orphan janitor for the duration, this being a TLSF campaign at a 7200 s cap.

## 9. Provenance

Campaign directory `experiments/aurus-h2h/`, tracked contents: this `PLAN.md`, `campaign.toml`, `PROVENANCE.json` written at launch, and `scripts/` — verbatim copies of `gen_configs.py`, `run_experiments.py` and `merge_experiments.py`, with blob shas recorded. `aurus_campaign.py` and `aurus_validate.py` are vendored alongside them, the AuRUS arm being unreproducible without both.

Both hosts run the campaign commit on branch `feat/aurus-h2h`, rebuilt before launch. `campaign.py stage` verifies each host's binary reports the declared commit with `dirty=0`, and every CSV row carries `commit` and `dirty`. A counter arm launched in stages carries more than one commit across its rows, so the family list each launch covered is recorded with it.

The archived configs record only what the profile overrides; every other value comes from the binary's default at run time, so reproducing this campaign requires the commit `PROVENANCE.json` names. The AuRUS side is pinned by its own checkout, whose commit `PROVENANCE.json` records beside counter's.

A head-to-head against a tool whose repairs supplied the scoring targets can only ever be reported with §7.1 attached to it. Writing the decision rule down before the rates exist is what keeps that caveat, and the 15 families still missing from one arm, from being negotiated after they do.
