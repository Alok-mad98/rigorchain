use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, RwLock};

use crate::auth::{AuthError, SharedPasswordAuthenticator, WorkerAuthenticator};
use crate::difficulty::share_target_from_difficulty;
use crate::vardiff::{next_difficulty, VardiffConfig, VardiffWindow};
use share_validator::expand_compact_bits;

/// Max failed `mining.authorize` attempts per session before it is locked out
/// (H-5a: online password guessing).
pub const MAX_AUTH_FAILURES: u32 = 5;

#[derive(Clone, Debug)]
pub struct MinerSession {
    pub session_id: u64,
    pub agent: String,
    pub extranonce1: Vec<u8>,
    pub extranonce2_size: u32,
    pub authorized: bool,
    /// Authenticated identity. H-5b: this — never a request parameter — is the
    /// only identity a share may be credited to.
    pub worker: Option<String>,
    pub difficulty: f64,
    pub subscription_id: String,
    pub auth_failures: u32,
    pub vardiff: VardiffWindow,
}

impl MinerSession {
    /// H-5b helper: the authenticated worker, if this session may be credited.
    pub fn authorized_worker(&self) -> Option<&str> {
        if self.authorized {
            self.worker.as_deref()
        } else {
            None
        }
    }
}

pub struct SessionManager {
    next_id: RwLock<u64>,
    sessions: RwLock<HashMap<u64, MinerSession>>,
    auth: Arc<dyn WorkerAuthenticator>,
    vardiff: VardiffConfig,
    /// Monotonic salt source for extranonce1 uniqueness.
    extranonce_counter: AtomicU64,
}

impl SessionManager {
    /// **Devtest only.** Wraps a shared plaintext password
    /// ([`SharedPasswordAuthenticator`]); `is_production_safe` returns false.
    pub fn new(pool_password: impl Into<String>) -> Self {
        Self::with_authenticator(Arc::new(SharedPasswordAuthenticator::new(pool_password)))
    }

    pub fn with_authenticator(auth: Arc<dyn WorkerAuthenticator>) -> Self {
        Self::with_authenticator_and_vardiff(auth, VardiffConfig::default())
    }

    pub fn with_authenticator_and_vardiff(
        auth: Arc<dyn WorkerAuthenticator>,
        vardiff: VardiffConfig,
    ) -> Self {
        Self {
            next_id: RwLock::new(1),
            sessions: RwLock::new(HashMap::new()),
            auth,
            vardiff,
            extranonce_counter: AtomicU64::new(0),
        }
    }

    /// H-5a startup gate: false while a devtest authenticator is installed.
    pub fn is_production_safe(&self) -> bool {
        !self.auth.is_devtest()
    }

    pub fn authenticator_description(&self) -> String {
        self.auth.describe()
    }

    pub fn vardiff_config(&self) -> &VardiffConfig {
        &self.vardiff
    }

    pub fn subscribe(&self, session_id: u64, agent: &str, extranonce2_size: u32) -> MinerSession {
        let id = session_id;
        {
            let mut n = self.next_id.write().unwrap();
            if *n <= id {
                *n = id + 1;
            }
        }
        // Distinct extranonce1 per subscription, including across id reuse, so two
        // connections can never be handed the same coinbase search space.
        let salt = self.extranonce_counter.fetch_add(1, Ordering::Relaxed);
        let mut en1 = vec![0u8; 4];
        en1[0] = (id & 0xff) as u8;
        en1[1] = ((id >> 8) & 0xff) as u8;
        en1[2] = (salt & 0xff) as u8;
        en1[3] = ((salt >> 8) & 0xff) as u8;
        let session = MinerSession {
            session_id: id,
            agent: agent.to_string(),
            extranonce1: en1,
            extranonce2_size,
            authorized: false,
            worker: None,
            difficulty: self.vardiff.start_difficulty,
            subscription_id: format!("{id:08x}"),
            auth_failures: 0,
            vardiff: VardiffWindow::default(),
        };
        self.sessions.write().unwrap().insert(id, session.clone());
        session
    }

    /// Authenticate and bind an identity to this session.
    ///
    /// H-5a: delegates to the configured [`WorkerAuthenticator`], counts failures
    /// and locks the session out after [`MAX_AUTH_FAILURES`]. Never auto-registers.
    pub fn authorize(
        &self,
        session_id: u64,
        worker: &str,
        password: &str,
    ) -> Result<(), AuthError> {
        {
            let guard = self
                .sessions
                .read()
                .map_err(|_| AuthError::Unavailable("session lock poisoned".into()))?;
            let session = guard.get(&session_id).ok_or(AuthError::UnknownWorker)?;
            if session.auth_failures >= MAX_AUTH_FAILURES {
                return Err(AuthError::Unavailable(
                    "too many failed authorize attempts on this connection".into(),
                ));
            }
        }

        let result = self.auth.authenticate(worker, password);
        let mut sessions = self
            .sessions
            .write()
            .map_err(|_| AuthError::Unavailable("session lock poisoned".into()))?;
        let session = sessions
            .get_mut(&session_id)
            .ok_or(AuthError::UnknownWorker)?;
        match &result {
            Ok(()) => {
                session.authorized = true;
                session.worker = Some(worker.to_string());
                session.auth_failures = 0;
            }
            Err(_) => {
                session.auth_failures = session.auth_failures.saturating_add(1);
                // A failed re-authorize must not keep a previously granted identity.
                session.authorized = false;
                session.worker = None;
            }
        }
        result
    }

    pub fn set_difficulty(&self, session_id: u64, difficulty: f64) {
        if let Ok(mut sessions) = self.sessions.write() {
            if let Some(s) = sessions.get_mut(&session_id) {
                s.difficulty = self.vardiff.clamp(difficulty);
            }
        }
    }

    /// Record an accepted share and return a new difficulty when vardiff retargets
    /// (H-5d). The caller is responsible for emitting `mining.set_difficulty`.
    pub fn observe_accepted_share(&self, session_id: u64, now_secs: u64) -> Option<f64> {
        let mut sessions = self.sessions.write().ok()?;
        let session = sessions.get_mut(&session_id)?;
        if session.vardiff.shares == 0 && session.vardiff.window_start_secs == 0 {
            session.vardiff.window_start_secs = now_secs;
        }
        session.vardiff.shares = session.vardiff.shares.saturating_add(1);
        let next = next_difficulty(&self.vardiff, session.difficulty, &session.vardiff, now_secs);
        if session.vardiff.shares >= self.vardiff.retarget_share_count {
            session.vardiff.reset(now_secs);
        }
        if let Some(d) = next {
            session.difficulty = d;
        }
        next
    }

    pub fn get(&self, session_id: u64) -> Option<MinerSession> {
        self.sessions.read().ok()?.get(&session_id).cloned()
    }

    pub fn remove(&self, session_id: u64) {
        if let Ok(mut sessions) = self.sessions.write() {
            sessions.remove(&session_id);
        }
    }

    pub fn session_count(&self) -> usize {
        self.sessions.read().map(|s| s.len()).unwrap_or(0)
    }

    /// Share target for a session's current difficulty.
    ///
    /// H-5d: this is the absolute Stratum target (`diff1_target / difficulty`),
    /// clamped so it is never *harder* than the network target — the old code
    /// divided the network target by a difficulty that was always 1.0, making a
    /// share exactly as hard as a block.
    pub fn share_target_for_session(
        &self,
        session_id: u64,
        block_target_hex: &str,
    ) -> Option<[u8; 32]> {
        let session = self.get(session_id)?;
        let block_target = expand_compact_bits(block_target_hex).ok()?;
        let share_target = share_target_from_difficulty(session.difficulty);
        if le_target_gt(&block_target, &share_target) {
            // Network target is already easier than the pool's share target; use it
            // so a share is never required to be harder than a block.
            return Some(block_target);
        }
        Some(share_target)
    }
}

/// Compare two little-endian uint256 targets: `a > b`.
fn le_target_gt(a: &[u8; 32], b: &[u8; 32]) -> bool {
    for i in (0..32).rev() {
        if a[i] != b[i] {
            return a[i] > b[i];
        }
    }
    false
}

pub type SharedSessionManager = Arc<SessionManager>;

#[cfg(test)]
mod tests {
    use super::*;
    use crate::auth::HashedCredentialAuthenticator;

    const SALT: &[u8] = b"0123456789abcdef";
    const PW: &str = "a-sufficiently-long-secret";

    fn hashed_manager() -> SessionManager {
        let auth = HashedCredentialAuthenticator::new();
        auth.register("alice.worker1", PW, SALT).unwrap();
        SessionManager::with_authenticator(Arc::new(auth))
    }

    #[test]
    fn devtest_manager_is_not_production_safe() {
        assert!(!SessionManager::new("x").is_production_safe());
        assert!(hashed_manager().is_production_safe());
    }

    /// H-5a regression: hardcoded `"x"` no longer authorizes anybody against a
    /// real credential store, and unknown workers are rejected outright.
    #[test]
    fn hardcoded_x_password_no_longer_authorizes() {
        let mgr = hashed_manager();
        mgr.subscribe(1, "rigor-miner/1.0-opushash", 4);
        assert_eq!(
            mgr.authorize(1, "alice.worker1", "x").unwrap_err(),
            AuthError::BadPassword
        );
        assert!(!mgr.get(1).unwrap().authorized);
        assert_eq!(
            mgr.authorize(1, "mallory", PW).unwrap_err(),
            AuthError::UnknownWorker
        );
        mgr.authorize(1, "alice.worker1", PW).unwrap();
        assert_eq!(mgr.get(1).unwrap().authorized_worker(), Some("alice.worker1"));
    }

    /// H-5a regression: brute forcing one connection is capped.
    #[test]
    fn repeated_auth_failures_lock_the_session() {
        let mgr = hashed_manager();
        mgr.subscribe(1, "agent", 4);
        for _ in 0..MAX_AUTH_FAILURES {
            assert!(mgr.authorize(1, "alice.worker1", "nope-nope-nope").is_err());
        }
        // Even the correct password is now refused on this connection.
        assert!(matches!(
            mgr.authorize(1, "alice.worker1", PW),
            Err(AuthError::Unavailable(_))
        ));
        assert!(!mgr.get(1).unwrap().authorized);
    }

    /// A failed re-authorize must drop the previously granted identity.
    #[test]
    fn failed_reauthorize_clears_identity() {
        let mgr = hashed_manager();
        mgr.subscribe(1, "agent", 4);
        mgr.authorize(1, "alice.worker1", PW).unwrap();
        assert!(mgr.get(1).unwrap().authorized);
        assert!(mgr.authorize(1, "alice.worker1", "wrong-but-long").is_err());
        let s = mgr.get(1).unwrap();
        assert!(!s.authorized);
        assert!(s.authorized_worker().is_none());
    }

    #[test]
    fn unauthorized_session_has_no_creditable_identity() {
        let mgr = hashed_manager();
        let s = mgr.subscribe(1, "agent", 4);
        assert!(s.authorized_worker().is_none());
    }

    /// H-5d regression: share difficulty is no longer pinned to 1.0, and against a
    /// hard network target the share target is strictly easier — so miners produce
    /// measurable shares instead of only whole blocks.
    #[test]
    fn share_target_is_easier_than_hard_network_target() {
        let mgr = hashed_manager();
        let s = mgr.subscribe(1, "agent", 4);
        assert!(s.difficulty > 1.0);

        // 0x1900ffff is far harder than difficulty-1 (0x1d00ffff).
        let block_target = expand_compact_bits("1900ffff").unwrap();
        let share_target = mgr.share_target_for_session(1, "1900ffff").unwrap();
        assert!(
            le_target_gt(&share_target, &block_target),
            "share target must be easier (numerically larger) than network target"
        );
    }

    /// A share must never be required to be harder than a block: when the network
    /// target is already easier than the pool's share target, use the network target.
    #[test]
    fn share_target_never_harder_than_network_target() {
        let mgr = hashed_manager();
        mgr.subscribe(1, "agent", 4);
        for bits in ["207fffff", "1d00ffff"] {
            let block_target = expand_compact_bits(bits).unwrap();
            let share_target = mgr.share_target_for_session(1, bits).unwrap();
            assert!(
                !le_target_gt(&block_target, &share_target),
                "{bits}: share target harder than network target"
            );
            assert_eq!(share_target, block_target, "{bits}: expected clamp to network target");
        }
    }

    #[test]
    fn vardiff_retargets_after_configured_share_count() {
        let mgr = hashed_manager();
        mgr.subscribe(1, "agent", 4);
        let n = mgr.vardiff_config().retarget_share_count;
        for i in 1..n {
            assert!(mgr.observe_accepted_share(1, 100 + i as u64).is_none());
        }
        // Window full and shares arrived far too fast => difficulty steps up.
        let next = mgr.observe_accepted_share(1, 100 + n as u64).unwrap();
        assert!(next > 1024.0);
        assert_eq!(mgr.get(1).unwrap().difficulty, next);
        // Window resets, so the very next share does not retarget again.
        assert!(mgr.observe_accepted_share(1, 200).is_none());
    }

    #[test]
    fn set_difficulty_is_clamped() {
        let mgr = hashed_manager();
        mgr.subscribe(1, "agent", 4);
        mgr.set_difficulty(1, 0.0);
        assert_eq!(mgr.get(1).unwrap().difficulty, mgr.vardiff_config().min_difficulty);
        mgr.set_difficulty(1, f64::INFINITY);
        assert_eq!(mgr.get(1).unwrap().difficulty, mgr.vardiff_config().min_difficulty);
        mgr.set_difficulty(1, 1e30);
        assert_eq!(mgr.get(1).unwrap().difficulty, mgr.vardiff_config().max_difficulty);
    }

    #[test]
    fn extranonce1_is_unique_per_subscription() {
        let mgr = hashed_manager();
        let a = mgr.subscribe(1, "agent", 4);
        let b = mgr.subscribe(1, "agent", 4);
        assert_ne!(a.extranonce1, b.extranonce1);
    }
}
