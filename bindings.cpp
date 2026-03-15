// python-libnest2d — pybind11 bindings for libnest2d
// LGPL-3.0-or-later | Based on pynest2d by Ultimaker B.V.
//
// Bindings written against tamasmeszaros/libnest2d master (header-only).
// API verified against actual nester.hpp, geometry_traits.hpp, nfpplacer.hpp.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <libnest2d/libnest2d.hpp>

#include <string>
#include <vector>

namespace py = pybind11;
using namespace libnest2d;

// ---------------------------------------------------------------------------
// MARK: Type aliases from libnest2d.hpp
// ---------------------------------------------------------------------------
// Point  = ClipperLib::IntPoint (via PointImpl)
// Box    = _Box<PointImpl>
// Item   = _Item<PolygonImpl>
// Coord  = ClipperLib::cInt (long long)
// NfpPlacer::Config = placers::NfpPConfig<PolygonImpl>

using NfpCfg = NfpPlacer::Config;

// ---------------------------------------------------------------------------
// MARK: Module definition
// ---------------------------------------------------------------------------

PYBIND11_MODULE(pynest2d, m)
{
    m.doc() = "Python bindings for libnest2d — 2D bin packing / polygon nesting.";

    // ── Point ──────────────────────────────────────────────────────────────
    py::class_<Point>(m, "Point",
        "2D integer coordinate (Clipper IntPoint).")
        .def(py::init([](Coord x, Coord y) {
            return Point{x, y};
        }), py::arg("x"), py::arg("y"))
        .def("x", [](const Point& p) -> Coord { return getX(p); })
        .def("y", [](const Point& p) -> Coord { return getY(p); })
        .def("__repr__", [](const Point& p) {
            return "Point(" + std::to_string(getX(p)) + ", " +
                   std::to_string(getY(p)) + ")";
        });

    // ── Box ────────────────────────────────────────────────────────────────
    py::class_<Box>(m, "Box",
        "Axis-aligned rectangular bin.")
        .def(py::init<Coord, Coord>(), py::arg("width"), py::arg("height"))
        .def(py::init<Coord, Coord, const Point&>(),
             py::arg("width"), py::arg("height"), py::arg("center"))
        .def("width",  &Box::width)
        .def("height", &Box::height)
        .def("center", &Box::center)
        .def("area", [](const Box& b) { return b.area<double>(); })
        .def("__repr__", [](const Box& b) {
            return "Box(" + std::to_string(b.width()) + ", " +
                   std::to_string(b.height()) + ")";
        });

    // ── Alignment enum ────────────────────────────────────────────────────
    py::enum_<NfpCfg::Alignment>(m, "Alignment")
        .value("CENTER",       NfpCfg::Alignment::CENTER)
        .value("BOTTOM_LEFT",  NfpCfg::Alignment::BOTTOM_LEFT)
        .value("BOTTOM_RIGHT", NfpCfg::Alignment::BOTTOM_RIGHT)
        .value("TOP_LEFT",     NfpCfg::Alignment::TOP_LEFT)
        .value("TOP_RIGHT",    NfpCfg::Alignment::TOP_RIGHT)
        .value("DONT_ALIGN",   NfpCfg::Alignment::DONT_ALIGN)
        .export_values();

    // ── NfpConfig ─────────────────────────────────────────────────────────
    py::class_<NfpCfg>(m, "NfpConfig",
        "Configuration for the NFP placer algorithm.")
        .def(py::init<>())
        .def_readwrite("alignment",      &NfpCfg::alignment)
        .def_readwrite("starting_point", &NfpCfg::starting_point)
        .def_readwrite("rotations",      &NfpCfg::rotations)
        .def_readwrite("accuracy",       &NfpCfg::accuracy)
        .def_readwrite("explore_holes",  &NfpCfg::explore_holes)
        .def_readwrite("parallel",       &NfpCfg::parallel);

    // ── Item ───────────────────────────────────────────────────────────────
    py::class_<Item>(m, "Item",
        "A polygonal item to be packed.")
        .def(py::init([](const std::vector<Point>& pts) {
            // Build a ClipperLib::Path from the points, then create an Item
            ClipperLib::Path path(pts.begin(), pts.end());
            return Item(path);
        }), py::arg("vertices"))

        // Placement state
        .def("binId", [](const Item& it) { return it.binId(); },
             "Bin index this item was assigned to (-1 = unplaced).")
        .def("setBinId", [](Item& it, int id) { it.binId(id); }, py::arg("id"))
        .def("isFixed", &Item::isFixed)
        .def("markAsFixedInBin", &Item::markAsFixedInBin, py::arg("binid"))
        .def("priority", [](const Item& it) { return it.priority(); })
        .def("setPriority", [](Item& it, int p) { it.priority(p); }, py::arg("priority"))

        // Geometry queries
        .def("vertexCount", &Item::vertexCount)
        .def("vertex", &Item::vertex, py::arg("idx"))
        .def("holeCount", &Item::holeCount)
        .def("area", &Item::area)
        .def("isContourConvex", &Item::isContourConvex)
        .def("boundingBox", &Item::boundingBox)

        // Containment
        .def("isInside", [](const Item& it, const Point& p) { return it.isInside(p); })

        // Transformations
        .def("translate", &Item::translate, py::arg("delta"))
        .def("translation", [](const Item& it) { return it.translation(); })
        .def("setTranslation", [](Item& it, const Point& p) { it.translation(p); }, py::arg("p"))
        .def("rotate", &Item::rotate, py::arg("radians"))
        .def("rotation", [](const Item& it) { return static_cast<double>(it.rotation()); })
        .def("setRotation", [](Item& it, double r) { it.rotation(r); }, py::arg("radians"))
        .def("inflation", [](const Item& it) { return it.inflation(); })
        .def("setInflation", [](Item& it, Coord d) { it.inflation(d); }, py::arg("distance"))
        .def("transformedShape", &Item::transformedShape)
        .def("resetTransformation", &Item::resetTransformation)
        .def("toString", &Item::toString)
        .def("__repr__", [](const Item& it) {
            return "<Item vertices=" + std::to_string(it.vertexCount()) +
                   " area=" + std::to_string(it.area()) + ">";
        });

    // ── nest() — main packing function ────────────────────────────────────
    m.def("nest",
        [](py::list py_items, const Box& bin, Coord distance, const NfpCfg& placer_cfg)
        -> std::size_t
        {
            // Copy items from Python list into a C++ vector.
            std::vector<Item> items;
            items.reserve(py::len(py_items));
            for (auto& obj : py_items)
                items.push_back(obj.cast<Item>());

            // Run the nester.
            NestConfig<NfpPlacer, FirstFitSelection> cfg(placer_cfg);
            auto num_bins = nest(items, bin, distance, cfg);

            // Write placement results back to the Python Item objects.
            for (std::size_t i = 0; i < items.size(); ++i) {
                auto& src = items[i];
                auto& dst = py_items[i].cast<Item&>();
                dst.translation(src.translation());
                dst.rotation(src.rotation());
                dst.binId(src.binId());
            }

            return num_bins;
        },
        py::arg("items"),
        py::arg("bin"),
        py::arg("distance") = Coord{0},
        py::arg("config")   = NfpCfg{},
        R"doc(
Arrange items in bins using NFP (No-Fit-Polygon) placement.

Args:
    items: List of Item instances to pack.
    bin: Box defining the bin dimensions.
    distance: Minimum spacing between items. Default 0.
    config: NfpConfig for placer options.

Returns:
    Number of bins required to fit all items.
)doc"
    );
}
