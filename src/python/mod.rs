//! PyO3 bindings for zoneout.

use datapod::{Geo, Point, Polygon};
use pyo3::exceptions::{PyRuntimeError, PyValueError};
use pyo3::prelude::*;

use crate::Zone as RsZone;

fn py_runtime_error(err: crate::Error) -> PyErr {
    PyRuntimeError::new_err(err.to_string())
}

fn polygon_from_tuples(points: Vec<(f64, f64, f64)>) -> PyResult<Polygon> {
    if points.len() < 3 {
        return Err(PyValueError::new_err(
            "zone boundary requires at least three points",
        ));
    }
    Ok(Polygon::new(
        points
            .into_iter()
            .map(|(x, y, z)| Point::new(x, y, z))
            .collect(),
    ))
}

#[pyclass(name = "Zone")]
pub struct PyZone {
    inner: RsZone,
}

#[pymethods]
impl PyZone {
    #[new]
    #[pyo3(signature = (name, kind, boundary, datum=(0.0, 0.0, 0.0), resolution=0.0))]
    fn new(
        name: &str,
        kind: &str,
        boundary: Vec<(f64, f64, f64)>,
        datum: (f64, f64, f64),
        resolution: f64,
    ) -> PyResult<Self> {
        let boundary = polygon_from_tuples(boundary)?;
        let datum = Geo::new(datum.0, datum.1, datum.2);
        let inner =
            RsZone::new(name, kind, boundary, datum, resolution).map_err(py_runtime_error)?;
        Ok(Self { inner })
    }

    #[getter]
    fn name(&self) -> &str {
        self.inner.name()
    }

    #[getter]
    fn kind(&self) -> &str {
        self.inner.kind()
    }

    fn area(&self) -> f64 {
        self.inner.poly().area()
    }

    fn perimeter(&self) -> f64 {
        self.inner.poly().perimeter()
    }

    fn child_count(&self) -> usize {
        self.inner.child_count()
    }

    fn contains(&self, point: (f64, f64, f64)) -> bool {
        self.inner.contains(Point::new(point.0, point.1, point.2))
    }

    fn set_property(&mut self, key: &str, value: &str) {
        self.inner.set_property(key, value);
    }

    fn get_property(&self, key: &str, default: &str) -> String {
        self.inner
            .property(key)
            .cloned()
            .unwrap_or_else(|| default.to_string())
    }

    fn add_child(&mut self, child: &PyZone) -> PyResult<()> {
        self.inner
            .add_child(child.inner.clone())
            .map_err(py_runtime_error)
    }
}

#[pyfunction]
fn version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

pub fn register_python_module(module: &Bound<'_, PyModule>) -> PyResult<()> {
    module.add("__version__", env!("CARGO_PKG_VERSION"))?;
    module.add_class::<PyZone>()?;
    module.add_function(wrap_pyfunction!(version, module)?)?;
    Ok(())
}

#[pymodule]
fn zoneout(module: &Bound<'_, PyModule>) -> PyResult<()> {
    register_python_module(module)
}
