import test from "node:test";
import assert from "node:assert/strict";

import { loadDashboard, log, runStart, stage } from "./harness.mjs";

const { parse, stageRows } = loadDashboard();

/* The roster is what holds the chart at a fixed height while interval-gated
   filters come and go, so these cover which rows exist and in what order, not
   how they are drawn. */

function rowsFor(text, gen) {
  const s = parse(text);
  return stageRows(s, s.stagesByGen[gen]);
}

test("lays out every stage the run declared, in pipeline order", () => {
  const rows = rowsFor(log(runStart({ stages: ["breed", "dedup", "score", "select"] }),
                           stage(1, 0, "breed"), stage(1, 1, "score")), 1);
  assert.deepEqual(rows.map((r) => r.name),
                   ["breed", "dedup", "score", "select"]);
});

test("a declared stage that did not run keeps its row with no record", () => {
  const rows = rowsFor(log(runStart({ stages: ["breed", "dedup", "score"] }),
                           stage(1, 0, "breed"), stage(1, 1, "score")), 1);
  const absent = rows.filter((r) => r.rec === null).map((r) => r.name);
  assert.deepEqual(absent, ["dedup"]);
});

test("rows hold the stage's own record, not a neighbour's", () => {
  const rows = rowsFor(log(runStart({ stages: ["breed", "score"] }),
                           stage(1, 0, "breed", { n_out: 40 }),
                           stage(1, 1, "score", { n_out: 30 })), 1);
  assert.equal(rows[0].rec.n_out, 40);
  assert.equal(rows[1].rec.n_out, 30);
});

/* Filters run on intervals: the set of stages that ran differs generation to
   generation, but the rows must not move under the reader. */
test("the row list is identical across generations that ran different stages", () => {
  const text = log(runStart({ stages: ["breed", "weakening", "score"] }),
                   stage(1, 0, "breed"), stage(1, 1, "weakening"),
                   stage(1, 2, "score"),
                   stage(2, 0, "breed"), stage(2, 1, "score"));
  const first = rowsFor(text, 1);
  const second = rowsFor(text, 2);
  assert.deepEqual(first.map((r) => r.name), second.map((r) => r.name));
  assert.deepEqual(second.filter((r) => r.rec === null).map((r) => r.name),
                   ["weakening"]);
});

test("falls back to the names seen so far when no roster is declared", () => {
  const rows = rowsFor(log(stage(1, 0, "breed"), stage(1, 1, "score")), 1);
  assert.deepEqual(rows.map((r) => r.name), ["breed", "score"]);
  assert.ok(rows.every((r) => r.rec !== null));
});

test("falls back when the declared roster is empty", () => {
  const rows = rowsFor(log(runStart({ stages: [] }), stage(1, 0, "breed")), 1);
  assert.deepEqual(rows.map((r) => r.name), ["breed"]);
});

/* A stage the binary runs but did not declare would otherwise vanish from the
   chart, which is the one failure the roster must not introduce. */
test("a stage outside the roster is appended rather than dropped", () => {
  const rows = rowsFor(log(runStart({ stages: ["breed", "score"] }),
                           stage(1, 0, "breed"), stage(1, 1, "surprise"),
                           stage(1, 2, "score")), 1);
  assert.deepEqual(rows.map((r) => r.name), ["breed", "score", "surprise"]);
  assert.equal(rows[2].rec.name, "surprise");
});

test("real-run rows cover the declared roster with none absent", () => {
  const s = parse(
    log(runStart({ stages: ["order-parents", "breed", "score", "select"] }),
        stage(2, 0, "order-parents"), stage(2, 1, "breed"),
        stage(2, 2, "score"), stage(2, 3, "select")));
  const rows = stageRows(s, s.stagesByGen[s.latestStageGen]);
  assert.equal(rows.length, 4);
  assert.equal(rows.filter((r) => r.rec === null).length, 0);
});
