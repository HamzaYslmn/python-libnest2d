// python-libnest2d — comprehensive pybind11 bindings for libnest2d
// LGPL-3.0-or-later | Based on pynest2d by Ultimaker B.V.
//
// Exposes the full public API of tamasmeszaros/libnest2d master (header-only):
//   Point, Box, Circle, Segment, Item, Rectangle,
//   NfpConfig, BLConfig, DJDConfig, NestControl,
//   nest (NFP+FirstFit), nest_blp (BLP+FirstFit), nest_djd (NFP+DJD),
//   Alignment, Orientation enums, mm() helper.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <libnest2d/libnest2d.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace py = pybind11;
using namespace libnest2d;

// ---------------------------------------------------------------------------
// MARK: Type aliases
// ---------------------------------------------------------------------------
using NfpCfg  = NfpPlacer::Config;           // placers::NfpPConfig<PolygonImpl>
using BlCfg   = BottomLeftPlacer::Config;     // placers::BLConfig<PolygonImpl>
using DjdCfg  = DJDHeuristic::Config;         // selections::_DJDHeuristic<…>::Config

// Helper: copy item placement results back to Python list after nest()
static void _writeback(std::vector<Item>& items, py::list& py_items) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto& src = items[i];
        auto& dst = py_items[i].cast<Item&>();
        dst.translation(src.translation());
        dst.rotation(src.rotation());
        dst.binId(src.binId());
    }
}

// Helper: convert Python list[Item] → C++ vector<Item>
static std::vector<Item> _to_vec(py::list& py_items) {
    std::vector<Item> v;
    v.reserve(py::len(py_items));
    for (auto& obj : py_items) v.push_back(obj.cast<Item>());
    return v;
}

// MARK: GIL-safe callback wrappers
// When nest() releases the GIL for parallel C++ work, any Python callback
// called from a worker thread must re-acquire the GIL first.
static NfpCfg _safe_nfp(const NfpCfg& src) {
    NfpCfg c = src;
    if (c.object_function) {
        auto fn = c.object_function;
        c.object_function = [fn](const Item& it) -> double {
            py::gil_scoped_acquire acq;
            return fn(it);
        };
    }
    return c;
}

static NestControl _safe_ctl(const NestControl& src) {
    NestControl c;
    if (src.progressfn) {
        auto fn = src.progressfn;
        c.progressfn = [fn](unsigned rem) {
            py::gil_scoped_acquire acq;
            fn(rem);
        };
    }
    if (src.stopcond) {
        auto fn = src.stopcond;
        c.stopcond = [fn]() -> bool {
            py::gil_scoped_acquire acq;
            return fn();
        };
    }
    return c;
}

// ---------------------------------------------------------------------------
// MARK: Module definition
// ---------------------------------------------------------------------------
PYBIND11_MODULE(pynest2d, m)
{
    m.doc() = "Python bindings for libnest2d — 2D bin packing / polygon nesting.";

    // ── MARK: Point ───────────────────────────────────────────────────────
    py::class_<Point>(m, "Point",
        "2D integer coordinate (Clipper IntPoint).")
        .def(py::init([](Coord x, Coord y) { return Point{x, y}; }),
             py::arg("x"), py::arg("y"))
        .def("x",    [](const Point& p) -> Coord { return getX(p); })
        .def("y",    [](const Point& p) -> Coord { return getY(p); })
        .def("setX", [](Point& p, Coord v) { setX(p, v); }, py::arg("x"))
        .def("setY", [](Point& p, Coord v) { setY(p, v); }, py::arg("y"))
        .def("__eq__", [](const Point& a, const Point& b) {
            return getX(a) == getX(b) && getY(a) == getY(b);
        })
        .def("__repr__", [](const Point& p) {
            return "Point(" + std::to_string(getX(p)) + ", "
                            + std::to_string(getY(p)) + ")";
        });

    // ── MARK: Box ─────────────────────────────────────────────────────────
    py::class_<Box>(m, "Box", "Axis-aligned rectangular bin.")
        .def(py::init<Coord, Coord>(),
             py::arg("width"), py::arg("height"))
        .def(py::init<Coord, Coord, const Point&>(),
             py::arg("width"), py::arg("height"), py::arg("center"))
        .def("width",     &Box::width)
        .def("height",    &Box::height)
        .def("center",    &Box::center)
        .def("minCorner", [](const Box& b) -> Point { return b.minCorner(); })
        .def("maxCorner", [](const Box& b) -> Point { return b.maxCorner(); })
        .def("area",      [](const Box& b) { return b.area<double>(); })
        .def_static("infinite", [](const Point& c) { return Box::infinite(c); },
             py::arg("center") = Point{0, 0},
             "Create a near-infinite box around a center point.")
        .def("__repr__", [](const Box& b) {
            return "Box(" + std::to_string(b.width()) + ", "
                          + std::to_string(b.height()) + ")";
        });

    // ── MARK: Circle ──────────────────────────────────────────────────────
    py::class_<Circle>(m, "Circle", "Circle shape (center + radius).")
        .def(py::init<>())
        .def(py::init<const Point&, double>(),
             py::arg("center"), py::arg("radius"))
        .def("center",    [](const Circle& c) -> Point { return c.center(); })
        .def("setCenter", [](Circle& c, const Point& p) { c.center(p); },
             py::arg("center"))
        .def("radius",    [](const Circle& c) { return c.radius(); })
        .def("setRadius", [](Circle& c, double r) { c.radius(r); },
             py::arg("radius"))
        .def("area",      &Circle::area)
        .def("__repr__", [](const Circle& c) {
            return "Circle(center=Point(" + std::to_string(getX(c.center()))
                   + ", " + std::to_string(getY(c.center()))
                   + "), r=" + std::to_string(c.radius()) + ")";
        });

    // ── MARK: Segment ─────────────────────────────────────────────────────
    py::class_<Segment>(m, "Segment", "Directed line segment.")
        .def(py::init<>())
        .def(py::init<const Point&, const Point&>(),
             py::arg("p1"), py::arg("p2"))
        .def("first",    [](const Segment& s) -> Point { return s.first(); })
        .def("second",   [](const Segment& s) -> Point { return s.second(); })
        .def("setFirst",  [](Segment& s, const Point& p) { s.first(p); },
             py::arg("p"))
        .def("setSecond", [](Segment& s, const Point& p) { s.second(p); },
             py::arg("p"))
        .def("angleToXaxis", [](const Segment& s) {
            return static_cast<double>(s.angleToXaxis());
        })
        .def("sqlength", [](const Segment& s) { return s.sqlength<double>(); },
             "Squared length (double precision).")
        .def("__repr__", [](const Segment& s) {
            return "Segment((" + std::to_string(getX(s.first())) + ", "
                   + std::to_string(getY(s.first())) + ") -> ("
                   + std::to_string(getX(s.second())) + ", "
                   + std::to_string(getY(s.second())) + "))";
        });

    // ── MARK: Orientation enum ────────────────────────────────────────────
    py::enum_<Orientation>(m, "Orientation")
        .value("CLOCKWISE",         Orientation::CLOCKWISE)
        .value("COUNTER_CLOCKWISE", Orientation::COUNTER_CLOCKWISE)
        .export_values();

    // ── MARK: Alignment enum ──────────────────────────────────────────────
    py::enum_<NfpCfg::Alignment>(m, "Alignment")
        .value("CENTER",       NfpCfg::Alignment::CENTER)
        .value("BOTTOM_LEFT",  NfpCfg::Alignment::BOTTOM_LEFT)
        .value("BOTTOM_RIGHT", NfpCfg::Alignment::BOTTOM_RIGHT)
        .value("TOP_LEFT",     NfpCfg::Alignment::TOP_LEFT)
        .value("TOP_RIGHT",    NfpCfg::Alignment::TOP_RIGHT)
        .value("DONT_ALIGN",   NfpCfg::Alignment::DONT_ALIGN)
        .export_values();

    // ── MARK: NfpConfig ───────────────────────────────────────────────────
    py::class_<NfpCfg>(m, "NfpConfig",
        "Configuration for the NFP placer (default: 4 rotations, CENTER alignment).")
        .def(py::init<>())
        .def_readwrite("alignment",       &NfpCfg::alignment)
        .def_readwrite("starting_point",  &NfpCfg::starting_point)
        .def_readwrite("accuracy",        &NfpCfg::accuracy)
        .def_readwrite("explore_holes",   &NfpCfg::explore_holes)
        .def_readwrite("parallel",        &NfpCfg::parallel)
        .def_readwrite("object_function", &NfpCfg::object_function)
        // rotations is vector<Radians>; Radians doesn't auto-convert from float
        .def_property("rotations",
            [](const NfpCfg& c) -> std::vector<double> {
                return {c.rotations.begin(), c.rotations.end()};
            },
            [](NfpCfg& c, const std::vector<double>& v) {
                c.rotations.assign(v.begin(), v.end());
            });

    // ── MARK: BLConfig ────────────────────────────────────────────────────
    py::class_<BlCfg>(m, "BLConfig",
        "Configuration for the Bottom-Left placer.")
        .def(py::init<>())
        .def_readwrite("min_obj_distance", &BlCfg::min_obj_distance)
        .def_readwrite("epsilon",          &BlCfg::epsilon)
        .def_readwrite("allow_rotations",  &BlCfg::allow_rotations);

    // ── MARK: DJDConfig ───────────────────────────────────────────────────
    py::class_<DjdCfg>(m, "DJDConfig",
        "Configuration for the DJD (Dyckhoff) heuristic selector.")
        .def(py::init<>())
        .def_readwrite("try_reverse_order",       &DjdCfg::try_reverse_order)
        .def_readwrite("try_pairs",               &DjdCfg::try_pairs)
        .def_readwrite("try_triplets",            &DjdCfg::try_triplets)
        .def_readwrite("initial_fill_proportion", &DjdCfg::initial_fill_proportion)
        .def_readwrite("waste_increment",         &DjdCfg::waste_increment)
        .def_readwrite("allow_parallel",          &DjdCfg::allow_parallel)
        .def_readwrite("force_parallel",          &DjdCfg::force_parallel);

    // ── MARK: NestControl ─────────────────────────────────────────────────
    py::class_<NestControl>(m, "NestControl",
        "Progress / stop callbacks for nest().")
        .def(py::init<>())
        .def(py::init<ProgressFunction>(),              py::arg("progress"))
        .def(py::init<StopCondition>(),                 py::arg("stop"))
        .def(py::init<ProgressFunction, StopCondition>(),
             py::arg("progress"), py::arg("stop"))
        .def_readwrite("progress", &NestControl::progressfn)
        .def_readwrite("stop",     &NestControl::stopcond);

    // ── MARK: Item ────────────────────────────────────────────────────────
    py::class_<Item>(m, "Item", "A polygonal item to be packed.")
        // --- constructors ---
        .def(py::init([](const std::vector<Point>& pts) {
            ClipperLib::Path path(pts.begin(), pts.end());
            return Item(path);
        }), py::arg("vertices"),
           "Create an item from a list of contour vertices (no holes).")

        .def(py::init([](const std::vector<Point>& contour,
                         const std::vector<std::vector<Point>>& holes) {
            ClipperLib::Path c(contour.begin(), contour.end());
            ClipperLib::Paths hs;
            hs.reserve(holes.size());
            for (auto& h : holes)
                hs.emplace_back(h.begin(), h.end());
            return Item(std::move(c), std::move(hs));
        }), py::arg("contour"), py::arg("holes"),
           "Create an item from contour + hole rings.")

        // --- placement state ---
        .def("binId",  [](const Item& it) { return it.binId(); },
             "Bin index (-1 = unplaced).")
        .def("setBinId", [](Item& it, int id) { it.binId(id); },
             py::arg("id"))
        .def("isFixed", &Item::isFixed)
        .def("markAsFixedInBin", &Item::markAsFixedInBin, py::arg("binid"))
        .def("priority",    [](const Item& it) { return it.priority(); })
        .def("setPriority", [](Item& it, int p) { it.priority(p); },
             py::arg("priority"))

        // --- geometry queries ---
        .def("vertexCount",      &Item::vertexCount)
        .def("vertex",           &Item::vertex, py::arg("idx"))
        .def("setVertex",        &Item::setVertex, py::arg("idx"), py::arg("v"))
        .def("holeCount",        &Item::holeCount)
        .def("area",             &Item::area)
        .def("isContourConvex",  &Item::isContourConvex)
        .def("isHoleConvex",     &Item::isHoleConvex, py::arg("holeidx"))
        .def("areHolesConvex",   &Item::areHolesConvex)
        .def("boundingBox",      &Item::boundingBox)

        // --- reference vertices ---
        .def("rightmostTopVertex",   &Item::rightmostTopVertex)
        .def("leftmostBottomVertex", &Item::leftmostBottomVertex)
        .def("referenceVertex",      &Item::referenceVertex)

        // --- containment ---
        .def("isInside", [](const Item& it, const Point& p) {
            return it.isInside(p);
        }, py::arg("point"), "Test if a point is inside this item.")
        .def("isInsideItem", [](const Item& it, const Item& other) {
            return it.isInside(other);
        }, py::arg("other"), "Test if this item's shape is inside another item.")
        .def("isInsideBox", [](const Item& it, const Box& b) {
            return it.isInside(b);
        }, py::arg("box"), "Test if this item fits inside a box.")
        .def("isInsideCircle", [](const Item& it, const Circle& c) {
            return it.isInside(c);
        }, py::arg("circle"), "Test if this item fits inside a circle.")

        // --- static collision checks ---
        .def_static("intersects", &Item::intersects,
             py::arg("a"), py::arg("b"),
             "Test if two items' transformed shapes overlap.")
        .def_static("touches", &Item::touches,
             py::arg("a"), py::arg("b"),
             "Test if two items' transformed shapes touch (share boundary).")
        .def_static("orientation", &Item::orientation,
             "Return the expected polygon orientation (CW / CCW).")

        // --- transformations ---
        .def("translate",       &Item::translate, py::arg("delta"))
        .def("translation",     [](const Item& it) { return it.translation(); })
        .def("setTranslation",  [](Item& it, const Point& p) { it.translation(p); },
             py::arg("p"))
        .def("rotate",          &Item::rotate, py::arg("radians"))
        .def("rotation",        [](const Item& it) {
            return static_cast<double>(it.rotation());
        })
        .def("setRotation",     [](Item& it, double r) { it.rotation(r); },
             py::arg("radians"))
        .def("inflation",       [](const Item& it) { return it.inflation(); })
        .def("setInflation",    [](Item& it, Coord d) { it.inflation(d); },
             py::arg("distance"))
        .def("inflate",         &Item::inflate, py::arg("distance"),
             "Increase inflation by delta (cumulative).")
        .def("resetTransformation", &Item::resetTransformation)

        // --- MARK: transformed geometry (authoritative placement result) ---
        .def("transformedContour", [](const Item& it) -> std::vector<Point> {
            auto& ts = it.transformedShape();
            auto& c  = shapelike::contour(ts);
            return {c.begin(), c.end()};
        }, "Contour vertices after rotation + translation.")
        .def("transformedHoles", [](const Item& it)
            -> std::vector<std::vector<Point>>
        {
            auto& ts = it.transformedShape();
            auto& hs = shapelike::holes(ts);
            std::vector<std::vector<Point>> out;
            out.reserve(hs.size());
            for (auto& h : hs) out.push_back({h.begin(), h.end()});
            return out;
        }, "Hole vertex rings after rotation + translation.")

        // --- raw (untransformed) geometry ---
        .def("rawContour", [](const Item& it) -> std::vector<Point> {
            auto& c = shapelike::contour(it.rawShape());
            return {c.begin(), c.end()};
        }, "Original contour vertices (before any transformation).")
        .def("rawHoles", [](const Item& it)
            -> std::vector<std::vector<Point>>
        {
            auto& hs = shapelike::holes(it.rawShape());
            std::vector<std::vector<Point>> out;
            out.reserve(hs.size());
            for (auto& h : hs) out.push_back({h.begin(), h.end()});
            return out;
        }, "Original hole vertex rings (before any transformation).")

        // --- string repr ---
        .def("toString", &Item::toString)
        .def("__repr__", [](const Item& it) {
            return "<Item verts=" + std::to_string(it.vertexCount())
                 + " holes=" + std::to_string(it.holeCount())
                 + " area="  + std::to_string(it.area())
                 + " bin="   + std::to_string(it.binId()) + ">";
        });

    // ── MARK: Rectangle ──────────────────────────────────────────────────
    py::class_<Rectangle, Item>(m, "Rectangle",
        "Convenience subclass of Item for rectangular shapes.")
        .def(py::init<Coord, Coord>(),
             py::arg("width"), py::arg("height"))
        .def("width",  &Rectangle::width)
        .def("height", &Rectangle::height);

    // ── MARK: nest() — NFP + FirstFit ─────────────────────────────────────
    m.def("nest",
        [](py::list py_items, const Box& bin, Coord distance,
           const NfpCfg& pcfg, const NestControl& ctl)
        -> std::size_t
        {
            auto items = _to_vec(py_items);
            auto safe_pcfg = _safe_nfp(pcfg);
            auto safe_ctl  = _safe_ctl(ctl);
            NestConfig<NfpPlacer, FirstFitSelection> cfg(safe_pcfg);
            std::size_t n;
            { py::gil_scoped_release release;
              n = libnest2d::nest<NfpPlacer, FirstFitSelection>(
                      items.begin(), items.end(), bin, distance, cfg, safe_ctl);
            }
            _writeback(items, py_items);
            return n;
        },
        py::arg("items"),
        py::arg("bin"),
        py::arg("distance") = Coord{0},
        py::arg("config")   = NfpCfg{},
        py::arg("control")  = NestControl{},
        R"doc(
Pack items using NFP placer + first-fit selection.

Args:
    items:    List[Item] to pack (modified in-place).
    bin:      Box defining bin dimensions.
    distance: Minimum spacing between items (default 0).
    config:   NfpConfig placer options.
    control:  NestControl with optional progress/stop callbacks.

Returns:
    Number of bins used.
)doc"
    );

    // ── MARK: nest_blp() — Bottom-Left + FirstFit ─────────────────────────
    m.def("nest_blp",
        [](py::list py_items, const Box& bin, Coord distance,
           const BlCfg& pcfg, const NestControl& ctl)
        -> std::size_t
        {
            auto items = _to_vec(py_items);
            auto safe_ctl = _safe_ctl(ctl);
            NestConfig<BottomLeftPlacer, FirstFitSelection> cfg(pcfg);
            std::size_t n;
            { py::gil_scoped_release release;
              n = libnest2d::nest<BottomLeftPlacer, FirstFitSelection>(
                      items.begin(), items.end(), bin, distance, cfg, safe_ctl);
            }
            _writeback(items, py_items);
            return n;
        },
        py::arg("items"),
        py::arg("bin"),
        py::arg("distance") = Coord{0},
        py::arg("config")   = BlCfg{},
        py::arg("control")  = NestControl{},
        "Pack items using bottom-left placer + first-fit selection."
    );

    // ── MARK: nest_djd() — NFP + DJD Heuristic ───────────────────────────
    m.def("nest_djd",
        [](py::list py_items, const Box& bin, Coord distance,
           const NfpCfg& pcfg, const DjdCfg& scfg, const NestControl& ctl)
        -> std::size_t
        {
            auto items = _to_vec(py_items);
            auto safe_pcfg = _safe_nfp(pcfg);
            auto safe_ctl  = _safe_ctl(ctl);
            NestConfig<NfpPlacer, DJDHeuristic> cfg(safe_pcfg, scfg);
            std::size_t n;
            { py::gil_scoped_release release;
              n = libnest2d::nest<NfpPlacer, DJDHeuristic>(
                      items.begin(), items.end(), bin, distance, cfg, safe_ctl);
            }
            _writeback(items, py_items);
            return n;
        },
        py::arg("items"),
        py::arg("bin"),
        py::arg("distance")        = Coord{0},
        py::arg("placer_config")   = NfpCfg{},
        py::arg("selector_config") = DjdCfg{},
        py::arg("control")         = NestControl{},
        "Pack items using NFP placer + DJD heuristic selection (slower, better packing)."
    );

    // ── MARK: mm() — unit conversion ──────────────────────────────────────
    m.def("mm", [](double val) -> Coord { return mm(val); },
          py::arg("val") = 1.0,
          "Convert millimeters to libnest2d internal coordinate units.");

    // ── MARK: Constants ───────────────────────────────────────────────────
    m.attr("MM_IN_COORDS") = CoordType<PointImpl>::MM_IN_COORDS;
    m.attr("BIN_ID_UNSET") = BIN_ID_UNSET;
}
