# python-libnest2d

**Unofficial** Python bindings for [libnest2d](https://github.com/tamasmeszaros/libnest2d) — a library for 2D bin packing / polygon nesting.

Based on [pynest2d](https://github.com/Ultimaker/pynest2d) by Ultimaker B.V., repackaged for easy `pip install`.

```bash
pip install python-libnest2d
```

## Quick Start

```python
from pynest2d import Point, Box, Item, Rectangle, nest

# Define items — polygons or rectangles
square = Item([Point(0, 0), Point(100, 0), Point(100, 100), Point(0, 100)])
triangle = Item([Point(0, 0), Point(100, 0), Point(50, 100)])
rect = Rectangle(200, 80)

# Define the bin (sheet)
bin_box = Box(1000, 500)

# Pack items into bins
items = [square, triangle, rect]
num_bins = nest(items, bin_box, distance=5)

# Read results
for item in items:
    t = item.translation()
    print(f"Bin {item.binId()}: ({t.x()}, {t.y()}) rot={item.rotation():.2f}")
```

## Examples

### Items with Holes

```python
from pynest2d import Point, Box, Item, nest

# Outer contour + one hole
contour = [Point(0,0), Point(0,500), Point(500,500), Point(500,0), Point(0,0)]
hole = [Point(100,100), Point(100,400), Point(400,400), Point(400,100), Point(100,100)]
part = Item(contour, [hole])

print(f"Holes: {part.holeCount()}, Convex: {part.isContourConvex()}")
```

### NFP Placer Configuration

```python
from pynest2d import NfpConfig, Alignment, nest, Rectangle, Box
import math

cfg = NfpConfig()
cfg.rotations = [0.0, math.pi / 2]     # Only 0° and 90°
cfg.alignment = Alignment.BOTTOM_LEFT   # Align to bottom-left corner
cfg.accuracy = 0.8                      # Higher quality (0.0–1.0)
cfg.parallel = True                     # Multi-threaded

items = [Rectangle(200, 100) for _ in range(10)]
nest(items, Box(1000, 500), config=cfg)
```

### Custom Scoring (Object Function)

```python
from pynest2d import NfpConfig, nest, Rectangle, Box

cfg = NfpConfig()
cfg.rotations = [0.0]
# Prefer items placed lower (minimize Y)
cfg.object_function = lambda item: float(item.translation().y())

items = [Rectangle(200, 100), Rectangle(150, 150)]
nest(items, Box(500, 500), config=cfg)
```

### Bottom-Left Placer

```python
from pynest2d import BLConfig, nest_blp, Rectangle, Box

cfg = BLConfig()
cfg.allow_rotations = True

items = [Rectangle(200, 100) for _ in range(5)]
num_bins = nest_blp(items, Box(500, 500), config=cfg)
```

### DJD Heuristic (Better Packing)

```python
from pynest2d import DJDConfig, NfpConfig, nest_djd, Rectangle, Box

dcfg = DJDConfig()
dcfg.try_pairs = True       # Try pair combinations
dcfg.try_triplets = False   # Skip triplets (slow for >100 items)

items = [Rectangle(200, 100) for _ in range(20)]
num_bins = nest_djd(items, Box(1000, 500), selector_config=dcfg)
```

### Progress & Stop Callbacks

```python
from pynest2d import NestControl, nest, Rectangle, Box

def on_progress(remaining):
    print(f"{remaining} items left...")

ctl = NestControl(progress=on_progress, stop=lambda: False)
items = [Rectangle(100, 50) for _ in range(30)]
nest(items, Box(500, 500), control=ctl)
```

### Collision Detection

```python
from pynest2d import Item, Point

a = Item([Point(0,0), Point(0,100), Point(100,100), Point(100,0)])
b = Item([Point(50,50), Point(50,150), Point(150,150), Point(150,50)])

a.setTranslation(Point(0, 0))
b.setTranslation(Point(0, 0))

print(f"Intersects: {Item.intersects(a, b)}")
print(f"Touches: {Item.touches(a, b)}")
```

### Reading Placed Geometry

```python
from pynest2d import Item, Point, Box, nest

items = [Item([Point(0,0), Point(0,100), Point(100,100), Point(100,0)])]
nest(items, Box(500, 500))

# Authoritative placement (rotation + translation applied by libnest2d)
placed_contour = items[0].transformedContour()
placed_holes = items[0].transformedHoles()

# Original geometry (untransformed)
raw_contour = items[0].rawContour()
raw_holes = items[0].rawHoles()
```

### Unit Conversion

```python
from pynest2d import mm, MM_IN_COORDS

print(f"1 mm = {mm(1.0)} internal units")
print(f"MM_IN_COORDS = {MM_IN_COORDS}")
```

## API Reference

### Core Types

| Class | Description |
|-------|-------------|
| `Point(x, y)` | 2D integer coordinate |
| `Box(w, h [, center])` | Axis-aligned rectangular bin |
| `Circle(center, radius)` | Circle shape |
| `Segment(p1, p2)` | Directed line segment |
| `Item(vertices)` | Polygonal item from contour |
| `Item(contour, holes)` | Polygonal item with holes |
| `Rectangle(w, h)` | Convenience Item subclass |

### Nest Functions

| Function | Placer | Selector | Best For |
|----------|--------|----------|----------|
| `nest()` | NFP | FirstFit | General-purpose (default) |
| `nest_blp()` | Bottom-Left | FirstFit | Simple, fast |
| `nest_djd()` | NFP | DJD Heuristic | Better packing, slower |

All accept: `items, bin, distance=0, config=..., control=NestControl()`

### Configuration

| Class | Key Fields |
|-------|------------|
| `NfpConfig` | `rotations`, `alignment`, `accuracy`, `object_function`, `parallel` |
| `BLConfig` | `allow_rotations`, `min_obj_distance`, `epsilon` |
| `DJDConfig` | `try_pairs`, `try_triplets`, `initial_fill_proportion`, `waste_increment` |
| `NestControl` | `progress(remaining)`, `stop() → bool` |

### Enums

| Enum | Values |
|------|--------|
| `Alignment` | `CENTER`, `BOTTOM_LEFT`, `BOTTOM_RIGHT`, `TOP_LEFT`, `TOP_RIGHT`, `DONT_ALIGN` |
| `Orientation` | `CLOCKWISE`, `COUNTER_CLOCKWISE` |

## Building from Source

Requires: CMake 3.20+, C++17 compiler, Boost, Python 3.8+

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

**ORIGINAL LIBNEST2D SOURCE:**
[tamasmeszaros/libnest2d](https://github.com/tamasmeszaros/libnest2d).

Source code: https://github.com/HamzaYslmn/python-libnest2d