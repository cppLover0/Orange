{
  description = "meowmewoemeowmeowemwmowemowmwoew";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };

        clangPkgs = pkgs.llvmPackages_20;
      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            rsync
            meson
            ninja
            gcc15
            cmake
            gnumake
            git
            coreutils
            bash
            gnutar
            pigz
            patchelf
            help2man
            clangPkgs.llvm
            clangPkgs.clang
            qemu
            xorriso
            texinfo
            bison
            flex
            autoconf
            automake
            libtool
            autogen
            gtk-doc
            doxygen
            glib
            glib.dev
            itstool
            libxml2
            vala
            python313
            rustc
            cargo
            rustfmt
            pkg-config
            glslang
          ];

          shellHook = ''
            export CC=${clangPkgs.clang}/bin/clang
            export CXX=${clangPkgs.clang}/bin/clang++
            export LD=${clangPkgs.lld}/bin/ld.lld
          '';
        };
      }
    );
}
