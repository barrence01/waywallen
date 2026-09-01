/// Tunable logging behavior. Prefer editing [`LoggingPolicy::DEFAULT`].
#[derive(Debug, Clone, Copy)]
pub struct LoggingPolicy {
    pub max_log_files: usize,
    pub file_prefix: &'static str,
    pub default_filter: &'static str,
    pub also_stderr: bool,
    pub async_channel_size: usize,
}

pub const DEFAULT_FILTER: &str = "info,zbus=warn,selectors::matching=warn,html5ever=warn";
pub const DEBUG_FILTER: &str = "debug,zbus=warn,selectors::matching=warn,html5ever=warn";

pub fn filter_for_debug(enabled: bool) -> &'static str {
    if enabled {
        DEBUG_FILTER
    } else {
        DEFAULT_FILTER
    }
}

impl LoggingPolicy {
    pub const DEFAULT: Self = Self {
        max_log_files: 8,
        file_prefix: "waywallen",
        default_filter: DEFAULT_FILTER,
        also_stderr: true,
        async_channel_size: 1024,
    };
}

pub const DEFAULT: LoggingPolicy = LoggingPolicy::DEFAULT;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn filter_for_debug_selects_expected_strings() {
        assert_eq!(filter_for_debug(false), DEFAULT_FILTER);
        assert_eq!(filter_for_debug(true), DEBUG_FILTER);
    }
}
