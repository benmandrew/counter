{
  description = "counter — genetic repair of FRETISH specifications";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            # Core build system
            cmake
            gnumake
            gcc
            pkg-config

            # FetchContent / ExternalProject: git clones and HTTPS downloads
            git
            curl
            cacert

            # cpptrace: libunwind-based stack unwinding (CPPTRACE_UNWIND_WITH_LIBUNWIND=ON)
            libunwind

            # Lint targets (clang-tidy, clang-format, cppcheck, cpplint)
            clang-tools
            cppcheck
            cpplint

            # Docs targets (doxygen + sphinx)
            doxygen
            graphviz
            (python3.withPackages (ps: with ps; [ sphinx breathe furo ]))

            # Runtime dependency of the prebuilt black-sat binary (cmake/black.cmake),
            # which dynamically links libfmt.so.9 and does not bundle it.
            fmt_9

            # SMT backend for building black-sat from source (cmake/black.cmake's
            # fallback path on non-Ubuntu-24.04 hosts, where the prebuilt deb doesn't apply).
            z3
          ];

          # Point curl/git at the nix CA bundle — required on NixOS, harmless on Ubuntu
          SSL_CERT_FILE = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";
          GIT_SSL_CAINFO = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";
          CURL_CA_BUNDLE = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";

          # Deliberately not LD_LIBRARY_PATH: that leaks into every child
          # process launched from a shell with this devShell active (e.g.
          # `code .`), silently swapping in nix-store libs for unrelated
          # programs and crashing them on ABI mismatches. cmake/black.cmake
          # reads this plain var and bakes the path directly into the
          # black-sat wrapper script instead, scoped to just that one exec.
          COUNTER_FMT9_LIB_DIR = "${pkgs.fmt_9}/lib";
        };
      });
}
