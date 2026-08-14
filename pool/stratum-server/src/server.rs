//! Blocking Stratum v1 TCP server.
//!
//! SECURITY-001-E / H-5c: the previous implementation called `read_line` on an
//! unbounded `BufReader`, with no connection limit and no socket timeouts. A single
//! client that opened a socket and streamed bytes without ever sending `\n` grew
//! the line buffer until the process was OOM-killed; a client that connected and
//! went silent held a thread forever; and there was no cap on concurrent threads.
//!
//! Consensus impact: NONE — transport-layer resource limits only.

use std::io::{BufRead, BufReader, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

use crate::error::{StratumError, StratumResult};
use crate::orchestrator::StratumOrchestrator;

/// Hard cap on a single Stratum request line. Real `mining.submit` /
/// `mining.subscribe` lines are a few hundred bytes.
pub const MAX_LINE_BYTES: usize = 8 * 1024;

/// Cap on concurrent connections.
pub const MAX_CONNECTIONS: usize = 4_096;

/// Socket read timeout — short enough to flush broadcast job notifies between
/// miner submits (was 300s; that left miners stuck on stale work after tip change).
pub const READ_TIMEOUT: Duration = Duration::from_secs(2);

/// Socket write timeout — a peer that stops reading cannot block us indefinitely.
pub const WRITE_TIMEOUT: Duration = Duration::from_secs(30);

/// Cap on requests per connection before it is recycled (cheap flood brake).
pub const MAX_REQUESTS_PER_CONNECTION: u64 = 1_000_000;

/// Tunable transport limits.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ServerLimits {
    pub max_line_bytes: usize,
    pub max_connections: usize,
    pub read_timeout: Duration,
    pub write_timeout: Duration,
    pub max_requests_per_connection: u64,
}

impl Default for ServerLimits {
    fn default() -> Self {
        Self {
            max_line_bytes: MAX_LINE_BYTES,
            max_connections: MAX_CONNECTIONS,
            read_timeout: READ_TIMEOUT,
            write_timeout: WRITE_TIMEOUT,
            max_requests_per_connection: MAX_REQUESTS_PER_CONNECTION,
        }
    }
}

impl ServerLimits {
    pub fn validate(&self) -> StratumResult<()> {
        if self.max_line_bytes == 0 {
            return Err(StratumError::Internal("max_line_bytes must be > 0".into()));
        }
        if self.max_connections == 0 {
            return Err(StratumError::Internal("max_connections must be > 0".into()));
        }
        if self.read_timeout.is_zero() || self.write_timeout.is_zero() {
            return Err(StratumError::Internal(
                "socket timeouts must be non-zero (H-5c)".into(),
            ));
        }
        Ok(())
    }
}

/// Blocking Stratum v1 TCP server (one thread per connection, bounded).
pub struct StratumServer {
    orchestrator: Arc<StratumOrchestrator>,
    listen_addr: String,
    limits: ServerLimits,
    live_connections: Arc<AtomicUsize>,
}

impl StratumServer {
    pub fn new(orchestrator: Arc<StratumOrchestrator>, listen_addr: impl Into<String>) -> Self {
        Self::with_limits(orchestrator, listen_addr, ServerLimits::default())
    }

    pub fn with_limits(
        orchestrator: Arc<StratumOrchestrator>,
        listen_addr: impl Into<String>,
        limits: ServerLimits,
    ) -> Self {
        Self {
            orchestrator,
            listen_addr: listen_addr.into(),
            limits,
            live_connections: Arc::new(AtomicUsize::new(0)),
        }
    }

    pub fn limits(&self) -> &ServerLimits {
        &self.limits
    }

    pub fn live_connections(&self) -> usize {
        self.live_connections.load(Ordering::Relaxed)
    }

    pub fn run(&self) -> StratumResult<()> {
        self.limits.validate()?;
        let listener = TcpListener::bind(&self.listen_addr)
            .map_err(|e| StratumError::Internal(e.to_string()))?;
        let mut session_id = 1u64;
        for stream in listener.incoming() {
            // A single failed accept must not tear the whole listener down.
            let stream = match stream {
                Ok(s) => s,
                Err(e) => {
                    self.orchestrator
                        .audit
                        .record("stratum.accept.error", e.to_string());
                    continue;
                }
            };

            let live = self.live_connections.fetch_add(1, Ordering::AcqRel) + 1;
            if live > self.limits.max_connections {
                self.live_connections.fetch_sub(1, Ordering::AcqRel);
                self.orchestrator.audit.record(
                    "stratum.conn.rejected",
                    format!("connection limit {} reached", self.limits.max_connections),
                );
                drop(stream);
                continue;
            }

            let orch = self.orchestrator.clone();
            let sid = session_id;
            session_id += 1;
            let limits = self.limits;
            let counter = self.live_connections.clone();
            thread::spawn(move || {
                let result = handle_connection(&orch, sid, stream, &limits);
                if let Err(e) = &result {
                    orch.audit
                        .record("stratum.conn.closed", format!("session={sid} err={e}"));
                }
                orch.sessions.remove(sid);
                counter.fetch_sub(1, Ordering::AcqRel);
            });
        }
        Ok(())
    }
}

fn handle_connection(
    orch: &Arc<StratumOrchestrator>,
    session_id: u64,
    stream: TcpStream,
    limits: &ServerLimits,
) -> StratumResult<()> {
    stream
        .set_read_timeout(Some(limits.read_timeout))
        .map_err(|e| StratumError::Internal(e.to_string()))?;
    stream
        .set_write_timeout(Some(limits.write_timeout))
        .map_err(|e| StratumError::Internal(e.to_string()))?;

    let mut reader = BufReader::with_capacity(
        limits.max_line_bytes.min(64 * 1024),
        stream
            .try_clone()
            .map_err(|e| StratumError::Internal(e.to_string()))?,
    );
    let mut writer = stream;
    let mut line = Vec::with_capacity(512);
    let mut requests = 0u64;
    let mut seen_notify_version = orch.notify_hub.version();

    loop {
        // Flush any newly published jobs before blocking on the next request.
        if let Some((ver, lines)) = orch.notify_hub.take_since(seen_notify_version) {
            for notify_line in lines {
                writer
                    .write_all(notify_line.as_bytes())
                    .map_err(|e| StratumError::Internal(e.to_string()))?;
            }
            let _ = writer.flush();
            seen_notify_version = ver;
        }

        line.clear();
        match read_line_capped(&mut reader, &mut line, limits.max_line_bytes) {
            Ok(0) => return Ok(()),
            Ok(_) => {}
            Err(e) => {
                let msg = e.to_string();
                // Read timeout / WouldBlock → flush notifies and keep the session.
                if is_idle_timeout(&msg) {
                    continue;
                }
                // Drop the connection; never grow a buffer for an abusive peer.
                let _ = writer.write_all(
                    b"{\"id\":null,\"result\":null,\"error\":[20,\"line too long\",null]}\n",
                );
                return Err(e);
            }
        }

        requests += 1;
        if requests > limits.max_requests_per_connection {
            return Err(StratumError::Internal(format!(
                "session={session_id} exceeded max_requests_per_connection"
            )));
        }

        let text = String::from_utf8_lossy(&line).into_owned();
        // A malformed request must close only this connection, never the listener.
        let responses = match orch.handle_request(session_id, &text) {
            Ok(r) => r,
            Err(e) => {
                orch.audit
                    .record("stratum.request.error", format!("session={session_id} err={e}"));
                return Err(e);
            }
        };
        for resp in responses {
            writer
                .write_all(resp.as_bytes())
                .map_err(|e| StratumError::Internal(e.to_string()))?;
        }
        let _ = writer.flush();

        // Also flush notifies that arrived during request handling (tip change).
        if let Some((ver, lines)) = orch.notify_hub.take_since(seen_notify_version) {
            for notify_line in lines {
                writer
                    .write_all(notify_line.as_bytes())
                    .map_err(|e| StratumError::Internal(e.to_string()))?;
            }
            let _ = writer.flush();
            seen_notify_version = ver;
        }
    }
}

fn is_idle_timeout(msg: &str) -> bool {
    let lower = msg.to_ascii_lowercase();
    lower.contains("timed out")
        || lower.contains("timeout")
        || lower.contains("wouldblock")
        || lower.contains("resource temporarily unavailable")
        || lower.contains("os error 11")
        || lower.contains("os error 35")
}

/// Read one `\n`-terminated line, refusing to buffer more than `max_bytes`.
///
/// Returns the number of bytes read (0 at clean EOF). This is the H-5c fix: the
/// previous `read_line` had no cap, so a peer streaming bytes with no newline grew
/// the buffer without bound.
pub fn read_line_capped<R: BufRead>(
    reader: &mut R,
    out: &mut Vec<u8>,
    max_bytes: usize,
) -> StratumResult<usize> {
    let mut total = 0usize;
    loop {
        let available = match reader.fill_buf() {
            Ok(b) => b,
            Err(ref e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(e) => return Err(StratumError::Internal(e.to_string())),
        };
        if available.is_empty() {
            if total == 0 {
                return Ok(0);
            }
            // EOF without a trailing newline: accept what we have.
            return Ok(total);
        }
        match available.iter().position(|b| *b == b'\n') {
            Some(idx) => {
                let take = idx + 1;
                if total + take > max_bytes {
                    reader.consume(take);
                    return Err(StratumError::Protocol(format!(
                        "request line exceeds {max_bytes} bytes (H-5c cap)"
                    )));
                }
                out.extend_from_slice(&available[..take]);
                reader.consume(take);
                return Ok(total + take);
            }
            None => {
                let take = available.len();
                if total + take > max_bytes {
                    reader.consume(take);
                    return Err(StratumError::Protocol(format!(
                        "request line exceeds {max_bytes} bytes without a newline (H-5c cap)"
                    )));
                }
                out.extend_from_slice(available);
                reader.consume(take);
                total += take;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[test]
    fn reads_normal_lines() {
        let mut r = Cursor::new(b"hello\nworld\n".to_vec());
        let mut buf = Vec::new();
        assert_eq!(read_line_capped(&mut r, &mut buf, 64).unwrap(), 6);
        assert_eq!(buf, b"hello\n");
        buf.clear();
        assert_eq!(read_line_capped(&mut r, &mut buf, 64).unwrap(), 6);
        assert_eq!(buf, b"world\n");
        buf.clear();
        assert_eq!(read_line_capped(&mut r, &mut buf, 64).unwrap(), 0);
    }

    /// H-5c regression: unbounded bytes with no newline must be refused with a
    /// bounded buffer instead of growing until OOM.
    #[test]
    fn newlineless_flood_is_capped_and_buffer_stays_bounded() {
        let flood = vec![b'A'; 1024 * 1024];
        let mut r = Cursor::new(flood);
        let mut buf = Vec::new();
        let err = read_line_capped(&mut r, &mut buf, 4096).unwrap_err().to_string();
        assert!(err.contains("H-5c"), "unexpected: {err}");
        assert!(buf.len() <= 4096, "buffer grew to {}", buf.len());
    }

    #[test]
    fn overlong_terminated_line_is_refused() {
        let mut data = vec![b'B'; 5000];
        data.push(b'\n');
        let mut r = Cursor::new(data);
        let mut buf = Vec::new();
        assert!(read_line_capped(&mut r, &mut buf, 4096).is_err());
    }

    #[test]
    fn line_at_exactly_the_cap_is_accepted() {
        let mut data = vec![b'C'; 63];
        data.push(b'\n');
        let mut r = Cursor::new(data);
        let mut buf = Vec::new();
        assert_eq!(read_line_capped(&mut r, &mut buf, 64).unwrap(), 64);
    }

    #[test]
    fn eof_without_newline_returns_partial_line() {
        let mut r = Cursor::new(b"partial".to_vec());
        let mut buf = Vec::new();
        assert_eq!(read_line_capped(&mut r, &mut buf, 64).unwrap(), 7);
        assert_eq!(buf, b"partial");
    }

    /// H-5c regression: limits must be sane and timeouts must never be disabled.
    #[test]
    fn limits_are_validated() {
        ServerLimits::default().validate().unwrap();

        let mut l = ServerLimits::default();
        l.max_line_bytes = 0;
        assert!(l.validate().is_err());

        let mut l = ServerLimits::default();
        l.max_connections = 0;
        assert!(l.validate().is_err());

        let mut l = ServerLimits::default();
        l.read_timeout = Duration::ZERO;
        assert!(l.validate().is_err());

        let mut l = ServerLimits::default();
        l.write_timeout = Duration::ZERO;
        assert!(l.validate().is_err());
    }
}
