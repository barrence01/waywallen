//! Daemon logging with reloadable filtering and daily files.

mod paths;
mod policy;

pub use paths::log_dir;
pub use policy::{filter_for_debug, LoggingPolicy, DEBUG_FILTER, DEFAULT, DEFAULT_FILTER};

use std::io::IsTerminal;
use std::path::Path;
use std::sync::OnceLock;

use tracing_appender::non_blocking::{ErrorCounter, NonBlocking, NonBlockingBuilder, WorkerGuard};
use tracing_appender::rolling::{RollingFileAppender, Rotation};
use tracing_subscriber::filter::EnvFilter;
use tracing_subscriber::layer::SubscriberExt;
use tracing_subscriber::reload;
use tracing_subscriber::util::SubscriberInitExt;
use tracing_subscriber::Registry;

type FilterHandle = reload::Handle<EnvFilter, Registry>;

static FILTER_HANDLE: OnceLock<FilterHandle> = OnceLock::new();
const DAILY_LOG_DIR: &str = "daemon";
const LOG_ENV: &str = "WW_LOG";

pub struct LoggingGuard {
    worker: Option<WorkerGuard>,
    error_counter: Option<ErrorCounter>,
}

impl Drop for LoggingGuard {
    fn drop(&mut self) {
        drop(self.worker.take());
        if let Some(counter) = &self.error_counter {
            let dropped = counter.dropped_lines();
            if dropped != 0 {
                eprintln!("waywallen: file logger dropped {dropped} line(s)");
            }
        }
    }
}

pub fn ww_log_active() -> bool {
    std::env::var_os(LOG_ENV).is_some()
}

pub fn ww_log_level() -> Option<log::LevelFilter> {
    std::env::var_os(LOG_ENV)
        .as_deref()
        .and_then(renderer_log_level)
}

pub fn init(policy: LoggingPolicy) -> LoggingGuard {
    match log_dir() {
        Some(dir) => init_in(policy, &dir),
        None => {
            eprintln!("waywallen: XDG_STATE_HOME and HOME unset; using stderr-only logging");
            install(policy, None)
        }
    }
}

pub fn init_in(policy: LoggingPolicy, dir: &Path) -> LoggingGuard {
    if let Err(error) = std::fs::create_dir_all(dir) {
        eprintln!(
            "waywallen: cannot create log directory {}: {error}; falling back to stderr-only",
            dir.display()
        );
        return install(policy, None);
    }

    let daily_dir = dir.join(DAILY_LOG_DIR);
    let file = match create_file_writer(policy, &daily_dir) {
        Ok(file) => Some(file),
        Err(error) => {
            eprintln!("waywallen: file logging unavailable ({error}); falling back to stderr-only");
            None
        }
    };
    let file_available = file.is_some();
    let guard = install(policy, file);
    if file_available {
        log::info!(
            "logging: writing to {} (keep at most {} daily log file(s))",
            daily_dir.display(),
            policy.max_log_files
        );
    }
    guard
}

pub fn init_stderr(default_filter: &str) {
    let filter = filter_from_environment(default_filter);
    let ansi = std::io::stderr().is_terminal();
    if let Err(error) = tracing_subscriber::fmt()
        .with_env_filter(filter)
        .with_ansi(ansi)
        .try_init()
    {
        eprintln!("waywallen: stderr logging init failed: {error}");
    }
}

pub fn apply_debug_setting(enabled: bool) {
    if ww_log_active() {
        log::info!("debug_logging_enabled ignored because WW_LOG is set");
        return;
    }
    let Some(handle) = FILTER_HANDLE.get() else {
        return;
    };
    let filter = filter_for_debug(enabled);
    let parsed = match EnvFilter::try_new(filter) {
        Ok(parsed) => parsed,
        Err(error) => {
            log::warn!("failed to parse log filter {filter}: {error}");
            return;
        }
    };
    match handle.reload(parsed) {
        Ok(()) => log::info!("log filter set to {filter}"),
        Err(error) => log::warn!("failed to apply log filter {filter}: {error}"),
    }
}

struct FileWriter {
    writer: NonBlocking,
    guard: WorkerGuard,
    error_counter: ErrorCounter,
}

fn create_file_writer(policy: LoggingPolicy, dir: &Path) -> Result<FileWriter, String> {
    std::fs::create_dir_all(dir)
        .map_err(|error| format!("cannot create {}: {error}", dir.display()))?;
    let latest = format!("{}-current.log", policy.file_prefix);
    let appender = match build_appender(policy, dir, Some(&latest)) {
        Ok(appender) => appender,
        Err(first_error) => {
            eprintln!(
                "waywallen: rolling log latest link unavailable ({first_error}); retrying without it"
            );
            build_appender(policy, dir, None).map_err(|second_error| {
                format!("with latest link: {first_error}; without latest link: {second_error}")
            })?
        }
    };

    let (writer, guard) = NonBlockingBuilder::default()
        .buffered_lines_limit(policy.async_channel_size.max(32))
        .lossy(true)
        .finish(appender);
    let error_counter = writer.error_counter();
    Ok(FileWriter {
        writer,
        guard,
        error_counter,
    })
}

fn build_appender(
    policy: LoggingPolicy,
    dir: &Path,
    latest: Option<&str>,
) -> Result<RollingFileAppender, tracing_appender::rolling::InitError> {
    let mut builder = RollingFileAppender::builder()
        .rotation(Rotation::DAILY)
        .filename_prefix(policy.file_prefix)
        .filename_suffix("log")
        .max_log_files(policy.max_log_files);
    if let Some(latest) = latest {
        builder = builder.latest_symlink(latest);
    }
    builder.build(dir)
}

fn install(policy: LoggingPolicy, file: Option<FileWriter>) -> LoggingGuard {
    let filter = filter_from_environment(policy.default_filter);
    let (filter_layer, handle) = reload::Layer::new(filter);
    let file_layer = file.as_ref().map(|file| {
        tracing_subscriber::fmt::layer()
            .with_ansi(false)
            .with_target(true)
            .with_writer(file.writer.clone())
    });
    let stderr_layer = policy.also_stderr.then(|| {
        tracing_subscriber::fmt::layer()
            .with_ansi(std::io::stderr().is_terminal())
            .with_target(true)
            .with_writer(std::io::stderr)
    });

    let subscriber = tracing_subscriber::registry()
        .with(filter_layer)
        .with(file_layer)
        .with(stderr_layer);
    if let Err(error) = subscriber.try_init() {
        eprintln!("waywallen: logging init failed: {error}");
        return LoggingGuard {
            worker: None,
            error_counter: None,
        };
    }
    if FILTER_HANDLE.set(handle).is_err() {
        eprintln!("waywallen: logging filter handle was already initialized");
    }

    let (worker, error_counter) = match file {
        Some(file) => (Some(file.guard), Some(file.error_counter)),
        None => (None, None),
    };
    LoggingGuard {
        worker,
        error_counter,
    }
}

fn filter_from_environment(default_filter: &str) -> EnvFilter {
    let raw = std::env::var_os(LOG_ENV);
    filter_from_value(default_filter, raw.as_deref())
}

fn filter_from_value(default_filter: &str, raw: Option<&std::ffi::OsStr>) -> EnvFilter {
    let Some(raw) = raw else {
        return EnvFilter::try_new(default_filter).unwrap_or_else(|error| {
            panic!("invalid built-in log filter {default_filter}: {error}")
        });
    };
    match raw.to_str().and_then(|raw| EnvFilter::try_new(raw).ok()) {
        Some(filter) => force_zbus_warn(filter),
        None => {
            eprintln!(
                "waywallen: invalid WW_LOG filter {:?}; using {default_filter}",
                raw
            );
            EnvFilter::try_new(default_filter).unwrap_or_else(|error| {
                panic!("invalid built-in log filter {default_filter}: {error}")
            })
        }
    }
}

fn force_zbus_warn(filter: EnvFilter) -> EnvFilter {
    filter.add_directive("zbus=warn".parse().expect("valid zbus log directive"))
}

fn renderer_log_level(raw: &std::ffi::OsStr) -> Option<log::LevelFilter> {
    let level = raw.to_str()?.split(',').next()?.trim();
    match level.to_ascii_lowercase().as_str() {
        "off" => Some(log::LevelFilter::Off),
        "error" => Some(log::LevelFilter::Error),
        "warn" => Some(log::LevelFilter::Warn),
        "info" => Some(log::LevelFilter::Info),
        "debug" => Some(log::LevelFilter::Debug),
        "trace" => Some(log::LevelFilter::Trace),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    #[test]
    fn rolling_writer_preserves_legacy_files() {
        let dir = tempfile::tempdir().unwrap();
        let legacy = dir.path().join("waywallen_r2026-08-26.log");
        let display = dir.path().join("waywallen_display_r2026-08-26.log");
        std::fs::write(&legacy, "daemon\n").unwrap();
        std::fs::write(&display, "display\n").unwrap();

        let daily_dir = dir.path().join(DAILY_LOG_DIR);
        let mut file = create_file_writer(LoggingPolicy::DEFAULT, &daily_dir).unwrap();
        writeln!(file.writer, "post-init-line").unwrap();
        drop(file.writer);
        drop(file.guard);

        let mut restarted = create_file_writer(LoggingPolicy::DEFAULT, &daily_dir).unwrap();
        writeln!(restarted.writer, "post-restart-line").unwrap();
        drop(restarted.writer);
        drop(restarted.guard);

        assert_eq!(std::fs::read_to_string(&legacy).unwrap(), "daemon\n");
        assert_eq!(std::fs::read_to_string(&display).unwrap(), "display\n");
        let current = std::fs::read_to_string(daily_dir.join("waywallen-current.log")).unwrap();
        assert!(current.contains("post-init-line"));
        assert!(current.contains("post-restart-line"));
    }

    #[test]
    fn invalid_environment_filter_falls_back_to_default() {
        let filter = filter_from_value(DEFAULT_FILTER, Some(std::ffi::OsStr::new("[")));
        assert_eq!(
            filter.to_string(),
            EnvFilter::try_new(DEFAULT_FILTER).unwrap().to_string()
        );
    }

    #[test]
    fn environment_filter_supports_targets_and_forces_zbus_warn() {
        let filter = filter_from_value(
            DEFAULT_FILTER,
            Some(std::ffi::OsStr::new(
                "debug,waywallen::probe=trace,zbus=trace",
            )),
        );
        let filter = filter.to_string();
        assert!(filter.contains("debug"));
        assert!(filter.contains("waywallen::probe=trace"));
        assert!(filter.contains("zbus=warn"));
        assert!(!filter.contains("zbus=trace"));
    }

    #[test]
    fn renderer_level_uses_leading_global_directive() {
        assert_eq!(
            renderer_log_level(std::ffi::OsStr::new("DEBUG,zbus=warn")),
            Some(log::LevelFilter::Debug)
        );
        assert_eq!(
            renderer_log_level(std::ffi::OsStr::new("waywallen::probe=trace")),
            None
        );
        assert_eq!(renderer_log_level(std::ffi::OsStr::new("verbose")), None);
    }
}
