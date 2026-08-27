use std::fs;
use std::path::Path;
use std::time::Duration;

use flexi_logger::{Age, Cleanup, Criterion, Duplicate, FileSpec, Logger, Naming, WriteMode};
use waywallen::logging::{LoggingPolicy, DEFAULT_FILTER};

fn count_rotated_logs(dir: &Path) -> usize {
    fs::read_dir(dir)
        .unwrap()
        .flatten()
        .filter(|entry| {
            entry
                .path()
                .file_name()
                .and_then(|name| name.to_str())
                .is_some_and(|name| {
                    name.starts_with("waywallen_r")
                        && name.ends_with(".log")
                        && name.contains("_00-00-00")
                })
        })
        .count()
}

#[test]
fn keep_log_files_retains_at_most_n_newest_on_rotation() {
    let dir = tempfile::tempdir().unwrap();
    let keep = 3_u8;
    let policy = LoggingPolicy {
        retention_days: keep,
        also_stderr: false,
        ..LoggingPolicy::DEFAULT
    };

    let mut logger = Logger::try_with_str(DEFAULT_FILTER)
        .unwrap()
        .log_to_file(
            FileSpec::default()
                .directory(dir.path())
                .basename(policy.file_prefix)
                .suppress_timestamp(),
        )
        .append()
        .rotate(
            Criterion::Age(Age::Second),
            Naming::TimestampsCustomFormat {
                current_infix: Some("rCURRENT"),
                format: "r%Y-%m-%d_00-00-00",
            },
            Cleanup::KeepLogFiles(usize::from(keep)),
        )
        .write_mode(WriteMode::AsyncWith {
            pool_capa: 32,
            message_capa: 1024,
            flush_interval: Duration::from_secs(2),
        });

    if policy.also_stderr {
        logger = logger.duplicate_to_stderr(Duplicate::All);
    }

    let handle = logger.start().expect("logger init");

    for i in 0..5 {
        log::info!(target: "waywallen::logging_test", "rotation-trigger-{i}");
        std::thread::sleep(Duration::from_secs(2));
    }
    handle.shutdown();

    let remaining = count_rotated_logs(dir.path());
    assert!(
        remaining <= usize::from(keep),
        "flexi_logger KeepLogFiles({keep}) should leave at most {keep} rotated logs, found {remaining}"
    );
}
