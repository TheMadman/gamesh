let
	pkgs = import <nixpkgs> {};
	srvsh = import (pkgs.fetchFromGitHub {
		owner = "TheMadman";
		repo = "srvsh";
		rev = "712898b70b9b53b4f445db4b6e4d2c87769ee044";
		hash = "sha256-US/FsySDVR1li9nZ7nbvv1mYi/EtzuxitqqtpdutO6I=";
	});
in
pkgs.stdenv.mkDerivation {
	pname = "gamesh";
	version = "0.0.1";
	src = ./.;

	nativeBuildInputs = [
		pkgs.cmake
	];

	buildInputs = [
		pkgs.sdl3
		srvsh
	];
}
