window.BENCHMARK_DATA = {
  "lastUpdate": 1782397536576,
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
          "id": "087d119e12521c1072a3700a6a845f95afd5953d",
          "message": "fix: sort config.hpp include in compare.cpp to satisfy clang-format",
          "timestamp": "2026-06-23T15:03:01+01:00",
          "tree_id": "3a1e39125a3c8487ce8bb6489e7eca2e6b2ec7e0",
          "url": "https://github.com/benmandrew/counter/commit/087d119e12521c1072a3700a6a845f95afd5953d"
        },
        "date": 1782223709482,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 707.2011960234668,
            "unit": "ns/iter",
            "extra": "iterations: 990616\ncpu: 707.1013985237469 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2616.486982234861,
            "unit": "ns/iter",
            "extra": "iterations: 259722\ncpu: 2616.27099360085 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 763.7621531836354,
            "unit": "ns/iter",
            "extra": "iterations: 936771\ncpu: 763.7065408728495 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 211.26998507014187,
            "unit": "ns/iter",
            "extra": "iterations: 3320191\ncpu: 211.23441452615222 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 238.3562342641,
            "unit": "ns/iter",
            "extra": "iterations: 2933578\ncpu: 238.29514504131149 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 266.1014474107714,
            "unit": "ns/iter",
            "extra": "iterations: 2619367\ncpu: 266.08472963124296 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 321.7045717948754,
            "unit": "ns/iter",
            "extra": "iterations: 2185553\ncpu: 321.66959986785935 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3918.7757494566654,
            "unit": "ns/iter",
            "extra": "iterations: 179897\ncpu: 3918.6963317898594 ns\nthreads: 1"
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
          "id": "27ce99c6870a2c145561ba788cb767c8e67465f6",
          "message": "fix: extract optional fitness total before stream chain to satisfy clang-tidy\n\nbugprone-unchecked-optional-access in clang-tidy-18 does not track an\noptional liveness check (if (repair_scored.fitness)) through chained <<\noperator calls, so it flags the fitness->total dereference on the third\nlink of the chain as unchecked. Extracting the value into a local variable\nbefore the stream expression makes the guarded access unambiguous.",
          "timestamp": "2026-06-23T15:46:50+01:00",
          "tree_id": "fd949f7293d945973d0d2d3bab94e58582d48794",
          "url": "https://github.com/benmandrew/counter/commit/27ce99c6870a2c145561ba788cb767c8e67465f6"
        },
        "date": 1782226326533,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 682.3906553560178,
            "unit": "ns/iter",
            "extra": "iterations: 995993\ncpu: 682.1723054278496 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2446.1334287709324,
            "unit": "ns/iter",
            "extra": "iterations: 286400\ncpu: 2445.7001710893865 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 743.7455806327994,
            "unit": "ns/iter",
            "extra": "iterations: 947251\ncpu: 743.6772903908255 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 206.44233403291295,
            "unit": "ns/iter",
            "extra": "iterations: 3385385\ncpu: 206.43071142573152 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 233.8878787868668,
            "unit": "ns/iter",
            "extra": "iterations: 2993537\ncpu: 233.86723698420965 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 262.96430091081373,
            "unit": "ns/iter",
            "extra": "iterations: 2688584\ncpu: 262.9443298033463 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 313.5961126159806,
            "unit": "ns/iter",
            "extra": "iterations: 2230909\ncpu: 313.5729126557828 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3915.6859361807647,
            "unit": "ns/iter",
            "extra": "iterations: 178849\ncpu: 3915.2928336194186 ns\nthreads: 1"
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
          "id": "15b1f69f0aee53d2f7ddb3fa4fd8e9dddff36ee7",
          "message": "fix: work around clang-tidy-18 false positive on optional access via struct member\n\nTwo changes bundled because one addresses the symptom and the other the root cause:\n\nci.yml: add run-clang-tidy-21 symlink alongside clang-tidy-21\n\n  CI was installing LLVM 21 and symlinking clang-tidy → clang-tidy-21, but\n  not symlinking run-clang-tidy. find_program then found the system default\n  run-clang-tidy (from Ubuntu 24.04's LLVM 18 package), which has clang-tidy-18\n  baked in as its default binary. The -clang-tidy-binary override in\n  run_clang_tidy.cmake was silently ignored because LLVM 18's run-clang-tidy.py\n  expects --clang-tidy-binary (two dashes) while the cmake passes one dash.\n  Adding the run-clang-tidy-21 symlink ensures the version-21 script is used,\n  which agrees with the clang-tidy binary already pinned.\n\ncompare.cpp: extract optional member to local ref before the guard\n\n  clang-tidy-18's bugprone-unchecked-optional-access does not propagate\n  optional state through member access on a structured-binding variable, so\n  if (repair_scored.fitness) did not suppress the warning on\n  repair_scored.fitness->total in the same block. Binding the member to a\n  local const ref first gives the checker a plain optional variable to track,\n  satisfying both clang-tidy-18 and clang-tidy-21.",
          "timestamp": "2026-06-23T17:34:33+01:00",
          "tree_id": "504d7e9a742efaa67dc5bd9805ffa684c5f8f592",
          "url": "https://github.com/benmandrew/counter/commit/15b1f69f0aee53d2f7ddb3fa4fd8e9dddff36ee7"
        },
        "date": 1782232768312,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 645.9108445160294,
            "unit": "ns/iter",
            "extra": "iterations: 1062952\ncpu: 645.7531976984851 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2437.216814840547,
            "unit": "ns/iter",
            "extra": "iterations: 279289\ncpu: 2437.0812455914843 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 849.6119132358837,
            "unit": "ns/iter",
            "extra": "iterations: 876353\ncpu: 849.5326928760444 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 203.96213286857443,
            "unit": "ns/iter",
            "extra": "iterations: 3438074\ncpu: 203.95197805515522 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 231.87441787524776,
            "unit": "ns/iter",
            "extra": "iterations: 3041659\ncpu: 231.83765274148095 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 257.012394511184,
            "unit": "ns/iter",
            "extra": "iterations: 2723867\ncpu: 256.9977146461265 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 310.2235865877932,
            "unit": "ns/iter",
            "extra": "iterations: 2260717\ncpu: 310.1963832713252 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 4023.0142609675986,
            "unit": "ns/iter",
            "extra": "iterations: 173761\ncpu: 4022.420589200108 ns\nthreads: 1"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "benmandrew@gmail.com",
            "name": "Ben Andrew",
            "username": "benmandrew"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "daa4cab66ff049611530ee0fa3d6581cd5bdad79",
          "message": "Merge pull request #3 from benmandrew/feature/ltl-normalisation\n\nfeat: normalise LTL formulae via ltlfilt to improve cache hit rates",
          "timestamp": "2026-06-24T13:05:19+01:00",
          "tree_id": "0aed1d3c0da76a61d4bd6f5b6a8099fa098ddbd7",
          "url": "https://github.com/benmandrew/counter/commit/daa4cab66ff049611530ee0fa3d6581cd5bdad79"
        },
        "date": 1782302999369,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 646.1236353411457,
            "unit": "ns/iter",
            "extra": "iterations: 1081131\ncpu: 646.0624901145189 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2449.434389074908,
            "unit": "ns/iter",
            "extra": "iterations: 277309\ncpu: 2449.2148253392425 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 889.5611800338638,
            "unit": "ns/iter",
            "extra": "iterations: 780181\ncpu: 889.4613762703781 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 212.54400459459586,
            "unit": "ns/iter",
            "extra": "iterations: 3433596\ncpu: 212.5265022442944 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 229.32517131840757,
            "unit": "ns/iter",
            "extra": "iterations: 3051044\ncpu: 229.3075707856065 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 256.02639768250435,
            "unit": "ns/iter",
            "extra": "iterations: 2733990\ncpu: 256.01600261888314 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 309.37755454312776,
            "unit": "ns/iter",
            "extra": "iterations: 2265180\ncpu: 309.3340666083934 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 4088.0334235365426,
            "unit": "ns/iter",
            "extra": "iterations: 172573\ncpu: 4087.7621586227283 ns\nthreads: 1"
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
          "id": "db74e01cbef90495c44f075b4483353b982a05a3",
          "message": "feat: show realizable candidate count in generation status line\n\nAdds a \"real\" column updated after each generation, counting candidates\nin the population that pass the realizability and false-condition checks.\nResults are cache hits (scoring already called both checkers during evolution).\n\nAlso removes the per-filter drop columns from the status line.",
          "timestamp": "2026-06-24T13:08:46+01:00",
          "tree_id": "f607521367a1ccbbac037c51d3f898db65f2de8f",
          "url": "https://github.com/benmandrew/counter/commit/db74e01cbef90495c44f075b4483353b982a05a3"
        },
        "date": 1782303242596,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 550.9257443169672,
            "unit": "ns/iter",
            "extra": "iterations: 1283996\ncpu: 550.8614014373876 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2014.3897847840847,
            "unit": "ns/iter",
            "extra": "iterations: 348069\ncpu: 2014.1016436396235 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 688.882850281401,
            "unit": "ns/iter",
            "extra": "iterations: 1030186\ncpu: 688.7903349492226 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 164.06170011939537,
            "unit": "ns/iter",
            "extra": "iterations: 4374481\ncpu: 164.04936402741265 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 180.86514583588567,
            "unit": "ns/iter",
            "extra": "iterations: 3875075\ncpu: 180.81228182680348 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 201.6657969409518,
            "unit": "ns/iter",
            "extra": "iterations: 3476575\ncpu: 201.6141161919419 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 245.26838983289034,
            "unit": "ns/iter",
            "extra": "iterations: 2885657\ncpu: 245.21789942463684 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3080.141827581695,
            "unit": "ns/iter",
            "extra": "iterations: 226310\ncpu: 3079.5150236401414 ns\nthreads: 1"
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
          "id": "525e27daed9903559471affa0fb3ea8dbf56e502",
          "message": "feat: show ltlfilt cache stats in timing report\n\nAdds ltlfilt to the tool timing report alongside ltl2tgba, ltlsynt,\nblack, and ganak, so normalisation cache hit rates are visible at\nthe end of each run.",
          "timestamp": "2026-06-24T13:15:45+01:00",
          "tree_id": "61f3327884b35f9aad7bc6c5b60f13baffe15788",
          "url": "https://github.com/benmandrew/counter/commit/525e27daed9903559471affa0fb3ea8dbf56e502"
        },
        "date": 1782304210440,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 720.3233042567566,
            "unit": "ns/iter",
            "extra": "iterations: 987163\ncpu: 720.3053690221371 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2599.682265377643,
            "unit": "ns/iter",
            "extra": "iterations: 268847\ncpu: 2599.1195661472884 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 898.2810245594338,
            "unit": "ns/iter",
            "extra": "iterations: 787070\ncpu: 898.1782052422274 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 206.436092383235,
            "unit": "ns/iter",
            "extra": "iterations: 3380137\ncpu: 206.42466059807634 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 233.31474891921377,
            "unit": "ns/iter",
            "extra": "iterations: 2994355\ncpu: 233.29702790751267 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 260.1123180208702,
            "unit": "ns/iter",
            "extra": "iterations: 2687396\ncpu: 260.0936739505457 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 313.24492204450996,
            "unit": "ns/iter",
            "extra": "iterations: 2235250\ncpu: 313.2125578794321 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3879.069239714254,
            "unit": "ns/iter",
            "extra": "iterations: 180590\ncpu: 3878.930378204772 ns\nthreads: 1"
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
          "id": "db2901e532ac99cd1644f2ad347b9f63eb16816f",
          "message": "ci: bump sccache-action to v0.0.10",
          "timestamp": "2026-06-24T13:28:23+01:00",
          "tree_id": "1da46ef96659e6fb71cd17e2c3002bf544f91391",
          "url": "https://github.com/benmandrew/counter/commit/db2901e532ac99cd1644f2ad347b9f63eb16816f"
        },
        "date": 1782304379250,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 676.6009487850833,
            "unit": "ns/iter",
            "extra": "iterations: 975985\ncpu: 676.4578328560377 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2471.314277332858,
            "unit": "ns/iter",
            "extra": "iterations: 282938\ncpu: 2471.1319476351714 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 907.1529124695841,
            "unit": "ns/iter",
            "extra": "iterations: 767596\ncpu: 907.0937863146762 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 226.13581890855173,
            "unit": "ns/iter",
            "extra": "iterations: 3092537\ncpu: 226.1045733648457 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 252.89359859996898,
            "unit": "ns/iter",
            "extra": "iterations: 2766223\ncpu: 252.87573163841094 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 283.40260159521904,
            "unit": "ns/iter",
            "extra": "iterations: 2502849\ncpu: 283.36610398789526 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 334.20467622166376,
            "unit": "ns/iter",
            "extra": "iterations: 2100542\ncpu: 334.19716530305044 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 4103.5142082417115,
            "unit": "ns/iter",
            "extra": "iterations: 173702\ncpu: 4102.833283439457 ns\nthreads: 1"
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
          "id": "33d4d32095e7cf6ded4b5fd37d74ea3ee7c4879d",
          "message": "feat: report timeout instead of incomparable in compare when black times out\n\nWhen spec_implies returns nullopt (black timeout) and neither direction\nis definitively true, classify the result as timeout rather than\nincomparable. Adds timeout count to the summary line.",
          "timestamp": "2026-06-24T13:20:00+01:00",
          "tree_id": "8b68ebf69b8c55a6fafe3641714e190ad4b76e14",
          "url": "https://github.com/benmandrew/counter/commit/33d4d32095e7cf6ded4b5fd37d74ea3ee7c4879d"
        },
        "date": 1782304448605,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 683.2748525437589,
            "unit": "ns/iter",
            "extra": "iterations: 990633\ncpu: 683.0776049253357 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2560.3319545093755,
            "unit": "ns/iter",
            "extra": "iterations: 284542\ncpu: 2559.9049419769317 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 941.2180467864887,
            "unit": "ns/iter",
            "extra": "iterations: 804121\ncpu: 941.0510855953272 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 205.95019524894045,
            "unit": "ns/iter",
            "extra": "iterations: 2535481\ncpu: 205.94007133163288 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 235.32424541885442,
            "unit": "ns/iter",
            "extra": "iterations: 2989208\ncpu: 235.28783610909633 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 261.70899830775426,
            "unit": "ns/iter",
            "extra": "iterations: 2693462\ncpu: 261.70348124458377 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 312.9046859994658,
            "unit": "ns/iter",
            "extra": "iterations: 2234614\ncpu: 312.85441825747097 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3830.207795898725,
            "unit": "ns/iter",
            "extra": "iterations: 182968\ncpu: 3829.8347579904703 ns\nthreads: 1"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "benmandrew@gmail.com",
            "name": "Ben Andrew",
            "username": "benmandrew"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "96c5dd969dd021949e7f3b62ce22993dd0bb6c90",
          "message": "Merge pull request #4 from benmandrew/feat/toml-config\n\nfeat: TOML configuration file support (--config flag)",
          "timestamp": "2026-06-24T16:20:09+01:00",
          "tree_id": "38e81a9de0b619d79864ea270fdb1597d514faed",
          "url": "https://github.com/benmandrew/counter/commit/96c5dd969dd021949e7f3b62ce22993dd0bb6c90"
        },
        "date": 1782314682247,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 541.6393633174567,
            "unit": "ns/iter",
            "extra": "iterations: 1294774\ncpu: 541.4825205016474 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 1982.6293500821398,
            "unit": "ns/iter",
            "extra": "iterations: 351229\ncpu: 1982.4773011340183 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 656.0269297494639,
            "unit": "ns/iter",
            "extra": "iterations: 1065216\ncpu: 655.918638097813 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 160.16023341295494,
            "unit": "ns/iter",
            "extra": "iterations: 4357256\ncpu: 160.14194093714025 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 180.54650794948225,
            "unit": "ns/iter",
            "extra": "iterations: 3878681\ncpu: 180.52800526777003 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 201.55954341454535,
            "unit": "ns/iter",
            "extra": "iterations: 3465463\ncpu: 201.5303981026488 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 243.09368162708256,
            "unit": "ns/iter",
            "extra": "iterations: 2883543\ncpu: 243.05924517165153 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3066.2180672434197,
            "unit": "ns/iter",
            "extra": "iterations: 228989\ncpu: 3065.8800990440604 ns\nthreads: 1"
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
          "id": "055eb7421f128b09ebd45824920954fb81763e54",
          "message": "refactor: rename example config file",
          "timestamp": "2026-06-24T16:52:09+01:00",
          "tree_id": "996ac30833bfe90422c0a07a18d84cdbbaca12d0",
          "url": "https://github.com/benmandrew/counter/commit/055eb7421f128b09ebd45824920954fb81763e54"
        },
        "date": 1782316620675,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 1048.8263811936656,
            "unit": "ns/iter",
            "extra": "iterations: 715631\ncpu: 1048.7000912481435 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2747.1428528104666,
            "unit": "ns/iter",
            "extra": "iterations: 263796\ncpu: 2746.915461189707 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 904.9124724158808,
            "unit": "ns/iter",
            "extra": "iterations: 764479\ncpu: 904.8766388612378 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 271.1922099293913,
            "unit": "ns/iter",
            "extra": "iterations: 2575304\ncpu: 271.18443337174944 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 238.99334356021367,
            "unit": "ns/iter",
            "extra": "iterations: 3009717\ncpu: 238.9869236875095 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 301.3653745320348,
            "unit": "ns/iter",
            "extra": "iterations: 2424025\ncpu: 301.34301832695627 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 385.6360283257047,
            "unit": "ns/iter",
            "extra": "iterations: 1790741\ncpu: 385.62271986847884 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 4004.4120880690116,
            "unit": "ns/iter",
            "extra": "iterations: 175181\ncpu: 4003.6046146557005 ns\nthreads: 1"
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
          "id": "812e1c266bff06302292d86be15c0b70fb49d89e",
          "message": "fix: fall back to deb download when PATH black fails to run\n\nIf a black binary exists on PATH but has an unsatisfied shared-library\ndependency (e.g. libfmt.so.9 missing after apt install libfmt-dev gives\nlibfmt.so.8), find_program would accept it and return early, leaving the\nproject with a non-functional black executable.\n\nNow the candidate is tested with --version before being accepted. A\nnon-zero exit falls through to the deb download path, whose wrapper\nscript sets LD_LIBRARY_PATH to the bundled libraries inside the\nextracted package, avoiding any system libfmt dependency.",
          "timestamp": "2026-06-24T17:59:48+01:00",
          "tree_id": "bc7f2dd12c77ca98965d50f1c07df0d6dc6462c0",
          "url": "https://github.com/benmandrew/counter/commit/812e1c266bff06302292d86be15c0b70fb49d89e"
        },
        "date": 1782320685610,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 656.6056236320501,
            "unit": "ns/iter",
            "extra": "iterations: 1073221\ncpu: 656.5499212184629 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2454.2441176884636,
            "unit": "ns/iter",
            "extra": "iterations: 284072\ncpu: 2454.011289391422 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 911.6744102214268,
            "unit": "ns/iter",
            "extra": "iterations: 768424\ncpu: 911.5667509083528 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 203.71878762730063,
            "unit": "ns/iter",
            "extra": "iterations: 3431684\ncpu: 203.71246390984706 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 230.880719103405,
            "unit": "ns/iter",
            "extra": "iterations: 3034501\ncpu: 230.86700943581803 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 257.0545451673117,
            "unit": "ns/iter",
            "extra": "iterations: 2721946\ncpu: 257.037541523601 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 310.0780391392206,
            "unit": "ns/iter",
            "extra": "iterations: 2256560\ncpu: 310.0469980855817 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 4011.879581421941,
            "unit": "ns/iter",
            "extra": "iterations: 174400\ncpu: 4011.5036295871555 ns\nthreads: 1"
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
          "id": "e0bf6152d306fbdb9135e77a9730ed6463e5bfbc",
          "message": "fix: build black from source on non-noble Linux\n\nThe prebuilt ubuntu24.04 deb requires Ubuntu 24.04 (noble) system\nlibraries. On other releases (e.g. 22.04 jammy) the binary fails at\nruntime due to missing shared libraries such as libfmt.so.9.\n\nDetect the Ubuntu codename via lsb_release at configure time. On noble\nuse the existing deb path; on all other releases build black from source\nusing the same ExternalProject approach as macOS, requiring either\nlibz3-dev or libcryptominisat5-dev as the SAT solver backend.",
          "timestamp": "2026-06-24T18:49:38+01:00",
          "tree_id": "75b5f5cbed1c98dcdff75164d9f21726314616e7",
          "url": "https://github.com/benmandrew/counter/commit/e0bf6152d306fbdb9135e77a9730ed6463e5bfbc"
        },
        "date": 1782323691806,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 660.1080622746111,
            "unit": "ns/iter",
            "extra": "iterations: 1058473\ncpu: 660.0578928324105 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2467.965369780051,
            "unit": "ns/iter",
            "extra": "iterations: 283885\ncpu: 2467.637423604629 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 884.0656844506632,
            "unit": "ns/iter",
            "extra": "iterations: 803082\ncpu: 883.9563307358403 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 203.85652002548815,
            "unit": "ns/iter",
            "extra": "iterations: 3374450\ncpu: 203.82663871149376 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 230.5274037830084,
            "unit": "ns/iter",
            "extra": "iterations: 3037847\ncpu: 230.5238673968768 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 256.98247236471894,
            "unit": "ns/iter",
            "extra": "iterations: 2721702\ncpu: 256.9704997093729 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 309.9986514814435,
            "unit": "ns/iter",
            "extra": "iterations: 2258775\ncpu: 309.9607145466019 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3978.64527489508,
            "unit": "ns/iter",
            "extra": "iterations: 176049\ncpu: 3978.2011485438693 ns\nthreads: 1"
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
          "id": "88b2ef2898897962a85e6769874bcb3da4ca3f03",
          "message": "feat: add parameter sweep experiment scripts\n\nAdds scripts to run a three-sweep sensitivity analysis (generations,\npopulation size, fitness weights) and collect results for statistical\ntesting in a Jupyter notebook.\n\nFresh-machine workflow:\n  pip install -r scripts/requirements.txt\n  python scripts/gen_configs.py\n  python scripts/run_experiments.py\n  jupyter notebook scripts/analyse.ipynb\n\nAlso gitignores experiments/ (generated results) and PLAN.md.",
          "timestamp": "2026-06-24T18:52:04+01:00",
          "tree_id": "cbbefb853a69b508eeb07740a49b52aea784bafd",
          "url": "https://github.com/benmandrew/counter/commit/88b2ef2898897962a85e6769874bcb3da4ca3f03"
        },
        "date": 1782323796893,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 641.9501702837144,
            "unit": "ns/iter",
            "extra": "iterations: 1092882\ncpu: 641.9136960806383 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2416.97410460866,
            "unit": "ns/iter",
            "extra": "iterations: 288198\ncpu: 2416.6380301043037 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 907.051349532831,
            "unit": "ns/iter",
            "extra": "iterations: 771341\ncpu: 907.0333263757535 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 203.21656929619584,
            "unit": "ns/iter",
            "extra": "iterations: 3460666\ncpu: 203.20128986732615 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 229.84469444201258,
            "unit": "ns/iter",
            "extra": "iterations: 3031585\ncpu: 229.83508989521994 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 257.51699646773636,
            "unit": "ns/iter",
            "extra": "iterations: 2717800\ncpu: 257.50261645448523 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 310.0698401573944,
            "unit": "ns/iter",
            "extra": "iterations: 2257283\ncpu: 310.0511903026782 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3993.292227762505,
            "unit": "ns/iter",
            "extra": "iterations: 176886\ncpu: 3993.1302194633813 ns\nthreads: 1"
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
          "id": "262cf91c0cb253309b5b048ff7f74c3ba729932f",
          "message": "fix: drop gen100, replace gen50 with gen40 in sweep A",
          "timestamp": "2026-06-25T12:56:53+01:00",
          "tree_id": "ceeca3ff74dfd3ebf4aa4a0c05e77e353070a29d",
          "url": "https://github.com/benmandrew/counter/commit/262cf91c0cb253309b5b048ff7f74c3ba729932f"
        },
        "date": 1782388920304,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 608.4141824564251,
            "unit": "ns/iter",
            "extra": "iterations: 1157712\ncpu: 608.3698795555372 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2332.6984174619065,
            "unit": "ns/iter",
            "extra": "iterations: 299898\ncpu: 2332.6006808981724 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 798.5231708438839,
            "unit": "ns/iter",
            "extra": "iterations: 869800\ncpu: 798.4708956081857 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 198.33968848573892,
            "unit": "ns/iter",
            "extra": "iterations: 3539228\ncpu: 198.3251768464761 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 224.7229564031807,
            "unit": "ns/iter",
            "extra": "iterations: 3118668\ncpu: 224.69851103099143 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 248.91272840323734,
            "unit": "ns/iter",
            "extra": "iterations: 2804853\ncpu: 248.89383899976207 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 303.65145804692025,
            "unit": "ns/iter",
            "extra": "iterations: 2340734\ncpu: 303.6111053199551 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3689.9403135291473,
            "unit": "ns/iter",
            "extra": "iterations: 189775\ncpu: 3689.548981688841 ns\nthreads: 1"
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
          "id": "cd0234a41e64080c074c29047341891b7acbb8e4",
          "message": "docs: add scripts/README.md for experiment setup and multi-machine splitting",
          "timestamp": "2026-06-25T13:04:00+01:00",
          "tree_id": "df832ea5897f9a6e28aaaaab2df3ae8034859616",
          "url": "https://github.com/benmandrew/counter/commit/cd0234a41e64080c074c29047341891b7acbb8e4"
        },
        "date": 1782389329875,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 709.1856314414122,
            "unit": "ns/iter",
            "extra": "iterations: 996189\ncpu: 709.1279144820913 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2580.9271928434823,
            "unit": "ns/iter",
            "extra": "iterations: 271759\ncpu: 2579.8795918442443 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 849.5779725307224,
            "unit": "ns/iter",
            "extra": "iterations: 806720\ncpu: 849.5458585382785 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 205.81434100267134,
            "unit": "ns/iter",
            "extra": "iterations: 3265476\ncpu: 205.7667531471676 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 233.6358734981155,
            "unit": "ns/iter",
            "extra": "iterations: 2997299\ncpu: 233.58009127551176 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 260.849065776755,
            "unit": "ns/iter",
            "extra": "iterations: 2684262\ncpu: 260.7602558915635 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 315.0480210455301,
            "unit": "ns/iter",
            "extra": "iterations: 2224129\ncpu: 314.9850291956986 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3846.967961454337,
            "unit": "ns/iter",
            "extra": "iterations: 181188\ncpu: 3846.4141830584795 ns\nthreads: 1"
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
          "id": "f2ff3684bbdaf9577628af601ed15ecab494382c",
          "message": "docs: document CMake ≥ 3.25, C++17, and libfmt transitive dep in README",
          "timestamp": "2026-06-25T15:07:43+01:00",
          "tree_id": "d3ff468cfbe3230db688a2800703cf05e114ea24",
          "url": "https://github.com/benmandrew/counter/commit/f2ff3684bbdaf9577628af601ed15ecab494382c"
        },
        "date": 1782396771006,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 646.9043080266526,
            "unit": "ns/iter",
            "extra": "iterations: 1107073\ncpu: 646.8658877960171 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2427.5416087209924,
            "unit": "ns/iter",
            "extra": "iterations: 289783\ncpu: 2427.364555546737 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 880.1994255606704,
            "unit": "ns/iter",
            "extra": "iterations: 799040\ncpu: 880.1821836203444 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 201.4218529435379,
            "unit": "ns/iter",
            "extra": "iterations: 3458454\ncpu: 201.40535684441662 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 229.0757870550881,
            "unit": "ns/iter",
            "extra": "iterations: 3062238\ncpu: 229.06136851544528 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 254.69711451223827,
            "unit": "ns/iter",
            "extra": "iterations: 2766534\ncpu: 254.64583337851602 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 304.61928759997187,
            "unit": "ns/iter",
            "extra": "iterations: 2263672\ncpu: 304.6086526669943 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3870.0895443049726,
            "unit": "ns/iter",
            "extra": "iterations: 178716\ncpu: 3869.836377268961 ns\nthreads: 1"
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
          "id": "d84febbc99028d44a2ad80f99398586916fff36e",
          "message": "feat: implement truncation selection via configurable selection_rate\n\nPreviously target_size == pop_size, so no individuals were ever culled.\nNow the top selection_rate fraction of the population breeds each\ngeneration, with offspring padded back to pop_size. Defaults to 0.5.",
          "timestamp": "2026-06-25T15:20:30+01:00",
          "tree_id": "f2121f663aee1870e7edb72de998709790a8f046",
          "url": "https://github.com/benmandrew/counter/commit/d84febbc99028d44a2ad80f99398586916fff36e"
        },
        "date": 1782397535979,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "BenchSyntacticSimilaritySmall",
            "value": 675.3392631029632,
            "unit": "ns/iter",
            "extra": "iterations: 983231\ncpu: 675.2428737499123 ns\nthreads: 1"
          },
          {
            "name": "BenchSyntacticSimilarityLarge",
            "value": 2573.3232490943237,
            "unit": "ns/iter",
            "extra": "iterations: 278256\ncpu: 2572.4704444827794 ns\nthreads: 1"
          },
          {
            "name": "BenchSpecImpliesWarmCache",
            "value": 859.9596834591463,
            "unit": "ns/iter",
            "extra": "iterations: 794463\ncpu: 859.9149551332159 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/5",
            "value": 205.70514053164965,
            "unit": "ns/iter",
            "extra": "iterations: 3396886\ncpu: 205.68545367727975 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/10",
            "value": 233.26467353261293,
            "unit": "ns/iter",
            "extra": "iterations: 2995751\ncpu: 233.25749503213055 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/20",
            "value": 264.0009713335887,
            "unit": "ns/iter",
            "extra": "iterations: 2692175\ncpu: 263.9601697512237 ns\nthreads: 1"
          },
          {
            "name": "BenchCountTraces/50",
            "value": 312.6949291843039,
            "unit": "ns/iter",
            "extra": "iterations: 2237569\ncpu: 312.6910128804965 ns\nthreads: 1"
          },
          {
            "name": "BenchMutateSpecification",
            "value": 3881.599149256207,
            "unit": "ns/iter",
            "extra": "iterations: 180783\ncpu: 3881.1789991315563 ns\nthreads: 1"
          }
        ]
      }
    ]
  }
}