//! Variable difficulty controller (SECURITY-001-E / H-5d).
//!
//! Share difficulty was previously fixed at `1.0` and fed to
//! `share_target_from_block`, which yields `share_target == network target`: a
//! miner only ever produced an accepted share when it produced a whole block. The
//! pool therefore had no hashrate measurement, no PPLNS signal, and no way to
//! detect or throttle an abusive connection.
//!
//! This controller retargets each session towards a fixed share interval, with
//! hard clamps so a client cannot drive its own difficulty to zero and flood the
//! server with trivially-valid work.
//!
//! Consensus impact: NONE — share difficulty is a pool-internal accounting knob.
//! Block candidacy is still decided against the network target by the validator.

/// Vardiff policy bounds.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct VardiffConfig {
    pub min_difficulty: f64,
    pub max_difficulty: f64,
    pub start_difficulty: f64,
    /// Desired seconds between accepted shares per connection.
    pub target_share_interval_secs: f64,
    /// Retarget after this many accepted shares.
    pub retarget_share_count: u32,
    /// Never change difficulty by more than this factor in one step.
    pub max_step_factor: f64,
}

impl Default for VardiffConfig {
    fn default() -> Self {
        Self {
            min_difficulty: 1.0,
            max_difficulty: 4_294_967_296.0,
            start_difficulty: 1024.0,
            target_share_interval_secs: 10.0,
            retarget_share_count: 20,
            max_step_factor: 4.0,
        }
    }
}

impl VardiffConfig {
    pub fn validate(&self) -> Result<(), String> {
        if !(self.min_difficulty.is_finite() && self.min_difficulty > 0.0) {
            return Err("min_difficulty must be finite and > 0".into());
        }
        if !(self.max_difficulty.is_finite() && self.max_difficulty >= self.min_difficulty) {
            return Err("max_difficulty must be finite and >= min_difficulty".into());
        }
        if !(self.start_difficulty >= self.min_difficulty
            && self.start_difficulty <= self.max_difficulty)
        {
            return Err("start_difficulty must be within [min, max]".into());
        }
        if !(self.target_share_interval_secs.is_finite() && self.target_share_interval_secs > 0.0) {
            return Err("target_share_interval_secs must be finite and > 0".into());
        }
        if self.retarget_share_count == 0 {
            return Err("retarget_share_count must be > 0".into());
        }
        if !(self.max_step_factor.is_finite() && self.max_step_factor > 1.0) {
            return Err("max_step_factor must be finite and > 1".into());
        }
        Ok(())
    }

    pub fn clamp(&self, difficulty: f64) -> f64 {
        if !difficulty.is_finite() || difficulty <= 0.0 {
            return self.min_difficulty;
        }
        difficulty.clamp(self.min_difficulty, self.max_difficulty)
    }
}

/// Per-session accepted-share observation window.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct VardiffWindow {
    pub shares: u32,
    pub window_start_secs: u64,
}

impl VardiffWindow {
    pub fn reset(&mut self, now_secs: u64) {
        self.shares = 0;
        self.window_start_secs = now_secs;
    }
}

/// Compute the next difficulty, or `None` when it is not time to retarget yet.
///
/// `window.shares` must already include the share just accepted.
pub fn next_difficulty(
    config: &VardiffConfig,
    current: f64,
    window: &VardiffWindow,
    now_secs: u64,
) -> Option<f64> {
    if window.shares < config.retarget_share_count {
        return None;
    }
    let elapsed = now_secs.saturating_sub(window.window_start_secs) as f64;
    // A zero-length window means shares arrived faster than clock resolution:
    // treat it as maximally fast and step difficulty up by the cap.
    let observed_interval = if elapsed <= 0.0 {
        0.0
    } else {
        elapsed / window.shares as f64
    };

    let raw = if observed_interval <= 0.0 {
        current * config.max_step_factor
    } else {
        current * (config.target_share_interval_secs / observed_interval)
    };

    let stepped = raw.clamp(
        current / config.max_step_factor,
        current * config.max_step_factor,
    );
    let next = config.clamp(stepped);
    if (next - current).abs() < f64::EPSILON * current.max(1.0) {
        return None;
    }
    Some(next)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cfg() -> VardiffConfig {
        VardiffConfig::default()
    }

    #[test]
    fn default_config_is_valid_and_not_unit_difficulty() {
        cfg().validate().unwrap();
        // H-5d regression: the start difficulty must not be the old fixed 1.0,
        // which made share_target equal to the network target.
        assert!(cfg().start_difficulty > 1.0);
    }

    #[test]
    fn invalid_configs_are_rejected() {
        let mut c = cfg();
        c.min_difficulty = 0.0;
        assert!(c.validate().is_err());

        let mut c = cfg();
        c.max_difficulty = 0.5;
        assert!(c.validate().is_err());

        let mut c = cfg();
        c.retarget_share_count = 0;
        assert!(c.validate().is_err());

        let mut c = cfg();
        c.max_step_factor = 1.0;
        assert!(c.validate().is_err());

        let mut c = cfg();
        c.start_difficulty = c.max_difficulty * 2.0;
        assert!(c.validate().is_err());
    }

    #[test]
    fn no_retarget_before_window_is_full() {
        let w = VardiffWindow {
            shares: 5,
            window_start_secs: 0,
        };
        assert!(next_difficulty(&cfg(), 1024.0, &w, 100).is_none());
    }

    #[test]
    fn fast_shares_raise_difficulty_and_slow_shares_lower_it() {
        let c = cfg();
        // 20 shares in 20s => 1s/share, 10x too fast => difficulty up (capped 4x).
        let fast = VardiffWindow {
            shares: 20,
            window_start_secs: 0,
        };
        let up = next_difficulty(&c, 1024.0, &fast, 20).unwrap();
        assert_eq!(up, 4096.0);

        // 20 shares in 2000s => 100s/share, 10x too slow => difficulty down (capped /4).
        let slow = VardiffWindow {
            shares: 20,
            window_start_secs: 0,
        };
        let down = next_difficulty(&c, 1024.0, &slow, 2000).unwrap();
        assert_eq!(down, 256.0);
    }

    #[test]
    fn difficulty_is_clamped_to_configured_bounds() {
        let mut c = cfg();
        c.min_difficulty = 512.0;
        c.max_difficulty = 2048.0;
        c.start_difficulty = 1024.0;
        c.validate().unwrap();

        let slow = VardiffWindow {
            shares: 20,
            window_start_secs: 0,
        };
        assert_eq!(next_difficulty(&c, 1024.0, &slow, 100_000).unwrap(), 512.0);

        let instant = VardiffWindow {
            shares: 20,
            window_start_secs: 50,
        };
        assert_eq!(next_difficulty(&c, 1024.0, &instant, 50).unwrap(), 2048.0);
    }

    /// A client cannot drive difficulty below the floor no matter how slow it is.
    #[test]
    fn repeated_slow_windows_cannot_go_below_floor() {
        let c = cfg();
        let mut d = c.start_difficulty;
        for _ in 0..50 {
            let w = VardiffWindow {
                shares: c.retarget_share_count,
                window_start_secs: 0,
            };
            if let Some(next) = next_difficulty(&c, d, &w, 1_000_000) {
                d = next;
            }
        }
        assert_eq!(d, c.min_difficulty);
    }
}
