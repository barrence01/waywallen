//! The shell renders daemon-owned tray labels, so `build.rs` extracts the UI's
//! `TrayMenu` translations into a Rust catalog to avoid a Qt runtime dependency.

use std::sync::{OnceLock, RwLock};

/// One language's messages as `(msgid, msgstr)` pairs, sorted by `msgid`.
type Catalog = &'static [(&'static str, &'static str)];

include!(concat!(env!("OUT_DIR"), "/tray_i18n_generated.rs"));

fn normalize_language_tag(language: &str) -> String {
    language
        .split(['.', '@'])
        .next()
        .unwrap_or_default()
        .replace('-', "_")
        .to_ascii_lowercase()
}

fn catalog_for(language: &str) -> Option<Catalog> {
    let mut normalized = normalize_language_tag(language);
    if normalized.is_empty() {
        return None;
    }

    // Qt shortens the default Simplified Chinese locale to `zh` in BCP 47 tags.
    if normalized == "zh" {
        normalized = "zh_cn".to_string();
    }

    loop {
        if let Some(catalog) = GENERATED_CATALOGS
            .iter()
            .find_map(|&(tag, catalog)| (tag == normalized.as_str()).then_some(catalog))
        {
            return Some(catalog);
        }
        let end = normalized.rfind('_')?;
        normalized.truncate(end);
    }
}

fn locale_language() -> Option<String> {
    for key in ["LC_ALL", "LC_MESSAGES", "LANG"] {
        let value = match std::env::var(key) {
            Ok(value) => value,
            Err(_) => continue,
        };
        if value.is_empty() {
            continue;
        }

        let base = value.split(['.', '@']).next().unwrap_or_default();
        if base.eq_ignore_ascii_case("C") || base.eq_ignore_ascii_case("POSIX") {
            return None;
        }

        let normalized = normalize_language_tag(&value);
        if !normalized.is_empty() {
            return Some(normalized);
        }
    }
    None
}

fn language_override() -> &'static RwLock<Option<String>> {
    static LANGUAGE: OnceLock<RwLock<Option<String>>> = OnceLock::new();
    LANGUAGE.get_or_init(|| RwLock::new(None))
}

/// `system` restores the process locale. Returns whether the selection changed.
pub fn set_language(language: &str) -> bool {
    let normalized = normalize_language_tag(language);
    let next = if language.eq_ignore_ascii_case("system") || normalized.is_empty() {
        None
    } else {
        Some(normalized)
    };

    let mut selected = language_override().write().unwrap();
    if *selected == next {
        return false;
    }
    *selected = next;
    true
}

fn active_catalog() -> Option<Catalog> {
    let selected = language_override().read().unwrap().clone();
    match selected {
        Some(language) => catalog_for(&language),
        None => locale_language().as_deref().and_then(catalog_for),
    }
}

/// Translate a tray label, falling back to the English source string.
pub fn tr(msgid: &'static str) -> &'static str {
    let Some(catalog) = active_catalog() else {
        return msgid;
    };
    match catalog.binary_search_by_key(&msgid, |(id, _)| *id) {
        Ok(index) => catalog[index].1,
        Err(_) => msgid,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn generated_catalogs_are_sorted() {
        for &(locale, catalog) in GENERATED_CATALOGS {
            assert!(
                catalog.windows(2).all(|w| w[0].0 < w[1].0),
                "{locale} catalog entries must be sorted by msgid for binary_search"
            );
            assert!(catalog.iter().all(|(_, text)| !text.is_empty()));
        }
    }

    #[test]
    fn catalog_selection_accepts_bcp47_and_locale_tags() {
        assert!(catalog_for("de-DE").is_some());
        assert!(catalog_for("ru_RU.UTF-8").is_some());
        assert!(catalog_for("zh-CN").is_some());
        assert!(catalog_for("en-US").is_none());
    }

    #[test]
    fn catalog_selection_does_not_fall_back_to_another_region() {
        assert_eq!(catalog_for("zh"), catalog_for("zh-CN"));
        assert!(catalog_for("zh").is_some());
        for language in ["zh-TW", "zh-HK", "zh-SG", "zh-Hant-TW"] {
            assert!(catalog_for(language).is_none(), "{language}");
        }
        assert_eq!(catalog_for("de-AT"), catalog_for("de"));
        assert_eq!(catalog_for("ru-BY"), catalog_for("ru"));
    }

    #[test]
    fn missing_message_falls_back_to_source() {
        let ru = catalog_for("ru").unwrap();
        assert!(ru
            .binary_search_by_key(&"not a tray label", |(id, _)| *id)
            .is_err());
    }
}
