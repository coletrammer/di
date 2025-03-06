{
  perSystem =
    { pkgs, ... }:
    let
      version = "0.1.0";
    in
    {
      packages.default = pkgs.stdenv.mkDerivation {
        name = "di-${version}";
        version = version;
        src = ../../.;

        buildInputs = with pkgs; [
          cmake
        ];
      };
    };
}
