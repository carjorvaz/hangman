{ pkgs ? import <nixpkgs> {} }:

with pkgs;

mkShell {
  buildInputs = [
    gcc
    gnumake
    netcat-openbsd
  ];
}
