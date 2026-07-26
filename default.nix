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
		rev = "0c5d1694cdbf0649927038ec489d7fccc56978e6";
		hash = "sha256-K6YtUMlsHwe+LdF/ZME20oV1cth8xYMQLTqtjEKzRA8=";
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
