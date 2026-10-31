# LaTeX Pad

A local LaTeX equation scratchpad for Linux. Write math in one compact window, toggle to a sharp preview with Ctrl+Enter, and copy or export the result. No network connection and no input-length limit beyond what LaTeX itself handles.

## Use

- `Ctrl+Enter` or the Preview button switches between source and preview.
- Click the preview to return to editing.
- Tall or wide equations scroll in the preview.
- For notes, write prose around `$$...$$` math blocks. Each prose line becomes a paragraph, lines starting with `- ` become bullet points, inline `$...$` math is preserved, and text specials such as `%`, `&`, `#`, and `_` are escaped.
- `Ctrl+Shift+C` copies a PNG at the selected export scale.
- File menu: New, Open, Save, Save As. The last scratchpad and file are restored on startup.
- Export menu: SVG, PNG, and vector PDF. The transparent option applies to SVG and PNG.
- Zoom changes only the on-screen preview. The PNG scale changes the PNG and clipboard resolution.

## Build

Dependencies: C++17 compiler, CMake, Ninja, Qt 6 Widgets and SVG, `latex`, `pdflatex`, and `dvisvgm`. The SVG and PDF exporters use different LaTeX paths: `latex` with `dvisvgm` for the preview and SVG/PNG files, and `pdflatex` with the `preview` package for vector PDF files.

On Fedora:

```sh
sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel texlive-dvisvgm
```

Build and test:

```sh
cmake -S . -B build -G Ninja
cmake --build build
tests/run-tests.sh
```

Run:

```sh
./run.sh
```

`run.sh` prepends `.deps/usr` when the dependency packages are unpacked there, so the app also runs without system Qt or `dvisvgm` development packages installed.

## Tests

`tests/app_test.cpp` covers rendering, multiline math, invalid input recovery, source/preview toggling, render cancellation, tall and wide scrolling, export dimensions, transparency, PDF overwrite, clipboard image data, export invalidation after edits, compact window sizing, and scratchpad persistence.
