use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::process::ChildStderr;

pub const TARGET_RENDERER: &str = "renderer";

pub const TARGET_DISPLAY: &str = "display";

const CHILD_LOG_TARGET: &str = "waywallen::child";

/// Format a non-empty child stderr line for the daemon log.
pub(crate) fn format_child_stderr_line(role: &str, line: &str) -> Option<String> {
    if line.trim().is_empty() {
        None
    } else {
        Some(format!("[{role}] {line}"))
    }
}

/// Pipe child stderr into the daemon logger without blocking the child.
pub fn forward_stderr(stderr: ChildStderr, role: &'static str) {
    tokio::spawn(async move {
        let mut lines = BufReader::new(stderr).lines();
        loop {
            match lines.next_line().await {
                Ok(Some(line)) => {
                    if let Some(message) = format_child_stderr_line(role, &line) {
                        log::debug!(target: CHILD_LOG_TARGET, "{message}");
                    }
                }
                Ok(None) => break,
                Err(error) => {
                    log::warn!(
                        target: CHILD_LOG_TARGET,
                        "Failed to read child stderr ({role}): {error}"
                    );
                    break;
                }
            }
        }
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn format_child_stderr_line_prefixes_role() {
        assert_eq!(
            format_child_stderr_line("renderer", "failed to initialize"),
            Some("[renderer] failed to initialize".to_string())
        );
    }

    #[test]
    fn format_child_stderr_line_ignores_empty_and_whitespace() {
        assert_eq!(format_child_stderr_line("renderer", ""), None);
        assert_eq!(format_child_stderr_line("display", "   "), None);
        assert_eq!(format_child_stderr_line("display", "\n"), None);
    }
}
