use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::path::{Component, Path, PathBuf};

const MAX_TRANSLATION_FILE_BYTES: u64 = 1024 * 1024;
const MAX_PLUGIN_TRANSLATION_BYTES: u64 = 4 * 1024 * 1024;
const MAX_TRANSLATION_SNAPSHOT_BYTES: usize = 16 * 1024 * 1024;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PluginMessage {
    pub plugin_id: String,
    pub msgid: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum LocalizedText {
    Raw(String),
    Message(PluginMessage),
}

impl Default for LocalizedText {
    fn default() -> Self {
        Self::Raw(String::new())
    }
}

impl LocalizedText {
    pub fn raw(value: impl Into<String>) -> Self {
        Self::Raw(value.into())
    }

    pub fn translated(plugin_id: impl Into<String>, msgid: impl Into<String>) -> Self {
        Self::Message(PluginMessage {
            plugin_id: plugin_id.into(),
            msgid: msgid.into(),
        })
    }

    pub fn is_translated(&self) -> bool {
        matches!(self, Self::Message(_))
    }

    pub fn text(&self) -> &str {
        match self {
            Self::Raw(value) => value,
            Self::Message(value) => &value.msgid,
        }
    }

    pub fn message(&self) -> Option<&PluginMessage> {
        match self {
            Self::Raw(_) => None,
            Self::Message(value) => Some(value),
        }
    }
}

impl From<String> for LocalizedText {
    fn from(value: String) -> Self {
        Self::raw(value)
    }
}

impl From<&str> for LocalizedText {
    fn from(value: &str) -> Self {
        Self::raw(value)
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct LocalizedTextDef {
    pub msgid: String,
}

impl LocalizedTextDef {
    pub fn resolve(&self, plugin_id: &str) -> LocalizedText {
        LocalizedText::translated(plugin_id, self.msgid.clone())
    }

    pub fn is_valid(&self) -> bool {
        !self.msgid.is_empty()
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct PluginTranslationMeta {
    pub directory: PathBuf,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PluginTranslationDocument {
    pub plugin_id: String,
    pub locale: String,
    pub po: Vec<u8>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct PluginTranslationSnapshot {
    pub generation: u64,
    pub documents: Vec<PluginTranslationDocument>,
}

pub fn load_plugin_translation_documents(
    plugin_dir: &Path,
    plugin_id: &str,
    metadata: &PluginTranslationMeta,
    files: &[String],
) -> Result<Vec<PluginTranslationDocument>, String> {
    if !is_safe_relative_path(&metadata.directory) || metadata.directory.as_os_str().is_empty() {
        return Err(format!(
            "plugin.i18n.directory must be a safe relative path, got {}",
            metadata.directory.display()
        ));
    }

    let plugin_root = plugin_dir
        .canonicalize()
        .map_err(|error| format!("canonicalize plugin root: {error}"))?;
    let mut documents = Vec::new();
    let mut total = 0_u64;
    let mut seen = HashSet::new();
    for file in files {
        let relative = Path::new(file);
        if relative.parent() != Some(metadata.directory.as_path()) {
            continue;
        }
        if !is_safe_relative_path(relative)
            || relative.extension().and_then(|value| value.to_str()) != Some("po")
        {
            return Err(format!("invalid translation path in files.txt: {file}"));
        }
        let locale = relative
            .file_stem()
            .and_then(|value| value.to_str())
            .ok_or_else(|| format!("translation filename is not UTF-8: {file}"))?;
        if !is_canonical_locale_tag(locale) {
            return Err(format!(
                "translation filename is not canonical BCP 47: {file}"
            ));
        }
        if !seen.insert(locale.to_string()) {
            return Err(format!("duplicate translation locale {locale}"));
        }

        let path = plugin_dir.join(relative);
        let resolved = path
            .canonicalize()
            .map_err(|error| format!("read declared translation {}: {error}", path.display()))?;
        if !resolved.starts_with(&plugin_root) {
            return Err(format!("translation escapes plugin root: {file}"));
        }
        let size = resolved
            .metadata()
            .map_err(|error| format!("inspect translation {}: {error}", resolved.display()))?
            .len();
        if size > MAX_TRANSLATION_FILE_BYTES {
            return Err(format!(
                "translation {file} is {size} bytes, limit is {MAX_TRANSLATION_FILE_BYTES}"
            ));
        }
        total = total.saturating_add(size);
        if total > MAX_PLUGIN_TRANSLATION_BYTES {
            return Err(format!(
                "plugin translations are {total} bytes, limit is {MAX_PLUGIN_TRANSLATION_BYTES}"
            ));
        }
        let po = std::fs::read(&resolved)
            .map_err(|error| format!("read translation {}: {error}", resolved.display()))?;
        documents.push(PluginTranslationDocument {
            plugin_id: plugin_id.to_string(),
            locale: locale.to_string(),
            po,
        });
    }
    Ok(documents)
}

pub fn prepare_plugin_translation_documents(
    documents: impl Iterator<Item = PluginTranslationDocument>,
) -> Result<Vec<PluginTranslationDocument>, String> {
    let mut documents: Vec<_> = documents.collect();
    documents.sort_by(|a, b| a.plugin_id.cmp(&b.plugin_id).then(a.locale.cmp(&b.locale)));
    let total: usize = documents.iter().map(|document| document.po.len()).sum();
    if total > MAX_TRANSLATION_SNAPSHOT_BYTES {
        return Err(format!(
            "plugin translation snapshot is {total} bytes, limit is {MAX_TRANSLATION_SNAPSHOT_BYTES}"
        ));
    }
    Ok(documents)
}

fn is_safe_relative_path(path: &Path) -> bool {
    path.is_relative()
        && path
            .components()
            .all(|component| matches!(component, Component::Normal(_)))
}

fn is_canonical_locale_tag(locale: &str) -> bool {
    let mut parts = locale.split('-');
    let Some(language) = parts.next() else {
        return false;
    };
    if !(2..=8).contains(&language.len())
        || !language.bytes().all(|value| value.is_ascii_lowercase())
    {
        return false;
    }
    parts.all(|part| {
        !part.is_empty()
            && part.bytes().all(|value| value.is_ascii_alphanumeric())
            && ((part.len() == 4
                && part.as_bytes()[0].is_ascii_uppercase()
                && part.as_bytes()[1..]
                    .iter()
                    .all(|value| value.is_ascii_lowercase()))
                || (part.len() == 2 && part.bytes().all(|value| value.is_ascii_uppercase()))
                || (part.len() == 3 && part.bytes().all(|value| value.is_ascii_digit()))
                || (4..=8).contains(&part.len()))
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn document(plugin_id: &str, locale: &str, po: &[u8]) -> PluginTranslationDocument {
        PluginTranslationDocument {
            plugin_id: plugin_id.to_string(),
            locale: locale.to_string(),
            po: po.to_vec(),
        }
    }

    #[test]
    fn translation_snapshot_is_sorted_without_rewriting_po_bytes() {
        let documents = prepare_plugin_translation_documents(
            [
                document("org.test.second", "zh-CN", b"second"),
                document("org.test.first", "zh-CN", b"first-zh"),
                document("org.test.first", "ru", b"first-ru"),
            ]
            .into_iter(),
        )
        .unwrap();
        assert_eq!(documents[0], document("org.test.first", "ru", b"first-ru"));
        assert_eq!(
            documents[1],
            document("org.test.first", "zh-CN", b"first-zh")
        );
        assert_eq!(
            documents[2],
            document("org.test.second", "zh-CN", b"second")
        );
    }

    #[test]
    fn translation_loader_rejects_duplicate_locale_and_oversized_file() {
        let directory = tempfile::tempdir().unwrap();
        let i18n = directory.path().join("i18n");
        std::fs::create_dir(&i18n).unwrap();
        std::fs::write(i18n.join("ru.po"), b"ru").unwrap();
        let metadata = PluginTranslationMeta {
            directory: PathBuf::from("i18n"),
        };
        let duplicate = load_plugin_translation_documents(
            directory.path(),
            "org.test",
            &metadata,
            &["i18n/ru.po".into(), "i18n/ru.po".into()],
        )
        .unwrap_err();
        assert!(duplicate.contains("duplicate translation locale ru"));

        std::fs::write(
            i18n.join("zh-CN.po"),
            vec![0_u8; MAX_TRANSLATION_FILE_BYTES as usize + 1],
        )
        .unwrap();
        let oversized = load_plugin_translation_documents(
            directory.path(),
            "org.test",
            &metadata,
            &["i18n/zh-CN.po".into()],
        )
        .unwrap_err();
        assert!(oversized.contains("limit"));
    }
}
