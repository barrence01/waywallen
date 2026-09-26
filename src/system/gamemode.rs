use std::sync::Arc;

use futures_util::StreamExt;
use tokio::sync::watch;

use crate::wallframe::routing::Router;

#[zbus::proxy(
    interface = "com.feralinteractive.GameMode",
    default_service = "com.feralinteractive.GameMode",
    default_path = "/com/feralinteractive/GameMode"
)]
trait GameMode {
    #[zbus(property)]
    fn client_count(&self) -> zbus::Result<i32>;
}

pub async fn run(
    router: Arc<Router>,
    connection: zbus::Connection,
    mut shutdown: watch::Receiver<bool>,
) {
    let proxy = match GameModeProxy::new(&connection).await {
        Ok(proxy) => proxy,
        Err(error) => {
            log::warn!("gamemode_monitor: failed to create proxy: {error}");
            return;
        }
    };
    let mut changes = proxy.receive_client_count_changed().await;

    match proxy.client_count().await {
        Ok(count) => update(&router, count).await,
        Err(error) => log::debug!("gamemode_monitor: unavailable: {error}"),
    }

    loop {
        tokio::select! {
            result = shutdown.changed() => {
                if result.is_err() || *shutdown.borrow() {
                    break;
                }
            }
            changed = changes.next() => {
                let Some(changed) = changed else {
                    log::warn!("gamemode_monitor: property stream ended");
                    break;
                };
                match changed.get().await {
                    Ok(count) => update(&router, count).await,
                    Err(error) => {
                        log::warn!("gamemode_monitor: read after change failed: {error}");
                    }
                }
            }
        }
    }
}

async fn update(router: &Arc<Router>, client_count: i32) {
    let active = client_count > 0;
    log::info!("gamemode_monitor: active={active} client_count={client_count}");
    router.update_gamemode_state(active).await;
}
