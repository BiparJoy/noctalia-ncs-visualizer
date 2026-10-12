{
  lib,
  stdenv,
  cmake,
  cava,
  libGL,
  kdePackages,
}:

let
  # The binary reports AURORA_NCS_VERSION from `project(aurora-ncs VERSION ...)`,
  # so read the version from that same line and it can never drift.
  cmakeLines = lib.splitString "\n" (builtins.readFile ../renderer/CMakeLists.txt);
  versionLine = lib.findFirst (l: lib.hasPrefix "project(aurora-ncs VERSION " l) null cmakeLines;
  version = builtins.head (builtins.match "project\\(aurora-ncs VERSION ([0-9.]+).*" versionLine);
in
stdenv.mkDerivation {
  pname = "aurora-ncs";
  inherit version;

  # Only what the CMake build reads. Editing workflows, docs, the plugin or the
  # flake itself does not change this source, so it does not trigger a rebuild.
  src = lib.fileset.toSource {
    root = ../renderer;
    fileset = lib.fileset.unions [
      ../renderer/CMakeLists.txt
      ../renderer/src
    ];
  };

  nativeBuildInputs = [
    cmake
    kdePackages.wrapQtAppsHook
  ];

  buildInputs = [
    kdePackages.qtbase
    kdePackages.qtdeclarative
    # main.cpp forces QT_QPA_PLATFORM=wayland, so the Wayland platform plugin is required.
    kdePackages.qtwayland
    kdePackages.layer-shell-qt
    libGL
  ];

  # The renderer starts `cava` by bare name (renderer/src/cava.cpp), so put it
  # on the wrapper's PATH.
  preFixup = ''
    qtWrapperArgs+=(--prefix PATH : ${lib.makeBinPath [ cava ]})
  '';

  meta = {
    description = "GPU renderer for the NCS Visualizer Noctalia plugin (tausif/ncs-visualizer)";
    homepage = "https://github.com/BiparJoy/noctalia-ncs-visualizer";
    license = lib.licenses.gpl3Plus;
    mainProgram = "aurora-ncs";
    platforms = lib.platforms.linux;
  };
}
