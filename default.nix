let
	pkgs = import <nixpkgs> {};
	libadt = import (pkgs.fetchFromGitHub {
		owner = "TheMadman";
		repo = "libadt";
		rev = "b8e1fea53d4a3a120254b97f1331123a6fd5fcc9";
		hash = "sha256-cAhYJ8CU9vtgzMnO0CJUyno2v4YN3m/YURtA1pgDL2s=";
	});
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
		libadt
	];
}
