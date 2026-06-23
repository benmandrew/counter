window.BENCHMARK_DATA = {
  "lastUpdate": 1782222721844,
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
          "id": "f33fcc916392347e03fc4e3f6f17034e22d8a14f",
          "message": "compare: sort by fitness descending, show fitness, fix LTL timeout\n\nThree related changes:\n\n1. compare now loads repairs as ScoredSpecification so it can read the\n   stored fitness.total; sorts repairs by fitness descending before\n   printing; appends [fitness: X.XXXX] to each output line.\n   load_scored_specification added to serialisation.hpp for this.\n\n2. requirement_to_ltl: when condition is \"true\" and condition_type is\n   Trigger, the G-clause G((!(true) & X(true)) -> X(body)) is\n   vacuously true (!(true) is always false). Emitting it caused black's\n   BMC to time out on the deeply-nested X bodies produced by\n   WithinTicks/ForTicks, making spec_implies return nullopt and compare\n   report everything as \"incomparable\". The clause is now omitted for\n   this case, emitting only the initial obligation (true) -> body.",
          "timestamp": "2026-06-23T13:58:04+01:00",
          "tree_id": "48aac8f1a081ea08b3a47c44561dd037d6f32743",
          "url": "https://github.com/benmandrew/counter/commit/f33fcc916392347e03fc4e3f6f17034e22d8a14f"
        },
        "date": 1782221642973,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 708.7921632125745,
            "unit": "ns/iter",
            "extra": "iterations: 993698\ncpu: 708.6703495428188 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2594.892382524769,
            "unit": "ns/iter",
            "extra": "iterations: 270142\ncpu: 2594.6016132256373 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 755.4961293524084,
            "unit": "ns/iter",
            "extra": "iterations: 914834\ncpu: 755.4035300393297 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 207.08440672276967,
            "unit": "ns/iter",
            "extra": "iterations: 3378783\ncpu: 207.02782125990333 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 234.14850611995303,
            "unit": "ns/iter",
            "extra": "iterations: 2986786\ncpu: 234.0919731108959 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 261.215140472842,
            "unit": "ns/iter",
            "extra": "iterations: 2682796\ncpu: 261.16674655844133 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 314.23938528183515,
            "unit": "ns/iter",
            "extra": "iterations: 2227167\ncpu: 314.20546775342825 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 2705.6932078943446,
            "unit": "ns/iter",
            "extra": "iterations: 261907\ncpu: 2705.340498726649 ns\nthreads: 1"
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
          "id": "bf300a7b2a61e4379d4460f35453104ec3890730",
          "message": "test: add regression for propositionally-equivalent response scoring\n\nAdds test_semantic_similarity_propequiv_responses_score_equal: two\nWithinTicks-5 requirements whose responses are propositionally identical\n(!tr & lo, written two syntactically different ways) must score the same\nagainst the same WithinTicks-7 original. Guards against formula strings that\nproduce different HOA automata before SPOT normalises them.\n\nAlso squashes three changes left uncommitted from the previous session:\n- compare: set Config::black_timeout = 20s so implication checks don't time\n  out on deeply-nested formulae (the LTL fix addressed the root cause but\n  left compare without its own override)\n- config: reduced generations/population for faster dev iteration; raised\n  p_trigger/p_response/p_timing and black_timeout to match current tuning\n- gitignore: broaden results/ pattern to results*/ to cover results-foo dirs",
          "timestamp": "2026-06-23T14:46:06+01:00",
          "tree_id": "f17a8f44285abf08984eb9fa33c1ad5ae068fd99",
          "url": "https://github.com/benmandrew/counter/commit/bf300a7b2a61e4379d4460f35453104ec3890730"
        },
        "date": 1782222720993,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 514.5633808163244,
            "unit": "ns/iter",
            "extra": "iterations: 1369894\ncpu: 514.4459126034569 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 1904.4558991512238,
            "unit": "ns/iter",
            "extra": "iterations: 365934\ncpu: 1904.0196893428872 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 586.2805499273181,
            "unit": "ns/iter",
            "extra": "iterations: 1170191\ncpu: 586.188923004877 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 160.40896672380683,
            "unit": "ns/iter",
            "extra": "iterations: 4360054\ncpu: 160.38171132742852 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 181.59176987076037,
            "unit": "ns/iter",
            "extra": "iterations: 3824703\ncpu: 181.52684326077082 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 204.71377292100132,
            "unit": "ns/iter",
            "extra": "iterations: 3457255\ncpu: 204.705159440076 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 243.59939901362148,
            "unit": "ns/iter",
            "extra": "iterations: 2873942\ncpu: 243.54695641039368 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3043.2036957642244,
            "unit": "ns/iter",
            "extra": "iterations: 230913\ncpu: 3042.936118797989 ns\nthreads: 1"
          }
        ]
      }
    ]
  }
}