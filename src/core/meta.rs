use serde::{Deserialize, Serialize};
use uuid::Uuid;

/// Shared identity + classification block. Mirrors the C++ `Meta` struct
/// (`id, name, type, subtype`) used by `Zone`, `Plot`, `Poly`, `Grid`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Meta {
    pub id: Uuid,
    pub name: String,
    #[serde(rename = "type")]
    pub kind: String,
    #[serde(default)]
    pub subtype: String,
}

impl Default for Meta {
    fn default() -> Self {
        Self {
            id: Uuid::new_v4(),
            name: String::new(),
            kind: String::new(),
            subtype: String::new(),
        }
    }
}

impl Meta {
    pub fn new(name: impl Into<String>, kind: impl Into<String>) -> Self {
        Self {
            id: Uuid::new_v4(),
            name: name.into(),
            kind: kind.into(),
            subtype: String::new(),
        }
    }

    pub fn with_subtype(mut self, subtype: impl Into<String>) -> Self {
        self.subtype = subtype.into();
        self
    }

    pub fn with_id(mut self, id: Uuid) -> Self {
        self.id = id;
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn meta_serialises_type_as_type() {
        let m = Meta::new("alpha", "field").with_subtype("crop");
        let v = serde_json::to_value(&m).unwrap();
        assert_eq!(v["name"], "alpha");
        assert_eq!(v["type"], "field");
        assert_eq!(v["subtype"], "crop");
    }

    #[test]
    fn meta_default_has_fresh_id() {
        let a = Meta::default();
        let b = Meta::default();
        assert_ne!(a.id, b.id);
    }
}
