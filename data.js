window.BENCHMARK_DATA = {
  "lastUpdate": 1782481407179,
  "repoUrl": "https://github.com/benmandrew/counter",
  "entries": {
    "counter benchmarks": [
      {
        "commit": {
          "author": {
            "email": "benmandrew@gmail.com",
            "name": "benmandrew",
            "username": "benmandrew"
          },
          "committer": {
            "email": "benmandrew@gmail.com",
            "name": "benmandrew",
            "username": "benmandrew"
          },
          "distinct": true,
          "id": "323680e4600b389ab27b8cdc0ba118589064e84e",
          "message": "feat: add descriptive names to benchmarks via ->Name()\n\nReplaces raw function names with human-readable labels on the\ngithub-action-benchmark GitHub Pages charts.",
          "timestamp": "2026-06-26T14:34:26+01:00",
          "tree_id": "f2bdba9700852bcf5ff64463782dd3f3cfe5a26f",
          "url": "https://github.com/benmandrew/counter/commit/323680e4600b389ab27b8cdc0ba118589064e84e"
        },
        "date": 1782481406688,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 656.0685095423507,
            "unit": "ns/iter",
            "extra": "iterations: 1086418\ncpu: 655.9967857675408 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2428.0910232418983,
            "unit": "ns/iter",
            "extra": "iterations: 285905\ncpu: 2427.8745527360497 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 909.3566041392346,
            "unit": "ns/iter",
            "extra": "iterations: 763718\ncpu: 909.2056858683441 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 209.20706841668914,
            "unit": "ns/iter",
            "extra": "iterations: 3437743\ncpu: 209.13034278594992 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 230.07660938691873,
            "unit": "ns/iter",
            "extra": "iterations: 3043426\ncpu: 230.0220057921566 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 256.4907849008149,
            "unit": "ns/iter",
            "extra": "iterations: 2726612\ncpu: 256.4306751382302 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 310.0372107726432,
            "unit": "ns/iter",
            "extra": "iterations: 2258647\ncpu: 309.9792397838177 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3909.0007865188286,
            "unit": "ns/iter",
            "extra": "iterations: 179271\ncpu: 3908.265592315545 ns\nthreads: 1"
          }
        ]
      }
    ]
  }
}