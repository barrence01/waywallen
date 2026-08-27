use std::sync::Arc;

use crate::error::Result;
use crate::DaemonContext;

pub async fn step_active(app: &Arc<DaemonContext>, delta: i32) -> Result<()> {
    if super::playlist::step_sessions(app, delta).await? {
        return Ok(());
    }
    super::queue::step(app, delta).await.map(|_| ())
}
