use std::fs::{File, OpenOptions};
use std::io::{self, BufWriter, Write};
use std::path::{Path, PathBuf};
use std::sync::Mutex;

use serde::Serialize;

use crate::schema::file_in_metrics_dir;

#[derive(Debug)]
pub enum MetricsWriterError {
    Io(io::Error),
    Serde(serde_json::Error),
}

impl From<io::Error> for MetricsWriterError {
    fn from(e: io::Error) -> Self {
        Self::Io(e)
    }
}

impl From<serde_json::Error> for MetricsWriterError {
    fn from(e: serde_json::Error) -> Self {
        Self::Serde(e)
    }
}

struct WriterState {
    path: PathBuf,
    day: String,
    writer: BufWriter<File>,
}

/// Append-only JSONL metrics writer with daily rotation ( Part E).
pub struct JsonlMetricsWriter {
    state: Mutex<WriterState>,
}

impl JsonlMetricsWriter {
    pub fn create_if_missing(dir: impl AsRef<Path>, filename: &str) -> io::Result<Self> {
        std::fs::create_dir_all(dir.as_ref())?;
        let path = dir.as_ref().join(filename);
        Self::open_path(path)
    }

    pub fn open_default(filename: &str) -> io::Result<Self> {
        Self::open_path(file_in_metrics_dir(filename)?)
    }

    fn open_path(path: PathBuf) -> io::Result<Self> {
        let day = utc_day_string();
        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&path)?;
        Ok(Self {
            state: Mutex::new(WriterState {
                path,
                day,
                writer: BufWriter::new(file),
            }),
        })
    }

    pub fn append_json_line<T: Serialize>(&self, row: &T) -> Result<(), MetricsWriterError> {
        let line = serde_json::to_string(row)?;
        let mut guard = self
            .state
            .lock()
            .map_err(|_| io::Error::other("metrics writer lock poisoned"))?;
        self.rotate_daily_locked(&mut guard)?;
        writeln!(guard.writer, "{line}")?;
        guard.writer.flush()?;
        Ok(())
    }

    pub fn flush(&self) -> io::Result<()> {
        let mut guard = self
            .state
            .lock()
            .map_err(|_| io::Error::other("metrics writer lock poisoned"))?;
        guard.writer.flush()
    }

    pub fn rotate_daily(&self) -> io::Result<()> {
        let mut guard = self
            .state
            .lock()
            .map_err(|_| io::Error::other("metrics writer lock poisoned"))?;
        self.rotate_daily_locked(&mut guard)
    }

    fn rotate_daily_locked(&self, state: &mut WriterState) -> io::Result<()> {
        let today = utc_day_string();
        if state.day == today {
            return Ok(());
        }
        state.writer.flush()?;
        let stem = state
            .path
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("metrics");
        let archive = state
            .path
            .parent()
            .unwrap_or_else(|| Path::new("."))
            .join(format!("{stem}.{}.jsonl", state.day));
        if state.path.exists() {
            let _ = std::fs::rename(&state.path, &archive);
        }
        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&state.path)?;
        state.writer = BufWriter::new(file);
        state.day = today;
        Ok(())
    }

    pub fn path(&self) -> io::Result<PathBuf> {
        let guard = self
            .state
            .lock()
            .map_err(|_| io::Error::other("metrics writer lock poisoned"))?;
        Ok(guard.path.clone())
    }

    /// Test hook for daily rotation verification.
    pub fn set_day_for_test(&self, day: &str) {
        let mut guard = self.state.lock().unwrap();
        guard.day = day.to_string();
    }
}

fn utc_day_string() -> String {
    let dur = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();
    let days = dur.as_secs() / 86_400;
    let (y, m, d) = super::timestamp::civil_from_days(days as i64);
    format!("{y:04}-{m:02}-{d:02}")
}
