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
          ];

          # Point curl/git at the nix CA bundle — required on NixOS, harmless on Ubuntu
          SSL_CERT_FILE = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";
          GIT_SSL_CAINFO = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";
          CURL_CA_BUNDLE = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";

          # cmake/black.cmake's wrapper script prepends its own lib dir and
          # preserves this, so the downloaded black-sat binary can find libfmt.
          LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [ pkgs.fmt_9 ];
        };
      });
}
