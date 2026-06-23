window.BENCHMARK_DATA = {
  "lastUpdate": 1782216992940,
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
          "id": "f31b92b084174b1a84907a2856a222c10de111aa",
          "message": "fix: move benchmark index.html to gh-pages root",
          "timestamp": "2026-06-22T21:16:40+01:00",
          "tree_id": "1dd5262373f2c1eb366329c233278c3b6990bbb8",
          "url": "https://github.com/benmandrew/counter/commit/f31b92b084174b1a84907a2856a222c10de111aa"
        },
        "date": 1782159675949,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 678.3572238813823,
            "unit": "ns/iter",
            "extra": "iterations: 1032319\ncpu: 678.2988688573978 ns\nthreads: 1"
          }
        ]
      },
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
          "id": "95701386de7a60c47e064e0a810d14535b31e46d",
          "message": "fix: make counter_fitness depend on spot_project so ltl2tgba is built before any target that links it",
          "timestamp": "2026-06-23T13:01:34+01:00",
          "tree_id": "f72ec42ba6fd6bd559969c799c580feeac19b3f3",
          "url": "https://github.com/benmandrew/counter/commit/95701386de7a60c47e064e0a810d14535b31e46d"
        },
        "date": 1782216992085,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 634.2307366109437,
            "unit": "ns/iter",
            "extra": "iterations: 1072968\ncpu: 634.1261892246554 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2405.8215330543494,
            "unit": "ns/iter",
            "extra": "iterations: 288496\ncpu: 2405.3731039598474 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 780.1609981669224,
            "unit": "ns/iter",
            "extra": "iterations: 892476\ncpu: 780.0739291588792 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 203.82914487824175,
            "unit": "ns/iter",
            "extra": "iterations: 3433207\ncpu: 203.8251416241433 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 230.53195808414537,
            "unit": "ns/iter",
            "extra": "iterations: 3028639\ncpu: 230.5221612744206 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 258.01674336232327,
            "unit": "ns/iter",
            "extra": "iterations: 2719406\ncpu: 258.00601344558333 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 310.45903859930485,
            "unit": "ns/iter",
            "extra": "iterations: 2242062\ncpu: 310.4533090521137 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 2770.3142134922327,
            "unit": "ns/iter",
            "extra": "iterations: 251606\ncpu: 2770.232355349236 ns\nthreads: 1"
          }
        ]
      }
    ]
  }
}