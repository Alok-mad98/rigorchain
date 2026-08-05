use std::collections::VecDeque;
use std::io::{BufRead, BufReader, Write};
use std::net::TcpStream;
use std::time::{Duration, Instant};

use serde_json::{json, Value};

use crate::error::{MinerError, MinerResult};
use crate::metrics::Metrics;
use crate::stratum::messages::{MiningNotify, MiningNotifyRigor, SetDifficulty, SubscribeResult};
use crate::stratum::protocol::{
    build_request, parse_line, requires_opushash_agent, StratumMessage, StratumResponse,
    DEFAULT_AGENT,
};

#[derive(Clone, Debug)]
pub struct PoolEndpoint {
    pub host: String,
    pub port: u16,
}

impl PoolEndpoint {
    pub fn parse(pool: &str) -> MinerResult<Self> {
        if let Some((host, port)) = pool.rsplit_once(':') {
            let port: u16 = port
                .parse()
                .map_err(|_| MinerError::Config(format!("invalid port in {pool}")))?;
            return Ok(Self {
                host: host.to_string(),
                port,
            });
        }
        Ok(Self {
            host: pool.to_string(),
            port: 3333,
        })
    }

    pub fn addr(&self) -> String {
        format!("{}:{}", self.host, self.port)
    }
}

#[derive(Debug)]
pub struct StratumClient {
    reader: BufReader<TcpStream>,
    writer: TcpStream,
    next_id: u64,
    pending: VecDeque<StratumEvent>,
    pub session: Option<SubscribeResult>,
    pub worker: String,
    pub agent: String,
    connected_at: Instant,
}

#[derive(Debug, Clone)]
pub enum StratumEvent {
    Notify(MiningNotify),
    NotifyRigor(MiningNotifyRigor),
    SetDifficulty(SetDifficulty),
    SubmitResult { id: u64, accepted: bool, message: Option<String> },
}

enum DispatchOutcome {
    Response(StratumResponse),
    Event(StratumEvent),
    Skip,
}

impl StratumClient {
    pub fn connect(endpoint: &PoolEndpoint) -> MinerResult<Self> {
        let stream = TcpStream::connect(&endpoint.addr())?;
        stream.set_read_timeout(Some(Duration::from_millis(250)))?;
        stream.set_write_timeout(Some(Duration::from_secs(30)))?;
        let reader = BufReader::new(stream.try_clone()?);
        Ok(Self {
            reader,
            writer: stream,
            next_id: 1,
            pending: VecDeque::new(),
            session: None,
            worker: String::new(),
            agent: DEFAULT_AGENT.to_string(),
            connected_at: Instant::now(),
        })
    }

    pub fn uptime(&self) -> Duration {
        self.connected_at.elapsed()
    }

    pub fn subscribe(&mut self, agent: Option<&str>) -> MinerResult<SubscribeResult> {
        let agent = agent.unwrap_or(DEFAULT_AGENT);
        if !requires_opushash_agent(agent) {
            return Err(MinerError::Stratum(
                "agent string must contain opushash".into(),
            ));
        }
        self.agent = agent.to_string();
        let id = self.next_id();
        self.send_request(
            id,
            "mining.subscribe",
            json!([agent, null]),
        )?;
        let resp = self.read_response(id)?;
        if !resp.is_ok() {
            return Err(MinerError::Stratum(
                resp.error_message()
                    .unwrap_or_else(|| "subscribe failed".into()),
            ));
        }
        let session = SubscribeResult::from_response(
            resp.result.as_ref().ok_or_else(|| MinerError::Stratum("empty subscribe result".into()))?,
        )?;
        self.session = Some(session.clone());
        Ok(session)
    }

    pub fn authorize(&mut self, worker: &str, password: &str) -> MinerResult<bool> {
        self.worker = worker.to_string();
        let id = self.next_id();
        self.send_request(id, "mining.authorize", json!([worker, password]))?;
        let resp = self.read_response(id)?;
        if !resp.is_ok() {
            return Ok(false);
        }
        Ok(resp.result.as_ref().and_then(|v| v.as_bool()).unwrap_or(false))
    }

    pub fn submit_share(
        &mut self,
        worker: &str,
        job_id: &str,
        extranonce2_hex: &str,
        ntime_hex: &str,
        nonce_hex: &str,
        metrics: &Metrics,
    ) -> MinerResult<bool> {
        metrics.record_share_submitted();
        let id = self.next_id();
        self.send_request(
            id,
            "mining.submit",
            json!([worker, job_id, extranonce2_hex, ntime_hex, nonce_hex]),
        )?;
        let resp = self.read_response(id)?;
        let accepted = resp.is_ok() && resp.result.as_ref().and_then(|v| v.as_bool()) == Some(true);
        if accepted {
            metrics.record_share_accepted();
        } else {
            metrics.record_share_rejected();
        }
        Ok(accepted)
    }

    /// Read one server push or response line and map to events.
    pub fn read_event(&mut self) -> MinerResult<Option<StratumEvent>> {
        if let Some(ev) = self.pending.pop_front() {
            return Ok(Some(ev));
        }
        let line = match self.read_line()? {
            Some(l) => l,
            None => return Ok(None),
        };
        self.dispatch_line(&line, None)
    }

    fn parse_push(&self, method: String, params: Value) -> MinerResult<StratumEvent> {
        match method.as_str() {
            "mining.notify" => Ok(StratumEvent::Notify(MiningNotify::parse(&params)?)),
            "mining.notify.rigor" => Ok(StratumEvent::NotifyRigor(MiningNotifyRigor::parse(&params)?)),
            "mining.set_difficulty" => Ok(StratumEvent::SetDifficulty(SetDifficulty::parse(&params)?)),
            other => Err(MinerError::Stratum(format!("unsupported push method {other}"))),
        }
    }

    fn next_id(&mut self) -> u64 {
        let id = self.next_id;
        self.next_id += 1;
        id
    }

    fn send_request(&mut self, id: u64, method: &str, params: Value) -> MinerResult<()> {
        let line = build_request(id, method, params)?;
        self.writer.write_all(line.as_bytes())?;
        self.writer.flush()?;
        Ok(())
    }

    fn read_response(&mut self, expect_id: u64) -> MinerResult<StratumResponse> {
        loop {
            let line = self
                .read_line()?
                .ok_or_else(|| MinerError::Stratum("connection closed".into()))?;
            match self.dispatch_message(&parse_line(&line)?, Some(expect_id))? {
                DispatchOutcome::Response(resp) => return Ok(resp),
                DispatchOutcome::Event(ev) => self.pending.push_back(ev),
                DispatchOutcome::Skip => {}
            }
        }
    }

    fn dispatch_line(
        &mut self,
        line: &str,
        expect_id: Option<u64>,
    ) -> MinerResult<Option<StratumEvent>> {
        match self.dispatch_message(&parse_line(line)?, expect_id)? {
            DispatchOutcome::Response(_) => Ok(None),
            DispatchOutcome::Event(ev) => Ok(Some(ev)),
            DispatchOutcome::Skip => Ok(None),
        }
    }

    fn dispatch_message(
        &mut self,
        msg: &StratumMessage,
        expect_id: Option<u64>,
    ) -> MinerResult<DispatchOutcome> {
        if let Some(method) = &msg.method {
            let ev = self.parse_push(method.clone(), msg.params.clone().unwrap_or(Value::Null))?;
            return Ok(DispatchOutcome::Event(ev));
        }
        if let Some(id) = msg.id.as_ref().and_then(|v| v.as_u64()) {
            let resp = StratumResponse {
                id: msg.id.clone(),
                result: msg.result.clone(),
                error: msg.error.clone(),
            };
            if expect_id == Some(id) {
                return Ok(DispatchOutcome::Response(resp));
            }
            return Ok(DispatchOutcome::Event(StratumEvent::SubmitResult {
                id,
                accepted: resp.is_ok() && resp.result.as_ref().and_then(|v| v.as_bool()) == Some(true),
                message: resp.error_message(),
            }));
        }
        if expect_id.is_some() && (msg.result.is_some() || msg.error.is_some()) {
            let resp = StratumResponse {
                id: msg.id.clone(),
                result: msg.result.clone(),
                error: msg.error.clone(),
            };
            return Ok(DispatchOutcome::Response(resp));
        }
        Ok(DispatchOutcome::Skip)
    }

    fn read_line(&mut self) -> MinerResult<Option<String>> {
        let mut line = String::new();
        match self.reader.read_line(&mut line) {
            Ok(0) => Ok(None),
            Ok(_) => Ok(Some(line)),
            Err(e)
                if e.kind() == std::io::ErrorKind::WouldBlock
                    || e.kind() == std::io::ErrorKind::TimedOut =>
            {
                Ok(None)
            }
            Err(e) => Err(MinerError::Io(e)),
        }
    }
}
