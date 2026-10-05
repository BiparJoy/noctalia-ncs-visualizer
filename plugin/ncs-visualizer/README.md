# NCS Visualizer

The NCS audio-visualizer orb from [spicetify-visualizer](https://github.com/Konsl/spicetify-visualizer)
as a Noctalia desktop widget. It reacts to whatever your speakers are playing
(any player, browser or game), and by default it takes its colour from your
Noctalia theme, so it changes with your wallpaper.

All 322 × 322 = **103,684 dots** are drawn every frame on the GPU, using the
original shaders.

![The orb in three colours](https://raw.githubusercontent.com/BiparJoy/noctalia-ncs-visualizer/main/screenshots/colours.png)

## Requirements

The orb is drawn by a small companion renderer, **`aurora-ncs`**. Noctalia plugins
can't ship compiled code, so you build it yourself; it takes a minute and needs no sudo:

```sh
git clone https://github.com/BiparJoy/noctalia-ncs-visualizer
cd noctalia-ncs-visualizer
./install.sh        # installs ~/.local/bin/aurora-ncs
```

`install.sh` prints the build-dependency packages for Fedora, Arch, Debian/Ubuntu
and openSUSE if anything is missing. At runtime you need **cava**, a Wayland
compositor with **wlr-layer-shell** (niri, Hyprland, Sway, river, …) and
**OpenGL 3.3**.

## Use

1. Install the plugin, then open the **desktop widget editor** and add **NCS Visualizer**.
2. Move and resize it like any other desktop widget. The orb follows the box.
3. In the widget's own settings, turn **Background off**. Otherwise Noctalia
   draws its usual card behind the orb.

The plugin's settings page covers colour (theme role or custom, with a separate
glow colour), opacity, dot density and size, glow, motion model, sensitivity,
frame rate, hide-when-silent, and showing the orb behind or above windows.

**Control Center tile:** click to show or hide the orb, right-click for settings.

**Keybind:** run `noctalia msg plugin tausif/ncs-visualizer:service all toggle`
(or `on` / `off`). For example, in niri:

```kdl
Mod+Alt+V { spawn "noctalia" "msg" "plugin" "tausif/ncs-visualizer:service" "all" "toggle"; }
```

## Background process (disclosure)

While at least one orb is placed and the visualizer is on, the plugin runs
**`aurora-ncs`**, which in turn runs **`cava`** to read the audio. The renderer
stops by itself within a second when you remove the last orb or turn the
visualizer off. It also stops within 20 seconds if Noctalia exits. It doesn't
access the network.

On Noctalia releases without `noctalia.getColor` (5.0 betas), the plugin runs
`noctalia theme <wallpaper>` once per wallpaper change to read the theme palette.

## Performance

At 60 fps with the original density, it uses about 1–2 % CPU for the renderer and
2 % for cava on an i7-12700H with Iris Xe. After the silence timeout it stops
drawing and holds the last frame.

## Credits

Shaders and pipeline from [spicetify-visualizer](https://github.com/Konsl/spicetify-visualizer)
by Konsl. The "Aurora" motion model comes from the Aurora player. Ported from the
[Plasma widget](https://github.com/BiparJoy/plasma-ncs-visualizer). GPL-3.0-or-later.
