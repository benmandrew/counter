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
          ];

          # Point curl/git at the nix CA bundle — required on NixOS, harmless on Ubuntu
          SSL_CERT_FILE = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";
          GIT_SSL_CAINFO = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";
          CURL_CA_BUNDLE = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";
        };
      });
}
