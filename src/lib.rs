//! zoneout — hierarchical agricultural zone / workspace library.
//!
//! See `PLAN.md` at the crate root for the design overview. The public
//! surface is kept flat at `zoneout::*` via re-exports below.

pub mod core;
pub mod utils;

pub mod grid;
pub mod io;
pub mod plot;
pub mod poly;
pub mod polygrid;
pub mod wire;
pub mod workspace;
pub mod zone;

pub use crate::core::error::{Error, Result};
pub use crate::core::meta::Meta;
pub use crate::utils::time::{LamportClock, Timestamp};

pub use crate::grid::Grid;
pub use crate::plot::{Plot, PlotBuilder};
pub use crate::poly::{LineElement, PointElement, Poly, PolygonElement, StructuredElement};
pub use crate::polygrid::{load_poly_grid, make_base_grid, save_poly_grid};
pub use crate::wire::{
    CoordMode, EdgeJson, JsonGeo, JsonPoint, NodeJson, WorkspaceJson, ZoneJson,
    infer_datum, infer_root_zone_id, require_valid_workspace_json, to_json_point,
    to_json_polygon, to_local_point, to_local_polygon, valid_latlon, valid_local_xy,
    validate_workspace_json,
};
pub use crate::workspace::{EdgeData, NodeData, NodePosition, Workspace};
pub use crate::zone::{Zone, ZoneBuilder, make_zone};
