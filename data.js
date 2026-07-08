window.BENCHMARK_DATA = {
  "lastUpdate": 1783515582885,
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
          "id": "facb19a7eb443e765487648bc836fdbb30dd2c92",
          "message": "fix: propagate worker thread exceptions to main thread instead of terminating\n\nAn uncaught exception in ThreadPool::worker_loop (e.g. from ganak or black\nreturning a non-zero exit code) caused std::terminate -> SIGABRT, crashing\nthe whole run with no useful error message. Store exceptions in the promise\nso they surface on future::get() in the main thread, then catch them in\nmain() and exit cleanly with a fatal: <message> diagnostic.",
          "timestamp": "2026-06-26T15:13:29+01:00",
          "tree_id": "2f092f01c1780bcc13c9cfca9a336fd3999e5ab4",
          "url": "https://github.com/benmandrew/counter/commit/facb19a7eb443e765487648bc836fdbb30dd2c92"
        },
        "date": 1782495311227,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 682.2087287003706,
            "unit": "ns/iter",
            "extra": "iterations: 996437\ncpu: 682.0727110695408 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2445.482755954507,
            "unit": "ns/iter",
            "extra": "iterations: 284533\ncpu: 2445.2326057083014 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 865.8476959967734,
            "unit": "ns/iter",
            "extra": "iterations: 800281\ncpu: 865.7845431792082 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 206.2405216362074,
            "unit": "ns/iter",
            "extra": "iterations: 3391329\ncpu: 206.22404785852387 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 233.10898449399295,
            "unit": "ns/iter",
            "extra": "iterations: 3003675\ncpu: 233.0721263119346 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 259.9794118913435,
            "unit": "ns/iter",
            "extra": "iterations: 2694128\ncpu: 259.9687572379633 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 312.8954411672585,
            "unit": "ns/iter",
            "extra": "iterations: 2235660\ncpu: 312.85461250816314 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3875.8243723046703,
            "unit": "ns/iter",
            "extra": "iterations: 181099\ncpu: 3875.6528583813288 ns\nthreads: 1"
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
          "id": "78f9091b87c1f24fac147054e0158c571f02a2d4",
          "message": "feat: add Nix dev shell as primary development workflow\n\nAdds flake.nix providing cmake, make, gcc, git, curl, libunwind, and\nlint tools (clang-tidy, clang-format, cppcheck, cpplint). CA bundle env\nvars ensure HTTPS downloads work on NixOS. SPOT, ganak, and black-sat\ncontinue to be fetched/built by CMake at configure time.\n\nUpdates README and CLAUDE.md to present `nix develop` as the\nfirst-class entry point, with the non-Nix path documented as supported\nbut secondary.",
          "timestamp": "2026-07-03T16:23:28+01:00",
          "tree_id": "af38aabdd586c39248127dffa3a6d8151a5c9447",
          "url": "https://github.com/benmandrew/counter/commit/78f9091b87c1f24fac147054e0158c571f02a2d4"
        },
        "date": 1783092449584,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 648.3040554837355,
            "unit": "ns/iter",
            "extra": "iterations: 1082263\ncpu: 648.2735268599222 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2463.5509720628306,
            "unit": "ns/iter",
            "extra": "iterations: 285784\ncpu: 2463.438862917448 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 905.6133488355589,
            "unit": "ns/iter",
            "extra": "iterations: 760321\ncpu: 905.5480093276394 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 203.21336534099433,
            "unit": "ns/iter",
            "extra": "iterations: 3436261\ncpu: 203.1921259764611 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 230.2990312306697,
            "unit": "ns/iter",
            "extra": "iterations: 2919374\ncpu: 230.23883579150873 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 257.1070654230573,
            "unit": "ns/iter",
            "extra": "iterations: 2724437\ncpu: 257.0466734962123 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 309.7405745798356,
            "unit": "ns/iter",
            "extra": "iterations: 2257963\ncpu: 309.66705876048485 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3930.9674294688944,
            "unit": "ns/iter",
            "extra": "iterations: 178290\ncpu: 3930.034903808402 ns\nthreads: 1"
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
          "id": "2340b3d507ca2007f8a4ff34f94704e596c0d0fa",
          "message": "fix: stop leaking Nix dev shell LD_LIBRARY_PATH into unrelated processes\n\nExporting LD_LIBRARY_PATH shell-wide affects every child process launched\nfrom a terminal with this devShell active (via direnv's `use flake`),\nnot just build/test commands. This silently swapped a mismatched\nnix-store libgcc_s.so.1 into VS Code when launched as `code .` from the\nproject directory, crashing it with SIGTRAP.\n\nstdenv.cc.cc.lib turned out to be unnecessary: nix's gcc-wrapper already\nbakes a correct RPATH for its own libstdc++/libgcc_s automatically, since\nCMake treats nix store paths as non-system and includes them (verified by\nrunning the debug test binary with LD_LIBRARY_PATH explicitly unset).\n\nThe only genuine runtime need was fmt_9, for the prebuilt black-sat .deb's\nwrapper script. That's now passed via a plain, inert env var\n(COUNTER_FMT9_LIB_DIR) and baked directly into the wrapper at configure\ntime, scoped to just that one exec instead of the whole shell.",
          "timestamp": "2026-07-06T14:30:29+01:00",
          "tree_id": "38a2c41b6dd69ce616449c32af1b51d5e89aaae4",
          "url": "https://github.com/benmandrew/counter/commit/2340b3d507ca2007f8a4ff34f94704e596c0d0fa"
        },
        "date": 1783346502391,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 526.3234429866371,
            "unit": "ns/iter",
            "extra": "iterations: 1336758\ncpu: 526.2670266420698 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2206.154235493687,
            "unit": "ns/iter",
            "extra": "iterations: 319278\ncpu: 2205.778083676295 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 877.7766466762449,
            "unit": "ns/iter",
            "extra": "iterations: 790380\ncpu: 877.7469672815605 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 129.25718770714042,
            "unit": "ns/iter",
            "extra": "iterations: 5420609\ncpu: 129.2353774271489 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 146.47547406727537,
            "unit": "ns/iter",
            "extra": "iterations: 4783182\ncpu: 146.4542687273868 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 164.54492556491326,
            "unit": "ns/iter",
            "extra": "iterations: 4241950\ncpu: 164.52736335883267 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 199.02178635796105,
            "unit": "ns/iter",
            "extra": "iterations: 3517660\ncpu: 198.96386944730284 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3751.9125979699556,
            "unit": "ns/iter",
            "extra": "iterations: 186792\ncpu: 3750.591770525507 ns\nthreads: 1"
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
          "id": "a57c90fc050a4a85cf2f76822e225942569395ed",
          "message": "fix: stop CI cache hits from being defeated by ExternalProject rebuilds\n\nExternalProject_Add regenerates its per-step driver scripts on every\nconfigure. When a restored CI cache already contains the fully-built\nSPOT/black-sat third_party trees, those regenerated scripts get a newer\nmtime than the cached \"-done\" stamps, so Make reruns the whole\nconfigure/build/install anyway. Diagnosed via gh run logs: SPOT's ~9-13\nminute from-source rebuild was repeating on every one of the 5 CI matrix\nlegs regardless of cache hits, wasting ~45-65 CI-minutes per run.\n\nSkip re-declaring the ExternalProject_Add entirely (replacing it with a\nno-op custom target) whenever a prior run's \"-done\" stamp is already\npresent, for spot_project and black.cmake's three from-source sub-builds\n(tsl-hopscotch-map, nlohmann_json, black itself).\n\nSeparately, sccache was already wired up via CMAKE_C/CXX_COMPILER_LAUNCHER\nfor the main project, but never reached these ExternalProject sub-builds:\nSPOT's autotools ./configure picks its own CXX with no override, and the\nblack.cmake CMAKE_ARGS never passed the launcher through. Both now thread\nCMAKE_CXX_COMPILER_LAUNCHER in explicitly, so a genuine cache miss at\nleast compiles fast on a subsequent run.",
          "timestamp": "2026-07-06T14:50:01+01:00",
          "tree_id": "50d1b79eca2cedd33f9b6b79e958c503388b9d95",
          "url": "https://github.com/benmandrew/counter/commit/a57c90fc050a4a85cf2f76822e225942569395ed"
        },
        "date": 1783346621006,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 526.9278018505942,
            "unit": "ns/iter",
            "extra": "iterations: 1329300\ncpu: 526.8849040848567 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2192.7569700224003,
            "unit": "ns/iter",
            "extra": "iterations: 318471\ncpu: 2192.5653136392325 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 860.7856988033025,
            "unit": "ns/iter",
            "extra": "iterations: 793528\ncpu: 860.7171139518705 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 129.22128696150554,
            "unit": "ns/iter",
            "extra": "iterations: 5415127\ncpu: 129.21247701115786 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 146.56330831251387,
            "unit": "ns/iter",
            "extra": "iterations: 4773757\ncpu: 146.55154943999028 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 165.314684405103,
            "unit": "ns/iter",
            "extra": "iterations: 4259226\ncpu: 165.30165926860894 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 199.27061864896578,
            "unit": "ns/iter",
            "extra": "iterations: 3519702\ncpu: 199.2546226924892 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3789.6457190715796,
            "unit": "ns/iter",
            "extra": "iterations: 185065\ncpu: 3789.1533353146187 ns\nthreads: 1"
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
          "id": "6a6b6a3b78e4abb015db5161daca0f245651c683",
          "message": "Merge pull request #5 from benmandrew/feature/ltl-formaliser-cli\n\nfeat: persistent LTL-formaliser CLI runner + FRETish-valid Requirement::to_string",
          "timestamp": "2026-07-07T11:52:41+01:00",
          "tree_id": "6d720b9de08bb38f78e58c408830b243cfacda29",
          "url": "https://github.com/benmandrew/counter/commit/6a6b6a3b78e4abb015db5161daca0f245651c683"
        },
        "date": 1783422515459,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 545.6766827054872,
            "unit": "ns/iter",
            "extra": "iterations: 1276486\ncpu: 545.6507936632286 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2234.4025531696393,
            "unit": "ns/iter",
            "extra": "iterations: 312396\ncpu: 2234.1677390235473 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 848.866899289286,
            "unit": "ns/iter",
            "extra": "iterations: 818493\ncpu: 848.7662154716048 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 162.27166654742433,
            "unit": "ns/iter",
            "extra": "iterations: 4332970\ncpu: 162.26006688253088 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 183.83791029239694,
            "unit": "ns/iter",
            "extra": "iterations: 3802197\ncpu: 183.80739477728264 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 211.95832134567257,
            "unit": "ns/iter",
            "extra": "iterations: 3361097\ncpu: 211.91618212744228 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 251.2760536532803,
            "unit": "ns/iter",
            "extra": "iterations: 2780445\ncpu: 251.23647977212272 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3783.088246564643,
            "unit": "ns/iter",
            "extra": "iterations: 187894\ncpu: 3782.755053381162 ns\nthreads: 1"
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
          "id": "cad434aed8e5b9f81e6688d8378c6a40fd4934b5",
          "message": "feat: add Always timing to the Timing algebraic data type\n\nAdds the Always modality (response must hold at every timepoint from the\ncondition onward, i.e. G R) as a seventh Timing variant, completing all\ndispatch sites:\n\n- requirement_to_ltl emits body G R; to_string emits \"always\" (both were\n  previously swallowed by the Eventually catch-all).\n- serialisation to_json/from_json/validator handle the \"Always\" type.\n- syntactic_similarity models Always as the top of the timing order:\n  its downset is every timing except the AfterTicks family.\n- Halstead counting, mutation (placeholder ForTicks weakening), CLI help,\n  and docs updated; fuzzer now differentially tests Always against the\n  real FRET CLI.\n\nVerified: 22/22 tests pass, lint/format clean, and the LTL translation\nmatches FRET's own \"always\" formalisation (checked via the equivalence\nfuzzer with ltlfilt).",
          "timestamp": "2026-07-08T13:45:54+01:00",
          "tree_id": "f3ed5bf6e3c8c27bae692f3639a4b2f600fa2b03",
          "url": "https://github.com/benmandrew/counter/commit/cad434aed8e5b9f81e6688d8378c6a40fd4934b5"
        },
        "date": 1783515581951,
        "tool": "googlecpp",
        "benches": [
          {
            "name": "Syntactic similarity - small formulas (3 variables)",
            "value": 539.5216483036788,
            "unit": "ns/iter",
            "extra": "iterations: 1307793\ncpu: 539.4695467860739 ns\nthreads: 1"
          },
          {
            "name": "Syntactic similarity - large formulas (11 variables, O(n*m) shared_subformulae)",
            "value": 2230.203534918055,
            "unit": "ns/iter",
            "extra": "iterations: 308522\ncpu: 2229.9246374650756 ns\nthreads: 1"
          },
          {
            "name": "Spec implication check - warm black cache",
            "value": 862.0965634353333,
            "unit": "ns/iter",
            "extra": "iterations: 810897\ncpu: 862.0341547693478 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:5",
            "value": 161.6528680716383,
            "unit": "ns/iter",
            "extra": "iterations: 4325537\ncpu: 161.6452054392322 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:10",
            "value": 183.88615583765815,
            "unit": "ns/iter",
            "extra": "iterations: 3777585\ncpu: 183.86197504490312 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:20",
            "value": 206.86451404970205,
            "unit": "ns/iter",
            "extra": "iterations: 3385089\ncpu: 206.8465216128734 ns\nthreads: 1"
          },
          {
            "name": "Trace model counting - matrix exponentiation/steps:50",
            "value": 251.91482575501166,
            "unit": "ns/iter",
            "extra": "iterations: 2779420\ncpu: 251.89874830000502 ns\nthreads: 1"
          },
          {
            "name": "Mutate specification - 3-guarantee takeoff spec",
            "value": 3764.6780692730254,
            "unit": "ns/iter",
            "extra": "iterations: 186624\ncpu: 3764.2696009087776 ns\nthreads: 1"
          }
        ]
      }
    ]
  }
}