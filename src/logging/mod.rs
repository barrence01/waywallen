//! Code-configured async file logging for the waywallen daemon.
//!
//! Tune behavior via [`LoggingPolicy::DEFAULT`]. Runtime filter still honors
//! `RUST_LOG` when set. Child-process stderr is forwarded through
//! [`forward_stderr`].

mod child_stdio;
mod paths;
mod policy;

pub use child_stdio::{forward_stderr, TARGET_DISPLAY, TARGET_RENDERER};
pub use paths::log_dir;
pub use policy::{filter_for_debug, LoggingPolicy, DEBUG_FILTER, DEFAULT, DEFAULT_FILTER};

use std::path::Path;
use std::sync::OnceLock;
use std::time::Duration;

use flexi_logger::{
    colored_opt_format, opt_format, Age, Cleanup, Criterion, Duplicate, FileSpec, Logger,
    LoggerHandle, Naming, WriteMode,
};

/// Global logger handle, initialized once per process.
///
/// [`LoggingGuard`] shuts down the writer on drop, but this `OnceLock` is never
/// reset — re-initializing logging in the same process is not supported.
static LOGGER_HANDLE: OnceLock<LoggerHandle> = OnceLock::new();

/// Keeps the flexi_logger async writer alive until process exit.
///
/// On drop, shuts down the underlying writer. The global [`LOGGER_HANDLE`] entry
/// is not cleared; a second init in the same process is not supported.
pub struct LoggingGuard {
    handle: Option<LoggerHandle>,
}

impl Drop for LoggingGuard {
    fn drop(&mut self) {
        if let Some(handle) = self.handle.take() {
            handle.shutdown();
        }
    }
}

/// Whether the daemon environment sets `RUST_LOG`, overriding the debug setting.
pub fn rust_log_active() -> bool {
    std::env::var_os("RUST_LOG").is_some()
}

/// Initialize logging under the default XDG state log directory.
pub fn init(policy: LoggingPolicy) -> LoggingGuard {
    match log_dir() {
        Some(dir) => init_in(policy, &dir),
        None => {
            eprintln!("waywallen: XDG_STATE_HOME and HOME unset; using stderr-only logging");
            init_stderr_only(policy)
        }
    }
}

/// Initialize logging under an explicit directory (tests / overrides).
pub fn init_in(policy: LoggingPolicy, dir: &Path) -> LoggingGuard {
    if let Err(error) = std::fs::create_dir_all(dir) {
        eprintln!(
            "waywallen: cannot create log directory {}: {error}; falling back to stderr-only",
            dir.display()
        );
        return init_stderr_only(policy);
    }

    match try_init_file(policy, dir) {
        Ok(handle) => {
            log::info!(
                "logging: writing to {} (keep at most {} rotated log file(s))",
                dir.display(),
                policy.retention_days
            );
            LoggingGuard {
                handle: Some(register_handle(handle)),
            }
        }
        Err(error) => {
            eprintln!("waywallen: file logging unavailable ({error}); falling back to stderr-only");
            init_stderr_only(policy)
        }
    }
}

pub fn apply_debug_setting(enabled: bool) {
    if rust_log_active() {
        log::info!("debug_logging_enabled ignored because RUST_LOG is set");
        return;
    }
    let Some(handle) = LOGGER_HANDLE.get() else {
        return;
    };
    let filter = filter_for_debug(enabled);
    match handle.parse_new_spec(filter) {
        Ok(()) => log::info!("log filter set to {filter}"),
        Err(error) => log::warn!("failed to apply log filter {filter}: {error}"),
    }
}

fn register_handle(handle: LoggerHandle) -> LoggerHandle {
    let _ = LOGGER_HANDLE.set(handle.clone());
    handle
}

fn try_init_file(
    policy: LoggingPolicy,
    dir: &Path,
) -> Result<LoggerHandle, flexi_logger::FlexiLoggerError> {
    let mut logger = Logger::try_with_env_or_str(policy.default_filter)?
        .log_to_file(
            FileSpec::default()
                .directory(dir)
                .basename(policy.file_prefix)
                .suppress_timestamp(),
        )
        .format_for_files(opt_format)
        .append()
        .rotate(
            Criterion::Age(Age::Day),
            Naming::TimestampsCustomFormat {
                current_infix: None,
                format: "r%Y-%m-%d",
            },
            Cleanup::KeepLogFiles(usize::from(policy.retention_days)),
        )
        .write_mode(WriteMode::AsyncWith {
            pool_capa: policy.async_channel_size.max(32),
            message_capa: 1024,
            flush_interval: Duration::from_secs(2),
        });

    if policy.also_stderr {
        logger = logger
            .duplicate_to_stderr(Duplicate::All)
            .format_for_stderr(colored_opt_format);
    }

    logger.start()
}

fn init_stderr_only(policy: LoggingPolicy) -> LoggingGuard {
    match Logger::try_with_env_or_str(policy.default_filter).and_then(|logger| {
        logger
            .log_to_stderr()
            .format_for_stderr(colored_opt_format)
            .start()
    }) {
        Ok(handle) => LoggingGuard {
            handle: Some(register_handle(handle)),
        },
        Err(error) => {
            eprintln!("waywallen: stderr logging init failed: {error}");
            LoggingGuard { handle: None }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::sync::Mutex;

    static INIT_LOCK: Mutex<()> = Mutex::new(());

    fn test_lock() -> std::sync::MutexGuard<'static, ()> {
        INIT_LOCK
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner)
    }

    #[test]
    fn init_writes_daily_log_file() {
        let _guard = test_lock();
        let dir = tempfile::tempdir().unwrap();
        let logging = init_in(LoggingPolicy::DEFAULT, dir.path());
        log::info!(target: "waywallen::logging_test", "smoke-test-line");
        drop(logging);

        let mut found = false;
        for entry in fs::read_dir(dir.path()).unwrap() {
            let path = entry.unwrap().path();
            let name = path.file_name().and_then(|n| n.to_str()).unwrap_or("");
            if name.starts_with("waywallen_r") && name.ends_with(".log") {
                let contents = fs::read_to_string(&path).unwrap();
                if contents.contains("smoke-test-line") {
                    found = true;
                    break;
                }
            }
        }
        assert!(found, "expected smoke-test-line in a waywallen_r*.log file");

        // opt_format prefixes each line with [YYYY-MM-DD HH:MM:SS...]
        let mut timestamped = false;
        for entry in fs::read_dir(dir.path()).unwrap() {
            let path = entry.unwrap().path();
            let name = path.file_name().and_then(|n| n.to_str()).unwrap_or("");
            if !(name.starts_with("waywallen_r") && name.ends_with(".log")) {
                continue;
            }
            let contents = fs::read_to_string(&path).unwrap();
            if contents
                .lines()
                .any(|line| line.starts_with('[') && line.contains("smoke-test-line"))
            {
                timestamped = true;
                break;
            }
        }
        assert!(
            timestamped,
            "expected opt_format timestamp prefix on smoke-test-line"
        );
    }

    #[test]
    fn rust_log_active_reflects_environment() {
        let _guard = test_lock();
        std::env::remove_var("RUST_LOG");
        assert!(!rust_log_active());

        std::env::set_var("RUST_LOG", "info");
        assert!(rust_log_active());

        std::env::remove_var("RUST_LOG");
        assert!(!rust_log_active());
    }

    #[test]
    fn rust_log_is_temporary_override_of_persisted_debug_preference() {
        let _guard = test_lock();

        // Persisted user preference stays enabled regardless of RUST_LOG.
        let debug_logging_enabled = true;

        std::env::set_var("RUST_LOG", "info");
        assert!(rust_log_active());
        assert_eq!(filter_for_debug(debug_logging_enabled), DEBUG_FILTER);

        std::env::remove_var("RUST_LOG");
        assert!(!rust_log_active());
        assert_eq!(filter_for_debug(debug_logging_enabled), DEBUG_FILTER);
    }
}
