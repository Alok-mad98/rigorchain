use crate::cache::JobCache;
use crate::error::AssemblerResult;
use crate::types::{JobId, PublishedJobSnapshot};
use std::sync::Arc;

/// Read-only JobSnapshot API .
pub struct JobSnapshotApi {
    cache: Arc<JobCache>,
}

impl JobSnapshotApi {
    pub fn new(cache: Arc<JobCache>) -> Self {
        Self { cache }
    }

    pub fn get_active_jobs(&self) -> AssemblerResult<Vec<Arc<PublishedJobSnapshot>>> {
        self.cache.get_active_jobs()
    }

    pub fn get_job_by_id(&self, id: &JobId) -> AssemblerResult<Option<Arc<PublishedJobSnapshot>>> {
        self.cache.get_job_by_id(id)
    }

    pub fn get_job_history(&self) -> AssemblerResult<Vec<Arc<PublishedJobSnapshot>>> {
        self.cache.get_job_history()
    }

    pub fn inner(&self) -> &Arc<JobCache> {
        &self.cache
    }
}
