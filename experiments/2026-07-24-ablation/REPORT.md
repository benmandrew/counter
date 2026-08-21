# AuRUS head-to-head: deviations from the published configuration

Amends `PROVENANCE.txt` for the `2026-07` campaign. Written 2026-07-30, after the
archived `Settings{...}` strings were compared against the experimental setup in
Brizzio et al. The campaign's AuRUS arm departs from that setup in two parameters.
One departure is deliberate and documented; the other is neither.

## What the AuRUS paper specifies

> As AuRUS is driven by random decisions, for each experiment, we run it 10
> times. Precisely, in our experimentation AuRUS is configured as follows: the
> population size is 100, the model-counter bound k is 20, best selector, the
> crossover operator is applied to 10% of the individuals, and the mutation
> operator is applied to each individual, to which each gene (sub-formula) is
> mutated with a probability of 1/N (where N is the size of the formula). The
> termination criterion is reached either when 1000 individuals are generated or
> after 2hrs of execution time.

## What the campaign ran

Settings were parsed from the `settings` column of `aurus_results-av2.csv` and
`aurus_results-av3.csv`. Every archived value is identical across the two
batches. The invocation is fixed in `scripts/aurus_campaign.py` at
`FIXED_FLAGS = ["-Max=1000", "-Gen=1000", "-Pop=100", "-k=10", "-addA"]`, with
the timeout supplied by `--gato` (default 3600).

| Paper | Campaign | Verdict |
| --- | --- | --- |
| population size 100 | `GA_POPULATION_SIZE=100` | matches |
| best selector | `GA_RANDOM_SELECTOR=false` | matches |
| crossover on 10% of individuals | `GA_CROSSOVER_RATE=10` | matches |
| mutation applied to each individual | `GA_MUTATION_RATE=100` | matches |
| terminate at 1000 individuals | `GA_MAX_NUM_INDIVIDUALS=1000` | matches |
| 10 runs per experiment | 30 repeats per spec | exceeds, harmless |
| **model-counter bound k = 20** | **`MC_BOUND=10`** | **halved, undocumented** |
| **or after 2hrs** | **`GA_EXECUTION_TIMEOUT=3600`** | **halved, documented** |
| each gene mutated with probability 1/N | `GA_GENE_MUTATION_RATE=0` | unresolved |

Three archived settings fall outside the quoted passage and cannot be checked
against it: the fitness weights (`STATUS_FACTOR=0.7`, `LOST_MODELS_FACTOR=0.1`,
`WON_MODELS_FACTOR=0.1`, `SYNTACTIC_FACTOR=0.1`), the operator switches
(`allowAssumptionAddition=true`, `allowGuaranteeRemoval=false`), and
`check_STRONG_SAT=false`. Confirming these needs the rest of the AuRUS paper.

### The timeout was a deliberate budget cut

`PLAN.md` §5 records the decision and its reasoning:

> **AuRUS repeats 30 → 20, GATO 7200 → 3600.** [...] The 1 h cap is the lever
> that makes the phase's worst case *bounded* (260 runs / 20 slots × 1 h = 13 h)
> instead of open-ended; it is still 6× counter's 600 s cap, so the budget
> asymmetry favours the baseline and gets stated in the paper as such.

The same section lists restoration priorities for spare capacity, item (3) being
"AuRUS repeats 20 → 30 and/or GATO back toward 7200 s". The repeats were restored
and the data carries 30 per spec. The timeout was not. The plan's undertaking to
state the asymmetry in the paper is also outstanding, because the paper carries no
head-to-head section at all yet.

### The model-counter bound was not

`-k=10` appears in `PLAN.md` §5's command line and in `FIXED_FLAGS`, with no
rationale in either file, in `EXPERIMENTS.md`, or in `PROVENANCE.txt`. Nothing
found so far explains it.

It also breaks the symmetry of the comparison rather than only departing from the
published setup. Counter's own `model_counting.default_bound` is 20
(`docs/configuration.rst`), so the campaign counted traces to depth 20 for counter
and to depth 10 for AuRUS, on the same specifications. Bounded model counting
feeds AuRUS's semantic-similarity heuristic, so the bound moves both its cost and
the repairs it converges on.

## What the deviations do to each result

Four results were drawn from this data. They are not equally exposed.

**Realisability soundness is unaffected.** Of 66,374 solutions AuRUS claimed
across all runs, 66,372 re-validated as realisable, leaving 2 disagreements, both
on `rg1`. Validation runs outside AuRUS through `ltlsynt`, so no GA parameter
reaches it. This result stands as measured.

**Output size is safe to an order of magnitude.** Completing runs return a median
of 291 solutions (mean 278, range 53–479), against a median of 4 maximal repairs
(mean 4.5, range 0–14) from counter's default cell. A longer timeout admits more
generations and would if anything raise AuRUS's count, so "several hundred against
a handful" holds. The precise ratio does not, and should not be printed.

**The timeout rate is compromised in an unknown direction.** 120 of 360 runs were
killed at the cap, a pooled rate of 0.333, concentrated on four specs:
`arbiter-aurus` 28/30, `humanoid-531` 30/30, `lily02` 30/30, `rg2` 29/30, plus
`humanoid-458` 3/30. Halving the timeout inflates that rate. Halving the
model-counter bound made each run cheaper and deflates it. The two errors oppose
each other and neither is quantified, so the rate at published settings is unknown.

One observation bears on the first of those. Among the 240 completing runs,
`aurus_time_s` has a median of 131 s and a maximum of 1659 s, and not one exceeds
1800 s. Every run that finished did so inside half the budget it was given. That
makes it unlikely the timeouts are marginal cases a doubled budget would rescue,
though it is an inference from the observed distribution rather than a measurement.

**The quality comparison is the most exposed.** Pooled *implies-genuine* of 0.192
for AuRUS against *implies-ideal* of 0.300 for counter rests on a fitness signal
computed at half the intended bound. This result also carries a separate open
question: whether `implies_genuine` and `implies_ideal` scored against the same
hand-written ideals. AuRUS's `best_relation` has an `unknown` category, used 15
times, that counter's never uses. Both questions need answering before the
comparison carries weight.

## What to re-run

### Scope: the AuRUS arm alone suffices

Both deviations are AuRUS parameters, so the question is whether replacing one arm
invalidates the other. It does not, for three reasons set out below.

The comparison is unpaired by design. `PLAN.md` §4 records that AuRUS is not
seedable, since `Settings.RANDOM_GENERATOR` is built from `Math.random()` with no
command-line override. §6 accordingly specifies Fisher's exact on success rate and
implies-genuine, with Mann-Whitney on time-to-solution. Those are two-sample tests
over distributions rather than seed-matched pairs. Counter's 20 seeds and a fresh
set of 30 AuRUS repeats therefore compose with no pairing to preserve. Contrast the
RQ2 factorial, whose 25{,}193 matched triples would break if either arm were
replaced alone.

Nothing about counter's control cell was run wrongly. Its configuration is
`level_name=default`, `selection=nsga2`, `metric=log`, at 20 seeds per spec.

The counter-side gaps are documentation rather than data, and are addressed below.

### AuRUS arm — required

Two values change in `scripts/aurus_campaign.py`:

```
FIXED_FLAGS = ["-Max=1000", "-Gen=1000", "-Pop=100", "-k=20", "-addA"]
--gato 7200
```

`KILL_GRACE_S` stays at 300, so the hard kill moves to 7500 s. The corpus is
unchanged: the 12 specs of the revised head-to-head set, 30 repeats each, 360 runs.
Keep the two-batch structure so batch remains available as a blocking factor.

Cost is bounded at 360 × 7200 s over 20 slots, or 36 h wall across both machines.
The realistic figure is lower, since only about 120 runs are expected to reach the
cap and the rest completed well inside it. That estimate does not account for
`k=20` slowing the completing runs, whose factor is unmeasured — a short
calibration on `minepump` and `lift` would fix it before committing the hours.

### Counter arm — no compute, but two records to close

The counter side needs no re-run for the deviations above. Two values are missing
from the archive, and both are recoverable without running anything:

1. **The model-counting bound.** It is absent from `results-ablate-tlsf.csv`, so
   the symmetry of the comparison can only be inferred from the documented default
   of 20, never confirmed from the data.
2. **Generations and population.** These are likewise absent. The campaign plan
   puts TLSF at generations 10 / population 200 with a 600 s cap, but the archived
   rows do not say so.

Neither is a measurement. `PROVENANCE.txt` pins the binary at `db30a59`-equivalent,
so both values can be read off the configuration template and campaign scripts at
that commit and written into `PROVENANCE.txt`. Doing so costs no machine time and
closes the symmetry question. Recording them in the results CSV would stop the gap
recurring, and is worth doing before the next campaign rather than after it.

### Two conditions that would pull counter in anyway

**A change of default selection scheme.** Should `nsga2-replicate` replace
`nsga2` as the default, the head-to-head has to be re-run against it, and counter's
arm goes with it. That decision is independent of the deviations in this report. A
head-to-head published under plain `nsga2` remains sound provided it is labelled as
such. Since a counter re-run is likely on that account, scheduling both arms in one
campaign is cheaper in wall-clock than running them apart.

**Timing comparisons need the original machines.** `PLAN.md` §4 forbids
co-scheduling AuRUS and counter on one machine while wall-clock is measured, and
§6's Mann-Whitney on time-to-solution is the metric exposed. Running the new AuRUS
arm on av2 and av3 under the same sequential-phase discipline keeps counter's
existing `wall_time_s` comparable. Running it on other hardware, or against other
load, forfeits the time comparison alone. Success rate and repair quality do not
depend on wall-clock and survive either way.

### Recommended sequence

1. Calibrate `k=20` on `minepump` and `lift`, which both complete quickly at
   `k=10`. This measures the one unmeasured factor in the cost estimate.
2. Re-run the AuRUS arm on av2 and av3 at `-k=20` and `--gato 7200`, phases
   sequential per machine.
3. Amend `PROVENANCE.txt` with counter's bound, generations and population while
   the runs proceed.
4. Settle the `nsga2-replicate` question separately. If it lands before the AuRUS
   hours are committed, fold both arms into a single campaign instead.

### Reporting obligation

The budget asymmetry survives the fix and grows. Counter is capped at 600 s and
AuRUS at 7200 s, a factor of twelve, against the six the plan anticipated. This
favours the baseline, and `PLAN.md` §5 already committed to stating it in the
paper. That undertaking should be honoured wherever the head-to-head lands.

## Standing questions

- Does `GA_GENE_MUTATION_RATE=0` select the 1/N default, or a literal zero? A
  literal zero contradicts the observed search, so a sentinel is the likely
  reading, but AuRUS's source settles it.
- Do `implies_genuine` and `implies_ideal` score against the same ideals?
- What does AuRUS's `best_relation` value `unknown` mean, and how should the 15
  runs carrying it be treated?
- Why was `-k=10` chosen? If a reason exists outside the files searched, it
  belongs in `PROVENANCE.txt`.

The archived settings made both deviations findable, and the campaign plan made one
of them explicable; without the `Settings{...}` column the model-counter bound would
have gone unnoticed indefinitely. What the record does not yet capture is counter's
own side of the same parameters, which is the gap worth closing before the next run
rather than after it.
