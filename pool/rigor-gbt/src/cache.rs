use crate::error::{GbtError, GbtResult};
use crate::gbt::GbtTemplate;
use crate::record::{build_template_record, normalize_gbt_record, unix_now, TemplateRecord};
use crate::types::{RecordId, TemplateState, TemplateVersion, TipHash};
use crate::validate::validate_template;
use std::collections::HashMap;
use std::sync::{Arc, RwLock};

/// In-memory immutable template cache (TASK-MINING-005).
///
/// Records are never mutated in place; state transitions replace the entire `Arc<TemplateRecord>`.
pub struct TemplateCache {
    inner: RwLock<CacheInner>,
    /// TTL in seconds for ACTIVE → EXPIRED (0 disables auto-expiry).
    pub ttl_secs: u64,
    /// Retention delay before terminal states move to ARCHIVED.
    pub archive_delay_secs: u64,
}

struct CacheInner {
    records: HashMap<RecordId, Arc<TemplateRecord>>,
    /// Chronological ingest order (append-only).
    history: Vec<RecordId>,
    /// Current ACTIVE record id (at most one).
    active_id: Option<RecordId>,
    /// Dedup: longpollid → record_id for same-tip generations.
    longpoll_index: HashMap<String, RecordId>,
    /// Current observed tip hash from adapter.
    current_tip: Option<TipHash>,
}

impl TemplateCache {
    pub fn new() -> Self {
        Self::with_policy(300, 3600)
    }

    pub fn with_policy(ttl_secs: u64, archive_delay_secs: u64) -> Self {
        Self {
            inner: RwLock::new(CacheInner {
                records: HashMap::new(),
                history: Vec::new(),
                active_id: None,
                longpoll_index: HashMap::new(),
                current_tip: None,
            }),
            ttl_secs,
            archive_delay_secs,
        }
    }

    /// Ingest a validated GBT snapshot. Returns `None` on duplicate generation.
    ///
    /// Dedup key is `(tip_hash, longpollid)`. Core's `longpollid` is
    /// `tip_hash || nTransactionsUpdatedLast` — unchanged means the work package
    /// did not change. Do **not** require identical raw JSON / payload hash:
    /// `curtime` (and similar) churn on every poll and would otherwise republish
    /// a new Stratum job every interval, staling the previous one and flooding
    /// miners with `stale job` rejects.
    pub fn ingest(
        &self,
        template: GbtTemplate,
        raw_json: String,
        tip_hash: TipHash,
    ) -> GbtResult<Option<Arc<TemplateRecord>>> {
        let payload_hash = validate_template(&template, &raw_json)?;
        let gbt = normalize_gbt_record(template, raw_json, tip_hash.clone(), payload_hash);

        let mut inner = self
            .inner
            .write()
            .map_err(|e| GbtError::Cache(format!("lock poisoned: {e}")))?;

        if let Some(existing_id) = inner.longpoll_index.get(&gbt.longpollid).copied() {
            if let Some(existing) = inner.records.get(&existing_id) {
                if existing.tip_hash == tip_hash
                    && matches!(
                        existing.template_state,
                        TemplateState::Active | TemplateState::Stale
                    )
                {
                    return Ok(None);
                }
            }
        }

        // Same tip + same longpollid as the current ACTIVE (index miss after archive).
        if let Some(active_id) = inner.active_id {
            if let Some(active) = inner.records.get(&active_id) {
                if active.tip_hash == tip_hash && active.longpollid == gbt.longpollid {
                    return Ok(None);
                }
            }
        }

        let (parent_id, version) = self.resolve_lineage(&inner, &tip_hash, &gbt.longpollid);

        if inner.current_tip.as_ref() != Some(&tip_hash) {
            self.invalidate_on_tip_change(&mut inner, &tip_hash);
            inner.current_tip = Some(tip_hash.clone());
        } else if let Some(active_id) = inner.active_id {
            self.transition_record(&mut inner, active_id, TemplateState::Stale)?;
        }

        let record = build_template_record(gbt, parent_id, version, TemplateState::Active);
        let record_id = record.record_id;
        let arc = Arc::new(record);

        inner.records.insert(record_id, arc.clone());
        inner.history.push(record_id);
        inner.active_id = Some(record_id);
        inner
            .longpoll_index
            .insert(arc.longpollid.clone(), record_id);

        Ok(Some(arc))
    }

    /// Handle explicit tip change / reorg from adapter (`TipChanged` event).
    pub fn handle_tip_changed(&self, new_tip: TipHash) -> GbtResult<Vec<RecordId>> {
        let mut inner = self
            .inner
            .write()
            .map_err(|e| GbtError::Cache(format!("lock poisoned: {e}")))?;

        let invalidated = self.invalidate_on_tip_change(&mut inner, &new_tip);
        inner.current_tip = Some(new_tip);
        inner.active_id = None;
        Ok(invalidated)
    }

    /// Apply TTL expiry to ACTIVE records past `ttl_secs`.
    pub fn apply_ttl_expiry(&self) -> GbtResult<Vec<RecordId>> {
        if self.ttl_secs == 0 {
            return Ok(Vec::new());
        }
        let mut inner = self
            .inner
            .write()
            .map_err(|e| GbtError::Cache(format!("lock poisoned: {e}")))?;

        let now = unix_now();
        let mut expired = Vec::new();

        if let Some(active_id) = inner.active_id {
            if let Some(rec) = inner.records.get(&active_id) {
                if rec.template_state == TemplateState::Active
                    && now.saturating_sub(rec.state_changed_at) >= self.ttl_secs
                {
                    self.transition_record(&mut inner, active_id, TemplateState::Expired)?;
                    expired.push(active_id);
                    inner.active_id = None;
                }
            }
        }
        Ok(expired)
    }

    /// Move eligible terminal records to ARCHIVED after retention delay.
    pub fn apply_archival(&self) -> GbtResult<Vec<RecordId>> {
        let mut inner = self
            .inner
            .write()
            .map_err(|e| GbtError::Cache(format!("lock poisoned: {e}")))?;

        let now = unix_now();
        let mut archived = Vec::new();
        let ids: Vec<RecordId> = inner.records.keys().copied().collect();

        for id in ids {
            let should_archive = inner.records.get(&id).map(|r| {
                matches!(
                    r.template_state,
                    TemplateState::Stale | TemplateState::Invalidated | TemplateState::Expired
                ) && now.saturating_sub(r.state_changed_at) >= self.archive_delay_secs
            });
            if should_archive == Some(true) {
                self.transition_record(&mut inner, id, TemplateState::Archived)?;
                archived.push(id);
                if inner.active_id == Some(id) {
                    inner.active_id = None;
                }
            }
        }
        Ok(archived)
    }

    pub fn get_active_template(&self) -> GbtResult<Option<Arc<TemplateRecord>>> {
        let inner = self
            .inner
            .read()
            .map_err(|e| GbtError::Cache(format!("lock poisoned: {e}")))?;
        Ok(inner
            .active_id
            .and_then(|id| inner.records.get(&id).cloned())
            .filter(|r| r.template_state == TemplateState::Active))
    }

    pub fn get_template_by_id(&self, id: RecordId) -> GbtResult<Option<Arc<TemplateRecord>>> {
        let inner = self
            .inner
            .read()
            .map_err(|e| GbtError::Cache(format!("lock poisoned: {e}")))?;
        Ok(inner.records.get(&id).cloned())
    }

    pub fn get_template_history(&self) -> GbtResult<Vec<Arc<TemplateRecord>>> {
        let inner = self
            .inner
            .read()
            .map_err(|e| GbtError::Cache(format!("lock poisoned: {e}")))?;
        Ok(inner
            .history
            .iter()
            .filter_map(|id| inner.records.get(id).cloned())
            .collect())
    }

    pub fn current_tip(&self) -> GbtResult<Option<TipHash>> {
        let inner = self
            .inner
            .read()
            .map_err(|e| GbtError::Cache(format!("lock poisoned: {e}")))?;
        Ok(inner.current_tip.clone())
    }

    fn resolve_lineage(
        &self,
        inner: &CacheInner,
        tip_hash: &TipHash,
        _longpollid: &str,
    ) -> (Option<RecordId>, TemplateVersion) {
        if inner.current_tip.as_ref() != Some(tip_hash) {
            return (None, TemplateVersion::INITIAL);
        }
        if let Some(active_id) = inner.active_id {
            if let Some(active) = inner.records.get(&active_id) {
                return (
                    Some(active_id),
                    active.template_version.next(),
                );
            }
        }
        (None, TemplateVersion::INITIAL)
    }

    fn invalidate_on_tip_change(
        &self,
        inner: &mut CacheInner,
        _new_tip: &TipHash,
    ) -> Vec<RecordId> {
        let mut invalidated = Vec::new();
        let ids: Vec<RecordId> = inner
            .records
            .iter()
            .filter(|(_, r)| {
                matches!(
                    r.template_state,
                    TemplateState::Active | TemplateState::Stale
                )
            })
            .map(|(id, _)| *id)
            .collect();

        for id in ids {
            if let Ok(()) = self.transition_record(inner, id, TemplateState::Invalidated) {
                invalidated.push(id);
            }
        }
        if inner.active_id.map(|a| invalidated.contains(&a)).unwrap_or(false) {
            inner.active_id = None;
        }
        invalidated
    }

    fn transition_record(
        &self,
        inner: &mut CacheInner,
        id: RecordId,
        new_state: TemplateState,
    ) -> GbtResult<()> {
        let existing = inner
            .records
            .get(&id)
            .ok_or_else(|| GbtError::Cache(format!("record {id} not found")))?
            .clone();

        if existing.template_state == new_state {
            return Ok(());
        }

        Self::assert_transition_allowed(existing.template_state, new_state)?;

        let now = unix_now();
        let mut replacement = (*existing).clone();
        replacement.template_state = new_state;
        replacement.state_changed_at = now;

        inner.records.insert(id, Arc::new(replacement));
        Ok(())
    }

    fn assert_transition_allowed(from: TemplateState, to: TemplateState) -> GbtResult<()> {
        use TemplateState::*;
        let allowed = matches!(
            (from, to),
            (Active, Stale)
                | (Active, Invalidated)
                | (Active, Expired)
                | (Active, Archived)
                | (Stale, Invalidated)
                | (Stale, Archived)
                | (Invalidated, Archived)
                | (Expired, Archived)
        );
        if allowed || from == to {
            Ok(())
        } else {
            Err(GbtError::Cache(format!(
                "illegal state transition {:?} -> {:?}",
                from, to
            )))
        }
    }
}

impl Default for TemplateCache {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn sample_raw(longpoll: &str) -> String {
        format!(
            r#"{{
            "previousblockhash": "0000000000000000000123456789abcdef0123456789abcdef0123456789abcd",
            "transactions": [{{
                "data": "0100000001",
                "txid": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            }}],
            "coinbasevalue": 12800000000,
            "longpollid": "{longpoll}",
            "weightlimit": 1000000,
            "bits": "1d00ffff",
            "prev_nbits": "1c00ff00",
            "height": 100
        }}"#
        )
    }

    fn tip_a() -> TipHash {
        TipHash("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc".into())
    }

    fn tip_b() -> TipHash {
        TipHash("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd".into())
    }

    #[test]
    fn ingest_creates_active_record() {
        let cache = TemplateCache::new();
        let raw = sample_raw("lp/1");
        let template: GbtTemplate = serde_json::from_str(&raw).unwrap();
        let rec = cache.ingest(template, raw, tip_a()).unwrap().unwrap();
        assert_eq!(rec.template_state, TemplateState::Active);
        assert_eq!(rec.template_version, TemplateVersion::INITIAL);
        assert!(cache.get_active_template().unwrap().is_some());
    }

    #[test]
    fn duplicate_longpollid_deduped() {
        let cache = TemplateCache::new();
        let raw = sample_raw("lp/1");
        let template: GbtTemplate = serde_json::from_str(&raw).unwrap();
        assert!(cache.ingest(template.clone(), raw.clone(), tip_a()).unwrap().is_some());
        assert!(cache.ingest(template, raw, tip_a()).unwrap().is_none());
        assert_eq!(cache.get_template_history().unwrap().len(), 1);
    }

    #[test]
    fn same_longpollid_different_curtime_is_deduped() {
        let cache = TemplateCache::new();
        let raw1 = sample_raw("lp/stable");
        let t1: GbtTemplate = serde_json::from_str(&raw1).unwrap();
        assert!(cache.ingest(t1, raw1, tip_a()).unwrap().is_some());

        // Simulate Core returning the same longpollid with a changed curtime field
        // (payload hash differs; generation id does not).
        let raw2 = sample_raw("lp/stable").replacen(
            "\"height\": 100",
            "\"height\": 100, \"curtime\": 9999999999",
            1,
        );
        let t2: GbtTemplate = serde_json::from_str(&raw2).unwrap();
        assert!(
            cache.ingest(t2, raw2, tip_a()).unwrap().is_none(),
            "curtime-only churn must not create a new template/job"
        );
        assert_eq!(cache.get_template_history().unwrap().len(), 1);
    }

    #[test]
    fn newer_template_version_stales_previous() {
        let cache = TemplateCache::new();
        let raw1 = sample_raw("lp/1");
        let t1: GbtTemplate = serde_json::from_str(&raw1).unwrap();
        let r1 = cache.ingest(t1, raw1, tip_a()).unwrap().unwrap();

        let raw2 = sample_raw("lp/2");
        let t2: GbtTemplate = serde_json::from_str(&raw2).unwrap();
        let r2 = cache.ingest(t2, raw2, tip_a()).unwrap().unwrap();

        assert_eq!(r2.template_version, TemplateVersion(2));
        assert_eq!(r2.parent_record_id, Some(r1.record_id));

        let old = cache.get_template_by_id(r1.record_id).unwrap().unwrap();
        assert_eq!(old.template_state, TemplateState::Stale);
        assert_eq!(cache.get_active_template().unwrap().unwrap().record_id, r2.record_id);
    }

    #[test]
    fn tip_change_invalidates_active_and_stale() {
        let cache = TemplateCache::new();
        let raw1 = sample_raw("lp/1");
        let t1: GbtTemplate = serde_json::from_str(&raw1).unwrap();
        let r1 = cache.ingest(t1, raw1, tip_a()).unwrap().unwrap();

        let raw2 = sample_raw("lp/2");
        let t2: GbtTemplate = serde_json::from_str(&raw2).unwrap();
        let r2 = cache.ingest(t2, raw2, tip_a()).unwrap().unwrap();

        let invalidated = cache.handle_tip_changed(tip_b()).unwrap();
        assert!(invalidated.contains(&r1.record_id));
        assert!(invalidated.contains(&r2.record_id));

        let s1 = cache.get_template_by_id(r1.record_id).unwrap().unwrap();
        let s2 = cache.get_template_by_id(r2.record_id).unwrap().unwrap();
        assert_eq!(s1.template_state, TemplateState::Invalidated);
        assert_eq!(s2.template_state, TemplateState::Invalidated);
        assert!(cache.get_active_template().unwrap().is_none());
    }

    #[test]
    fn reorg_then_new_template_resets_version() {
        let cache = TemplateCache::new();
        let raw = sample_raw("lp/1");
        let t: GbtTemplate = serde_json::from_str(&raw).unwrap();
        cache.ingest(t.clone(), raw.clone(), tip_a()).unwrap();

        cache.handle_tip_changed(tip_b()).unwrap();

        let raw_new = sample_raw("lp/reorg/1");
        let t_new: GbtTemplate = serde_json::from_str(&raw_new).unwrap();
        let r = cache.ingest(t_new, raw_new, tip_b()).unwrap().unwrap();
        assert_eq!(r.template_version, TemplateVersion::INITIAL);
        assert_eq!(r.template_state, TemplateState::Active);
    }

    #[test]
    fn ttl_expires_active() {
        let cache = TemplateCache::with_policy(1, 3600);
        let raw = sample_raw("lp/ttl");
        let t: GbtTemplate = serde_json::from_str(&raw).unwrap();
        let r = cache.ingest(t, raw, tip_a()).unwrap().unwrap();

        std::thread::sleep(std::time::Duration::from_secs(2));
        let expired = cache.apply_ttl_expiry().unwrap();
        assert_eq!(expired, vec![r.record_id]);

        let updated = cache.get_template_by_id(r.record_id).unwrap().unwrap();
        assert_eq!(updated.template_state, TemplateState::Expired);
        assert!(cache.get_active_template().unwrap().is_none());
    }

    #[test]
    fn archival_moves_stale_to_archived() {
        let cache = TemplateCache::with_policy(300, 0);
        let raw1 = sample_raw("lp/a1");
        let t1: GbtTemplate = serde_json::from_str(&raw1).unwrap();
        let r1 = cache.ingest(t1, raw1, tip_a()).unwrap().unwrap();

        let raw2 = sample_raw("lp/a2");
        let t2: GbtTemplate = serde_json::from_str(&raw2).unwrap();
        cache.ingest(t2, raw2, tip_a()).unwrap();

        let archived = cache.apply_archival().unwrap();
        assert!(archived.contains(&r1.record_id));
        let rec = cache.get_template_by_id(r1.record_id).unwrap().unwrap();
        assert_eq!(rec.template_state, TemplateState::Archived);
    }

    #[test]
    fn records_are_replaced_not_mutated() {
        let cache = TemplateCache::new();
        let raw1 = sample_raw("lp/immut");
        let t1: GbtTemplate = serde_json::from_str(&raw1).unwrap();
        let r1 = cache.ingest(t1, raw1, tip_a()).unwrap().unwrap();
        let state_at_ingest = r1.template_state;

        let raw2 = sample_raw("lp/immut2");
        let t2: GbtTemplate = serde_json::from_str(&raw2).unwrap();
        cache.ingest(t2, raw2, tip_a()).unwrap();

        assert_eq!(r1.template_state, state_at_ingest);
        let current = cache.get_template_by_id(r1.record_id).unwrap().unwrap();
        assert_eq!(current.template_state, TemplateState::Stale);
        assert!(!Arc::ptr_eq(&r1, &current));
    }
}
