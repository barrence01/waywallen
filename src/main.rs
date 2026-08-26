fn main() -> anyhow::Result<()> {
    let _logging = waywallen::logging::init(waywallen::logging::LoggingPolicy::DEFAULT);

    let config = waywallen::daemon::DaemonConfig::from_env();
    let runtime = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()?;
    let result = runtime.block_on(waywallen::daemon::run(config));
    runtime.shutdown_timeout(std::time::Duration::from_secs(3));
    result
}
