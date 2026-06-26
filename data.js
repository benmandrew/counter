window.BENCHMARK_DATA = {
  "lastUpdate": 1782481949618,
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
          "id": "5c8702760acd914661f346d570cab68e2f3234ae",
          "message": "fix: enable sccache GHA cache backend via SCCACHE_GHA_ENABLED",
          "timestamp": "2026-06-26T14:45:07+01:00",
          "tree_id": "be70eba8d15a7a496f6ef02c5bd58ce500e038d5",
          "url": "https://github.com/benmandrew/counter/commit/5c8702760acd914661f346d570cab68e2f3234ae"
        },
        "date": 1782481804341,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 739.0545446401281,
            "unit": "ns/iter",
            "extra": "iterations: 959966\ncpu: 739.0531539658698 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2617.908086817267,
            "unit": "ns/iter",
            "extra": "iterations: 269624\ncpu: 2617.744993027327 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 870.8529666928075,
            "unit": "ns/iter",
            "extra": "iterations: 799948\ncpu: 870.8482038832527 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 206.16903479322,
            "unit": "ns/iter",
            "extra": "iterations: 3395863\ncpu: 206.11824799763716 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 239.95250681336313,
            "unit": "ns/iter",
            "extra": "iterations: 2917429\ncpu: 239.8988468956742 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 262.73857660538596,
            "unit": "ns/iter",
            "extra": "iterations: 2696090\ncpu: 262.6764158466519 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 322.5050580551939,
            "unit": "ns/iter",
            "extra": "iterations: 2170004\ncpu: 322.44272222539684 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3812.6988788625044,
            "unit": "ns/iter",
            "extra": "iterations: 182850\ncpu: 3811.966086956521 ns\nthreads: 1"
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
          "id": "49b2eee19ae9a0d75afec375990aa40b46aca50e",
          "message": "ci: gate build-and-test on check passing",
          "timestamp": "2026-06-26T14:48:21+01:00",
          "tree_id": "5c220f8d54222ebf0ad47b5f037503f92da6cf8e",
          "url": "https://github.com/benmandrew/counter/commit/49b2eee19ae9a0d75afec375990aa40b46aca50e"
        },
        "date": 1782481949327,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 641.2142994786103,
            "unit": "ns/iter",
            "extra": "iterations: 1089774\ncpu: 641.1871149430984 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2451.1241264675755,
            "unit": "ns/iter",
            "extra": "iterations: 285765\ncpu: 2450.9508897170754 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 903.0263139660816,
            "unit": "ns/iter",
            "extra": "iterations: 750476\ncpu: 903.0102415000613 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 204.2399973242386,
            "unit": "ns/iter",
            "extra": "iterations: 3453226\ncpu: 204.22263964188852 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 231.10704484782778,
            "unit": "ns/iter",
            "extra": "iterations: 3052188\ncpu: 231.09693767225355 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 256.0898837757512,
            "unit": "ns/iter",
            "extra": "iterations: 2715440\ncpu: 256.0586906725983 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 310.6386689226117,
            "unit": "ns/iter",
            "extra": "iterations: 2262468\ncpu: 310.6177285159394 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3893.066275643462,
            "unit": "ns/iter",
            "extra": "iterations: 179870\ncpu: 3892.477533774393 ns\nthreads: 1"
          }
        ]
      }
    ]
  }
}