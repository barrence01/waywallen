use super::*;

impl Router {
    pub async fn update_session_state(
        self: &Arc<Self>,
        locked: Option<bool>,
        inactive: Option<bool>,
        gamemode: Option<bool>,
    ) {
        {
            let mut inner = self.inner.lock().await;
            if let Some(locked) = locked {
                inner.session_locked = locked;
            }
            if let Some(inactive) = inactive {
                inner.session_inactive = inactive;
            }
            if let Some(gamemode) = gamemode {
                inner.gamemode = gamemode;
            }
        }
        self.refresh_auto_policy(false).await;
    }

    pub(super) async fn refresh_auto_policy(self: &Arc<Self>, reset: bool) {
        let changed = {
            let mut inner = self.inner.lock().await;
            let now = tokio::time::Instant::now();
            let global = self
                .settings
                .get()
                .map(|s| s.global().effective_auto_replay())
                .unwrap_or_default();
            let facts = auto_replay::Facts {
                flags: 0,
                session_locked: inner.session_locked,
                session_inactive: inner.session_inactive,
                gamemode: inner.gamemode,
            };
            inner
                .session_auto_replay
                .update(global, facts, true, now, reset);
            self.schedule_auto_resume(auto_replay::Source::Session, &inner.session_auto_replay);
            let mut effects = inner.session_auto_replay.effects();
            for (&id, state) in &mut inner.displays {
                let policy = self.resolved_auto_replay(&state.info);
                state.auto_replay.update(
                    policy,
                    auto_replay::Facts {
                        flags: state.auto_replay.last_flags,
                        ..facts
                    },
                    false,
                    now,
                    reset,
                );
                self.schedule_auto_resume(auto_replay::Source::Display(id), &state.auto_replay);
                let contribution = state.auto_replay.effects();
                effects.merge(auto_replay::Effects {
                    pause: false,
                    stop_local: false,
                    ..contribution
                });
            }
            let mut changed = inner.auto_effects != effects;
            inner.auto_effects = effects;
            for state in inner.displays.values_mut() {
                let was_paused = state.display_paused();
                let was_auto_paused = state.auto_paused;
                state.auto_paused =
                    !effects.stop && (effects.pause_all || state.auto_replay.effects().pause);
                state.consumption_changed(was_paused);
                changed |= was_auto_paused != state.auto_paused
                    || state.auto_replay.stop_applied
                        != (effects.stop || state.auto_replay.effects().stop_local);
            }
            changed
        };
        if changed {
            self.reconcile_lifecycle().await;
        }
    }

    fn schedule_auto_resume(&self, source: auto_replay::Source, state: &auto_replay::State) {
        let key = deadline::DeadlineKey::AutoReplayResume(source);
        if let Some(at) = state.next_deadline() {
            self.deadlines.schedule(key, state.resume_token, at);
        } else {
            self.deadlines.cancel(key);
        }
    }

    pub(super) async fn finish_auto_replay_resume(
        self: &Arc<Self>,
        source: auto_replay::Source,
        token: u64,
    ) {
        let current = {
            let inner = self.inner.lock().await;
            match source {
                auto_replay::Source::Session => Some(&inner.session_auto_replay),
                auto_replay::Source::Display(id) => inner.displays.get(&id).map(|s| &s.auto_replay),
            }
            .is_some_and(|s| s.resume_token == token && s.next_deadline().is_some())
        };
        if current {
            self.refresh_auto_policy(false).await;
        }
    }
}
