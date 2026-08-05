use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct RecordTimestamps {
    pub ts: String,
    pub ts_ms: u64,
}

pub fn now_record_timestamps() -> RecordTimestamps {
    let dur = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default();
    let ts_ms = dur.as_millis() as u64;
    let secs = dur.as_secs();
    let nanos = dur.subsec_nanos();
    RecordTimestamps {
        ts: format_utc_rfc3339(secs, nanos),
        ts_ms,
    }
}

fn format_utc_rfc3339(secs: u64, nanos: u32) -> String {
    // chrono-free ISO-8601 UTC for schema compliance.
    let days = secs / 86_400;
    let day_secs = secs % 86_400;
    let hour = (day_secs / 3600) as u32;
    let minute = ((day_secs % 3600) / 60) as u32;
    let second = (day_secs % 60) as u32;

    let (year, month, day) = civil_from_days(days as i64);
    if nanos == 0 {
        format!(
            "{year:04}-{month:02}-{day:02}T{hour:02}:{minute:02}:{second:02}Z"
        )
    } else {
        let millis = nanos / 1_000_000;
        format!(
            "{year:04}-{month:02}-{day:02}T{hour:02}:{minute:02}:{second:02}.{millis:03}Z"
        )
    }
}

pub fn civil_from_days(z: i64) -> (i32, u32, u32) {
    let z = z + 719_468;
    let era = (if z >= 0 { z } else { z - 146_096 }) / 146_097;
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1_460 + doe / 36_524 - doe / 146_096) / 365;
    let y = (yoe as i32) + era as i32 * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = (doy - (153 * mp + 2) / 5 + 1) as u32;
    let m = (mp + if mp < 10 { 3 } else { -9 }) as u32;
    let year = y + if m <= 2 { 1 } else { 0 };
    (year, m, d)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn timestamps_have_required_fields() {
        let t = now_record_timestamps();
        assert!(t.ts.ends_with('Z'));
        assert!(t.ts.contains('T'));
        assert!(t.ts_ms > 0);
    }
}
