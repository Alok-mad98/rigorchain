use crate::cache::TemplateCache;
use crate::error::{GbtError, GbtResult};
use crate::gbt::GbtTemplate;
use crate::record::TemplateRecord;
use crate::rpc::RigordRpc;
use crate::types::TipHash;
use std::sync::Arc;
use std::time::Duration;

/// GBT adapter: polls `rigord`, validates, normalizes, and hands off to Template Cache.
pub struct GbtAdapter {
    rpc: RigordRpc,
    cache: Arc<TemplateCache>,
    last_longpollid: Option<String>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum PollEvent {
    TemplateCreated(Arc<TemplateRecord>),
    TemplateDeduped,
    TipChanged { old_tip: Option<TipHash>, new_tip: TipHash },
    SkippedIbd,
    Rejected(String),
}

impl GbtAdapter {
    pub fn new(rpc: RigordRpc, cache: Arc<TemplateCache>) -> Self {
        Self {
            rpc,
            cache,
            last_longpollid: None,
        }
    }

    pub fn cache(&self) -> &Arc<TemplateCache> {
        &self.cache
    }

    pub fn rpc(&self) -> &RigordRpc {
        &self.rpc
    }

    /// Single poll cycle: auxiliary RPCs + `getblocktemplate` + cache ingest.
    pub fn poll_once(&mut self) -> GbtResult<PollEvent> {
        let chain_info = self.rpc.get_blockchain_info()?;
        if chain_info.initialblockdownload == Some(true) {
            return Ok(PollEvent::SkippedIbd);
        }

        let tip_hash = TipHash(self.rpc.get_best_block_hash()?);
        let old_tip = self.cache.current_tip()?;

        let tip_changed = if old_tip.as_ref() != Some(&tip_hash) && old_tip.is_some() {
            self.cache.handle_tip_changed(tip_hash.clone())?;
            self.last_longpollid = None;
            Some(PollEvent::TipChanged {
                old_tip,
                new_tip: tip_hash.clone(),
            })
        } else {
            None
        };

        // Interval poll only — never pass longpollid. Core longpoll holds an HTTP
        // worker until tip/mempool change and does not abort on client disconnect;
        // with a short poll interval that soft-deadlocks rigord RPC (LISTEN but no
        // responses). Solo cpuminer uses longpoll differently (few connections);
        // the pool poller must stay non-blocking.
        let (value, raw) = self.rpc.get_block_template(None)?;

        let template = GbtTemplate::from_json_value(&value)
            .map_err(|e| GbtError::Rejected(format!("GBT parse error: {e}")))?;

        match self.cache.ingest(template, raw, tip_hash) {
            Ok(Some(record)) => {
                self.last_longpollid = Some(record.longpollid.clone());
                Ok(PollEvent::TemplateCreated(record))
            }
            Ok(None) => {
                if let Some(ev) = tip_changed {
                    Ok(ev)
                } else {
                    Ok(PollEvent::TemplateDeduped)
                }
            }
            Err(GbtError::Validation { rule, message }) => {
                Ok(PollEvent::Rejected(format!("{rule}: {message}")))
            }
            Err(GbtError::Rejected(msg)) => Ok(PollEvent::Rejected(msg)),
            Err(e) => Err(e),
        }
    }

    /// Blocking poll loop with interval (for standalone service binary).
    pub fn run_poll_loop(&mut self, interval: Duration) -> GbtResult<()> {
        loop {
            match self.poll_once() {
                Ok(_) | Err(GbtError::Rpc(_)) | Err(GbtError::Http(_)) => {}
                Err(e) => return Err(e),
            }
            std::thread::sleep(interval);
        }
    }
}
