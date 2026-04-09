let
	pkgs = import <nixpkgs> {};
	libadt = import (pkgs.fetchFromGitHub {
		owner = "TheMadman";
		repo = "libadt";
		rev = "6be08777cb39a84e81b74ce1591a74558ae9c630";
		hash = "sha256-+rcZ5+IbsWjD9AjWxEMM3PWLWonPnc25hbk4UPHOtlU=";
	});
	srvsh = import (pkgs.fetchFromGitHub {
		owner = "TheMadman";
		repo = "srvsh";
		rev = "56bd0d01b683e256c723352d0160298ef568acbf";
		hash = "sha256-ka+sgW8FY1KbYRC0lcznAtkA+Jiw+RqyBLYVPrYO62k=";
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
