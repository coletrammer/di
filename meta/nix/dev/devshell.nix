{
  perSystem =
    {
      config,
      pkgs,
      ...
    }:
    let
      constants = import ../constants.nix;
      gcc = pkgs."gcc${constants.gccVersion}";
      clang = pkgs."llvmPackages_${constants.clangVersion}".libcxxClang;
      stdenv = pkgs."gcc${constants.gccVersion}Stdenv";
    in
    {
      devShells.default = pkgs.mkShell.override { inherit stdenv; } {
        packages =
          [
            # Compilers
            clang
            gcc
          ]
          # Treefmt and all individual formatters
          ++ [ config.treefmt.build.wrapper ]
          ++ builtins.attrValues config.treefmt.build.programs
          ++ (with pkgs; [
            # Build support
            cmake
            ninja
            ccache

            # justfile support
            just

            # Docs
            doxygen
            graphviz

            # Coverage
            lcov
          ]);
      };
    };
}
