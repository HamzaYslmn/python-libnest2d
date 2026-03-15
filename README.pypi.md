# python-libnest2d

**Unofficial** Python bindings for [libnest2d](https://github.com/tamasmeszaros/libnest2d) — a library for 2D bin packing / polygon nesting.

Based on [pynest2d](https://github.com/Ultimaker/pynest2d) by Ultimaker B.V., repackaged for easy `pip install`.

```bash
pip install python-libnest2d
```

## Quick Start

```python
from pynest2d import Point, Box, Item, nest

# Define items as polygons
square = Item([Point(0, 0), Point(100, 0), Point(100, 100), Point(0, 100)])
triangle = Item([Point(0, 0), Point(100, 0), Point(50, 100)])

# Define the bin (sheet)
bin_shape = Box(1000, 1000)

# Pack items into bins
num_bins = nest([square, triangle], bin_shape, distance=5)

# Read results
for item in [square, triangle]:
    t = item.translation()
    print(f"Bin {item.binId()}: ({t.x()}, {t.y()}) rot={item.rotation():.2f}")
```

## API

| Class | Description |
|-------|-------------|
| `Point(x, y)` | 2D integer coordinate |
| `Box(w, h)` | Rectangular bin |
| `Item(vertices)` | Polygonal item to nest |
| `NfpConfig()` | NFP placer configuration |
| `nest(items, bin, distance, config)` | Main nesting function |

## Building from Source

Requires: CMake 3.20+, C++17 compiler, Python 3.8+

```bash
pip install .
```

## License

**LGPL-3.0-or-later** — see [LICENSE](LICENSE) for full text.

| Component | License |
|-----------|---------|
| pynest2d (bindings) | LGPL-3.0 — © Ultimaker B.V. |
| libnest2d (engine) | LGPL-3.0 — © Tamás Mészáros |
| Clipper | BSL-1.0 — © Angus Johnson |
| Boost | BSL-1.0 |
| NLopt | LGPL-2.1+ — © Steven G. Johnson |
| pybind11 | BSD-3-Clause |

**This is NOT an official Ultimaker product.** For the official version, see [Ultimaker/pynest2d](https://github.com/Ultimaker/pynest2d).

Source code: https://github.com/HamzaYslmn/python-libnest2d
