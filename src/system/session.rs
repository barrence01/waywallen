use std::sync::Arc;

use anyhow::Result;
use futures_util::StreamExt;
use tokio::sync::mpsc;
use tokio::sync::watch;
use tokio::task::JoinSet;
use zbus::proxy;
use zbus::zvariant::OwnedObjectPath;

use crate::wallframe::routing::Router;

// ---------------------------------------------------------------------------
// D-Bus proxy definitions

#[proxy(
    interface = "org.freedesktop.ScreenSaver",
    default_service = "org.freedesktop.ScreenSaver",
    default_path = "/org/freedesktop/ScreenSaver"
)]
trait ScreenSaver {
    /// Fired when the screensaver / lock screen is activated or deactivated.
    #[zbus(signal)]
    fn active_changed(&self, new_value: bool) -> zbus::Result<()>;

    /// Returns the current active state synchronously so we can seed the
    /// initial value without waiting for the first signal.
    fn get_active(&self) -> zbus::Result<bool>;
}

#[proxy(
    interface = "org.freedesktop.login1.Session",
    default_service = "org.freedesktop.login1"
)]
trait Login1Session {
    /// `true` when this session is the currently active VT session.
    #[zbus(property)]
    fn active(&self) -> zbus::Result<bool>;
}

#[proxy(
    interface = "org.freedesktop.login1.Manager",
    default_service = "org.freedesktop.login1",
    default_path = "/org/freedesktop/login1"
)]
trait Login1Manager {
    fn get_session(&self, session_id: &str) -> zbus::Result<OwnedObjectPath>;

    #[zbus(signal)]
    fn session_removed(&self, session_id: &str, object_path: OwnedObjectPath) -> zbus::Result<()>;
}

// ---------------------------------------------------------------------------
// Internal event type

enum SessionEvent {
    /// The screen-saver / lock screen changed state.
    Locked(bool),
    /// The login session's `Active` property changed.
    SessionActive(bool),
    /// The current login1 session was removed.
    LoginSessionRemoved,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ExitReason {
    DaemonShutdown,
    LoginSessionRemoved,
}

// ---------------------------------------------------------------------------
// Public entry point

pub async fn run(
    router: Arc<Router>,
    session_bus: zbus::Connection,
    mut shutdown: watch::Receiver<bool>,
) -> Result<ExitReason> {
    log::info!("session_monitor: starting");
    let (tx, mut rx) = mpsc::channel::<SessionEvent>(16);
    let mut watchers = JoinSet::new();

    // --- Screen saver (session bus) ----------------------------------------
    log::info!("session_monitor: session bus connected, starting screen-saver monitor");
    {
        let tx2 = tx.clone();
        watchers.spawn(async move {
            monitor_screen_saver(session_bus, tx2).await;
        });
    }

    // --- Login session (system bus) ----------------------------------------
    match zbus::Connection::system().await {
        Ok(conn) => {
            log::info!("session_monitor: system bus connected, starting login-session monitor");
            let tx2 = tx.clone();
            watchers.spawn(async move {
                monitor_login_session(conn, tx2).await;
            });
        }
        Err(e) => {
            log::warn!("session_monitor: cannot connect to D-Bus system bus: {e}");
        }
    }

    // --- Main dispatch loop -------------------------------------------------
    let reason = loop {
        tokio::select! {
            // Watch for shutdown signal.
            result = shutdown.changed() => {
                if result.is_err() || *shutdown.borrow() {
                    break ExitReason::DaemonShutdown;
                }
            }
            // Receive events from sub-tasks and forward to router.
            Some(event) = rx.recv() => {
                match event {
                    SessionEvent::Locked(active) => {
                        log::info!("session_monitor: screen lock active={active}");
                        router.update_session_state(Some(active), None).await;
                    }
                    SessionEvent::SessionActive(active) => {
                        // session_inactive is the inverse of `Active`
                        log::info!("session_monitor: login session active={active}");
                        router.update_session_state(None, Some(!active)).await;
                    }
                    SessionEvent::LoginSessionRemoved => {
                        log::info!("session_monitor: login session removed");
                        break ExitReason::LoginSessionRemoved;
                    }
                }
            }
            joined = watchers.join_next(), if !watchers.is_empty() => {
                if let Some(Err(error)) = joined {
                    log::warn!("session_monitor: watcher join failed: {error}");
                }
            }
        }
    };

    watchers.abort_all();
    while watchers.join_next().await.is_some() {}
    log::info!("session_monitor: stopped");
    Ok(reason)
}

// ---------------------------------------------------------------------------
// Screen saver sub-task

async fn monitor_screen_saver(conn: zbus::Connection, tx: mpsc::Sender<SessionEvent>) {
    log::info!(
        "session_monitor: subscribing to org.freedesktop.ScreenSaver \
         at /org/freedesktop/ScreenSaver"
    );
    let proxy = match ScreenSaverProxy::new(&conn).await {
        Ok(p) => p,
        Err(e) => {
            log::warn!("session_monitor: ScreenSaver proxy unavailable: {e}");
            return;
        }
    };

    // Seed initial state.
    match proxy.get_active().await {
        Ok(active) => {
            log::info!("session_monitor: ScreenSaver initial state: active={active}");
            let _ = tx.send(SessionEvent::Locked(active)).await;
        }
        Err(e) => {
            // Service may not be running yet (screen not locked on startup).
            log::info!("session_monitor: ScreenSaver.GetActive not available yet: {e}");
        }
    }

    // Subscribe to future changes.
    let mut stream = match proxy.receive_active_changed().await {
        Ok(s) => s,
        Err(e) => {
            log::warn!("session_monitor: ScreenSaver.ActiveChanged subscribe failed: {e}");
            return;
        }
    };

    log::info!("session_monitor: listening for ScreenSaver.ActiveChanged signals");

    while let Some(signal) = stream.next().await {
        match signal.args() {
            Ok(args) => {
                log::info!(
                    "session_monitor: ScreenSaver.ActiveChanged new_value={}",
                    args.new_value()
                );
                let _ = tx.send(SessionEvent::Locked(*args.new_value())).await;
            }
            Err(e) => {
                log::warn!("session_monitor: bad ActiveChanged args: {e}");
            }
        }
    }

    log::info!("session_monitor: ScreenSaver signal stream ended (service stopped?)");
}

// ---------------------------------------------------------------------------
// Login session sub-task

async fn monitor_login_session(conn: zbus::Connection, tx: mpsc::Sender<SessionEvent>) {
    let session_id = match std::env::var("XDG_SESSION_ID") {
        Ok(id) if !id.is_empty() => id,
        _ => {
            log::warn!("session_monitor: $XDG_SESSION_ID not set; login-session monitor disabled");
            return;
        }
    };

    let manager = match Login1ManagerProxy::new(&conn).await {
        Ok(proxy) => proxy,
        Err(error) => {
            log::warn!("session_monitor: login1 Manager proxy unavailable: {error}");
            return;
        }
    };
    let mut removed = match manager.receive_session_removed().await {
        Ok(stream) => stream,
        Err(error) => {
            log::warn!("session_monitor: SessionRemoved subscribe failed: {error}");
            return;
        }
    };
    let session_path = match manager.get_session(&session_id).await {
        Ok(path) => path,
        Err(error) => {
            log::warn!("session_monitor: login1 GetSession({session_id}) failed: {error}");
            return;
        }
    };
    log::info!("session_monitor: login1 session path: {session_path}");

    let session_builder = match Login1SessionProxy::builder(&conn).path(session_path.clone()) {
        Ok(b) => b,
        Err(e) => {
            log::warn!("session_monitor: login1 Session path invalid: {e}");
            return;
        }
    };
    let session = match session_builder.build().await {
        Ok(p) => p,
        Err(e) => {
            log::warn!("session_monitor: login1 Session proxy unavailable: {e}");
            return;
        }
    };

    // Seed initial state.
    match session.active().await {
        Ok(active) => {
            log::info!("session_monitor: login1 Session initial Active={active}");
            let _ = tx.send(SessionEvent::SessionActive(active)).await;
        }
        Err(e) => {
            log::warn!("session_monitor: Session.Active initial read failed: {e}");
        }
    }

    // Subscribe to property changes.
    let mut active_changes = session.receive_active_changed().await;

    log::info!("session_monitor: listening for login1 session changes");

    loop {
        tokio::select! {
            Some(change) = active_changes.next() => {
                match change.get().await {
                    Ok(active) => {
                        log::info!("session_monitor: login1 Session.Active changed to {active}");
                        let _ = tx.send(SessionEvent::SessionActive(active)).await;
                    }
                    Err(error) => {
                        log::warn!("session_monitor: Session.Active read after change failed: {error}");
                    }
                }
            }
            Some(signal) = removed.next() => {
                match signal.args() {
                    Ok(args)
                        if session_removed_matches(
                            &session_id,
                            &session_path,
                            args.session_id,
                            &args.object_path,
                        ) =>
                    {
                        let _ = tx.send(SessionEvent::LoginSessionRemoved).await;
                        return;
                    }
                    Ok(_) => {}
                    Err(error) => {
                        log::warn!("session_monitor: bad SessionRemoved args: {error}");
                    }
                }
            }
            else => break,
        }
    }

    log::info!("session_monitor: login1 session streams ended");
}

fn session_removed_matches(
    current_id: &str,
    current_path: &OwnedObjectPath,
    removed_id: &str,
    removed_path: &OwnedObjectPath,
) -> bool {
    current_id == removed_id && current_path == removed_path
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn session_removed_only_matches_current_session() {
        let current_path = OwnedObjectPath::try_from("/org/freedesktop/login1/session/3").unwrap();
        let other_path = OwnedObjectPath::try_from("/org/freedesktop/login1/session/4").unwrap();

        assert!(session_removed_matches(
            "3",
            &current_path,
            "3",
            &current_path
        ));
        assert!(!session_removed_matches(
            "3",
            &current_path,
            "3",
            &other_path
        ));
        assert!(!session_removed_matches(
            "3",
            &current_path,
            "4",
            &other_path
        ));
    }
}
