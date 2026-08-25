Dear ImGui vendored snapshot
============================

This directory contains the small SDL2/SDLRenderer2 subset of Dear ImGui used by
the gb-recompiled runtime and the generated multi-ROM launcher.

Source snapshot:
- Upstream project: https://github.com/ocornut/imgui
- Version copied into this repo: 1.90.4

Included files:
- Core runtime sources: `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`
- Core headers: `imgui.h`, `imgui_internal.h`, `imconfig.h`
- stb dependencies bundled by Dear ImGui: `imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h`
- SDL2 backends: `backends/imgui_impl_sdl2.*`, `backends/imgui_impl_sdlrenderer2.*`
- Upstream license: `LICENSE.txt`

When updating ImGui, keep this snapshot minimal and only copy the files above
unless the runtime starts using additional backends or optional modules.
