import test from "node:test";
import assert from "node:assert/strict";

import {
  loadDashboard, log, runStart, stage, generation, realRun
} from "./harness.mjs";

const { parse } = loadDashboard();

test("routes each record type to its own slot", () => {
  const s = parse(log(runStart({ seed: 42 }), generation(1), stage(1, 0, "breed"),
                      { type: "run_end", generations_run: 1, n_realizable: 3,
                        n_maximal: 2, elapsed_s: 9 }));
  assert.equal(s.runStart.seed, 42);
  assert.equal(s.runEnd.n_maximal, 2);
  assert.equal(s.gens.length, 1);
  assert.deepEqual(Object.keys(s.stagesByGen), ["1"]);
  assert.equal(s.present, true);
  assert.equal(s.fetched, true);
  assert.equal(s.error, null);
});

test("orders generations by gen, not by position in the file", () => {
  const s = parse(log(generation(3), generation(1), generation(2)));
  assert.deepEqual(s.gens.map((g) => g.gen), [1, 2, 3]);
});

test("orders each generation's stages by index, not by position", () => {
  const s = parse(log(stage(1, 2, "score"), stage(1, 0, "breed"),
                      stage(1, 1, "dedup")));
  assert.deepEqual(s.stagesByGen[1].map((r) => r.name),
                   ["breed", "dedup", "score"]);
});

test("a later generation record replaces an earlier one with the same gen", () => {
  const s = parse(log(generation(1, { best_fitness: 0.1 }),
                      generation(1, { best_fitness: 0.9 })));
  assert.equal(s.gens.length, 1);
  assert.equal(s.gens[0].best_fitness, 0.9);
});

test("tracks the latest generation to have reported a stage", () => {
  const s = parse(log(stage(1, 0, "breed"), stage(4, 0, "breed"),
                      stage(2, 0, "breed")));
  assert.equal(s.latestStageGen, 4);
});

test("latestStageGen stays null when no stage record arrives", () => {
  assert.equal(parse(log(runStart(), generation(1))).latestStageGen, null);
});

/* The page polls a file the writer is appending to, so a fetch can land
   mid-line. Dropping the partial line and keeping everything before it is what
   stops the display flickering to an error once per generation. */
test("ignores a partial trailing line", () => {
  const text = log(runStart(), generation(1)) + '{"type":"generat';
  const s = parse(text);
  assert.equal(s.gens.length, 1);
  assert.equal(s.error, null);
});

test("skips malformed and unrecognised lines without losing the rest", () => {
  const text = [
    JSON.stringify(runStart()),
    "not json at all",
    JSON.stringify(null),
    JSON.stringify([1, 2, 3]),
    JSON.stringify({ type: "something_new", gen: 1 }),
    JSON.stringify({ gen: 1 }),
    "",
    "   ",
    JSON.stringify(generation(1))
  ].join("\n");
  const s = parse(text);
  assert.ok(s.runStart);
  assert.equal(s.gens.length, 1);
});

test("drops generation and stage records that carry no gen", () => {
  const s = parse(log({ type: "generation", best_fitness: 1 },
                      { type: "stage", name: "breed", i: 0 }));
  assert.equal(s.gens.length, 0);
  assert.deepEqual(s.stagesByGen, {});
  assert.equal(s.latestStageGen, null);
});

test("empty input parses to a present-but-empty state", () => {
  const s = parse("");
  assert.equal(s.runStart, null);
  assert.deepEqual(s.gens, []);
  assert.equal(s.latestStageGen, null);
});

/* The fallback roster, for a log written before run_start declared one. */
test("collects stage names in order of first sight, without duplicates", () => {
  const s = parse(log(stage(1, 0, "breed"), stage(1, 1, "score"),
                      stage(2, 0, "breed"), stage(2, 1, "dedup"),
                      stage(2, 2, "score")));
  assert.deepEqual(s.stageRoster, ["breed", "score", "dedup"]);
});

test("parses a log captured from a real run", () => {
  const s = parse(realRun());
  assert.equal(s.runStart.input, "examples/fsm-combined/spec.json");
  assert.equal(s.runStart.stages.length, 12);
  assert.deepEqual(s.gens.map((g) => g.gen), [1, 2]);
  assert.equal(s.latestStageGen, 2);
  assert.ok(s.runEnd);
  for (const gen of s.gens) {
    assert.equal(typeof gen.elapsed_s, "number");
    assert.equal(typeof gen.best_fitness, "number");
    assert.equal(typeof gen.mean_fitness, "number");
    assert.equal(typeof gen.population, "number");
    assert.equal(typeof gen.objectives, "object");
  }
  for (const rec of s.stagesByGen[1]) {
    assert.equal(typeof rec.name, "string");
    assert.equal(typeof rec.n_in, "number");
    assert.equal(typeof rec.n_out, "number");
    assert.equal(typeof rec.elapsed_s, "number");
  }
});
