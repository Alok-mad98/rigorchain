use crate::cache::TemplateCache;
use crate::error::GbtResult;
use crate::record::TemplateRecord;
use crate::types::RecordId;
use std::sync::Arc;

/// Read-only Template Cache API (§6).
pub struct TemplateCacheApi {
    cache: Arc<TemplateCache>,
}

impl TemplateCacheApi {
    pub fn new(cache: Arc<TemplateCache>) -> Self {
        Self { cache }
    }

    /// `GetActiveTemplate()` — current ACTIVE template, if any.
    pub fn get_active_template(&self) -> GbtResult<Option<Arc<TemplateRecord>>> {
        self.cache.get_active_template()
    }

    /// `GetTemplateById(record_id)` — any cached record by primary key.
    pub fn get_template_by_id(&self, id: RecordId) -> GbtResult<Option<Arc<TemplateRecord>>> {
        self.cache.get_template_by_id(id)
    }

    /// `GetTemplateHistory()` — chronological ingest order (all states).
    pub fn get_template_history(&self) -> GbtResult<Vec<Arc<TemplateRecord>>> {
        self.cache.get_template_history()
    }

    pub fn inner(&self) -> &Arc<TemplateCache> {
        &self.cache
    }
}

impl From<Arc<TemplateCache>> for TemplateCacheApi {
    fn from(cache: Arc<TemplateCache>) -> Self {
        Self::new(cache)
    }
}
