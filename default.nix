let
	pkgs = import <nixpkgs> {};
	srvsh = import (pkgs.fetchFromGitHub {
		owner = "TheMadman";
		repo = "srvsh";
		rev = "daa29475743076d49300dc7ee47f64afbbeb6863";
		hash = "sha256-/2QXa7LsxJbzonedWxeLBsQ8kbNJgCkGMjX0BTjSelc=";
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
