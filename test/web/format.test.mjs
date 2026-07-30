import test from "node:test";
import assert from "node:assert/strict";

import { loadDashboard } from "./harness.mjs";

const {
  pollMs, fmtPollRate, fmt2, fmt3, fmtInt, fmtDuration, fmtStageTime, signed,
  truncate, niceStep, niceTicks, barPath
} = loadDashboard();

const DASH = "—";
const MINUS = "−";

test("poll rate defaults to a second", () => {
  assert.equal(pollMs(""), 1000);
  assert.equal(pollMs("?other=3"), 1000);
  assert.equal(pollMs(undefined), 1000);
});

test("poll rate is read in seconds", () => {
  assert.equal(pollMs("?poll=5"), 5000);
  assert.equal(pollMs("?poll=2.5"), 2500);
  assert.equal(pollMs("?a=1&poll=3"), 3000);
  assert.equal(pollMs("?poll=3&a=1"), 3000);
});

test("poll=0 means load once", () => {
  assert.equal(pollMs("?poll=0"), 0);
});

/* Each poll refetches and reparses the whole log, which grows with the run, so
   the low end is clamped rather than honoured. */
test("poll rate is clamped to a quarter-second and five minutes", () => {
  assert.equal(pollMs("?poll=0.01"), 250);
  assert.equal(pollMs("?poll=100000"), 300000);
});

test("an unusable poll rate falls back to the default", () => {
  assert.equal(pollMs("?poll=soon"), 1000);
  assert.equal(pollMs("?poll="), 1000);
  assert.equal(pollMs("?poll=-4"), 1000);
});

test("poll rate survives URL encoding", () => {
  assert.equal(pollMs("?poll=%320"), 20000);
});

test("the poll rate is reported without trailing zeroes", () => {
  assert.equal(fmtPollRate(1000), "1s");
  assert.equal(fmtPollRate(2500), "2.5s");
  assert.equal(fmtPollRate(250), "0.25s");
  assert.equal(fmtPollRate(500), "0.5s");
});

test("numbers round to their stated precision", () => {
  assert.equal(fmt3(0.123456), "0.123");
  assert.equal(fmt2(0.126), "0.13");
  assert.equal(fmtInt(3.6), "4");
  assert.equal(fmtInt(0), "0");
});

/* A missing measurement and a measured zero must not look the same: the writer
   omits keys it never measured, and an em dash is how that reads. */
test("anything that is not a finite number renders as a dash", () => {
  for (const fmt of [fmt2, fmt3, fmtInt, fmtDuration, signed]) {
    assert.equal(fmt(undefined), DASH);
    assert.equal(fmt(null), DASH);
    assert.equal(fmt(NaN), DASH);
    assert.equal(fmt(Infinity), DASH);
    assert.equal(fmt("4"), DASH);
  }
});

test("durations pick a unit by magnitude", () => {
  assert.equal(fmtDuration(0.25), "0.3s");
  assert.equal(fmtDuration(9.94), "9.9s");
  assert.equal(fmtDuration(42), "42s");
  assert.equal(fmtDuration(90), "1m 30s");
  assert.equal(fmtDuration(3600), "1h 0m");
  assert.equal(fmtDuration(7500), "2h 5m");
});

test("a negative duration is not a duration", () => {
  assert.equal(fmtDuration(-1), DASH);
});

/* Stage times run from tens of microseconds to seconds within one chart, so
   the column would be all "0.0s" on the general duration formatter. */
test("sub-second stage times are reported in milliseconds", () => {
  assert.equal(fmtStageTime(0.016129113), "16ms");
  assert.equal(fmtStageTime(0.0042), "4.2ms");
  assert.equal(fmtStageTime(0.9994), "999ms");
  assert.equal(fmtStageTime(0.0001), "0.1ms");
  assert.equal(fmtStageTime(0), "0ms");
});

test("stage times of a second or more read as durations", () => {
  assert.equal(fmtStageTime(1), "1.0s");
  assert.equal(fmtStageTime(12.4), "12s");
  assert.equal(fmtStageTime(90), "1m 30s");
});

test("a stage time that was never recorded is a dash", () => {
  assert.equal(fmtStageTime(undefined), DASH);
  assert.equal(fmtStageTime(-1), DASH);
});

test("signed deltas carry a typographic minus, and zero carries neither sign", () => {
  assert.equal(signed(4), "+4");
  assert.equal(signed(-4), MINUS + "4");
  assert.equal(signed(0), "±0");
});

test("truncation replaces the tail with an ellipsis", () => {
  assert.equal(truncate("abcdefgh", 5), "abcd…");
  assert.equal(truncate("abcde", 5), "abcde");
  assert.equal(truncate(null, 5), "");
  assert.equal(truncate(12345678, 5), "1234…");
});

test("tick steps snap to 1, 2 or 5 times a power of ten", () => {
  assert.equal(niceStep(1, false), 1);
  assert.equal(niceStep(1.5, false), 2);
  assert.equal(niceStep(3, false), 5);
  assert.equal(niceStep(6, false), 10);
  assert.equal(niceStep(0.03, false), 0.05);
  assert.equal(niceStep(230, false), 500);
});

test("integral axes never step by a fraction", () => {
  assert.equal(niceStep(0.2, true), 1);
  assert.equal(niceStep(0.9, true), 1);
  assert.ok(niceTicks(4, 5, true).every(Number.isInteger));
});

test("ticks start at zero and cover the maximum", () => {
  const ticks = niceTicks(87, 5, false);
  assert.equal(ticks[0], 0);
  assert.ok(ticks[ticks.length - 1] >= 87);
  for (let i = 1; i < ticks.length; i++) {
    assert.ok(ticks[i] > ticks[i - 1]);
  }
});

/* Floating-point accumulation would otherwise print 0.30000000000000004. */
test("tick values are free of accumulation noise", () => {
  assert.deepEqual(niceTicks(1, 5, false), [0, 0.2, 0.4, 0.6, 0.8, 1]);
});

test("an empty or degenerate axis still yields a usable pair of ticks", () => {
  assert.deepEqual(niceTicks(0, 5, false), [0, 1]);
  assert.deepEqual(niceTicks(-3, 5, true), [0, 1]);
});

test("a bar rounds only its far end, and stays on the baseline", () => {
  const d = barPath(10, 5, 100, 20, 4);
  assert.ok(d.startsWith("M10"), d);
  assert.ok(/[Aa]/.test(d), "expected an arc on the far end: " + d);
  assert.ok(d.trim().endsWith("Z"), d);
});

/* A stage that emitted nothing gets no bar at all rather than a sliver; the
   caller draws the path only when there is one. */
test("a bar with no length yields no path", () => {
  assert.equal(barPath(0, 0, 0, 20, 4), null);
  assert.equal(barPath(0, 0, -5, 20, 4), null);
});

/* A radius wider than the bar would otherwise invert the arc. */
test("a bar too small to round falls back to square corners", () => {
  const d = barPath(0, 0, 0.5, 20, 4);
  assert.ok(!/[Aa]/.test(d), "expected no arc: " + d);
  assert.ok(d.trim().endsWith("Z"), d);
});

test("the corner radius is clamped by the bar's own size", () => {
  for (const [len, h] of [[1, 20], [100, 1], [2, 2], [8, 8]]) {
    const d = barPath(0, 0, len, h, 4);
    assert.equal(typeof d, "string", `len=${len} h=${h}`);
    assert.ok(!/NaN|Infinity|-\d*\.?\d+e/.test(d), `len=${len} h=${h}: ${d}`);
  }
});
