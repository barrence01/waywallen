use std::ffi::OsString;
use std::path::PathBuf;

/// `$XDG_STATE_HOME/waywallen/logs`, else `~/.local/state/waywallen/logs`.
///
/// Returns `None` when neither `XDG_STATE_HOME` nor `HOME` is set.
pub fn log_dir() -> Option<PathBuf> {
    log_dir_from(|key| std::env::var_os(key))
}

/// Testable variant of [`log_dir`].
pub fn log_dir_from(get: impl Fn(&str) -> Option<OsString>) -> Option<PathBuf> {
    if let Some(xdg) = get("XDG_STATE_HOME") {
        return Some(PathBuf::from(xdg).join("waywallen").join("logs"));
    }
    if let Some(home) = get("HOME") {
        return Some(
            PathBuf::from(home)
                .join(".local")
                .join("state")
                .join("waywallen")
                .join("logs"),
        );
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::OsString;

    #[test]
    fn prefers_xdg_state_home() {
        let dir = log_dir_from(|key| match key {
            "XDG_STATE_HOME" => Some(OsString::from("/tmp/state")),
            "HOME" => Some(OsString::from("/home/user")),
            _ => None,
        });
        assert_eq!(dir, Some(PathBuf::from("/tmp/state/waywallen/logs")));
    }

    #[test]
    fn falls_back_to_home_local_state() {
        let dir = log_dir_from(|key| match key {
            "HOME" => Some(OsString::from("/home/user")),
            _ => None,
        });
        assert_eq!(
            dir,
            Some(PathBuf::from("/home/user/.local/state/waywallen/logs"))
        );
    }

    #[test]
    fn returns_none_when_no_home_or_xdg() {
        let dir = log_dir_from(|_| None);
        assert_eq!(dir, None);
    }
}
