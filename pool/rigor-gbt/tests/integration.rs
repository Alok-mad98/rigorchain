use rigor_gbt::{
    GbtTemplate, RecordId, TemplateCache, TemplateCacheApi, TemplateState, TemplateVersion,
    TipHash, validate_template,
};
use std::sync::Arc;

fn fixture(longpoll: &str) -> (GbtTemplate, String) {
    let raw = format!(
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
            "height": 200
        }}"#
    );
    let template: GbtTemplate = serde_json::from_str(&raw).unwrap();
    (template, raw)
}

fn tip(label: u8) -> TipHash {
    let mut bytes = [0u8; 32];
    bytes[31] = label;
    TipHash(hex::encode(bytes))
}

#[test]
fn read_api_exposes_active_and_history() {
    let cache = Arc::new(TemplateCache::new());
    let api = TemplateCacheApi::new(cache.clone());

    let (t, raw) = fixture("api/1");
    let rec = cache.ingest(t, raw, tip(0xa)).unwrap().unwrap();

    let active = api.get_active_template().unwrap().unwrap();
    assert_eq!(active.record_id, rec.record_id);

    let by_id = api.get_template_by_id(rec.record_id).unwrap().unwrap();
    assert_eq!(by_id.height, 200);

    let history = api.get_template_history().unwrap();
    assert_eq!(history.len(), 1);
}

#[test]
fn validation_rejects_before_cache_touch() {
    let (t, raw) = fixture("bad/1");
    // V-06: lottery GBT keys on the raw payload fail ingest.
    let raw = raw.replacen(
        "\"height\": 200",
        "\"height\": 200, \"convergence_root\": \"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"",
        1,
    );
    let err = validate_template(&t, &raw);
    assert!(err.is_err());
}

#[test]
fn reorg_sequence_invalidates_then_accepts_new_tip() {
    let cache = Arc::new(TemplateCache::new());
    let (t1, raw1) = fixture("reorg/1");
    let r1 = cache.ingest(t1, raw1, tip(0xa)).unwrap().unwrap();

    cache.handle_tip_changed(tip(0xb)).unwrap();

    let stale = cache.get_template_by_id(r1.record_id).unwrap().unwrap();
    assert_eq!(stale.template_state, TemplateState::Invalidated);

    let (t2, raw2) = fixture("reorg/2");
    let r2 = cache.ingest(t2, raw2, tip(0xb)).unwrap().unwrap();
    assert_eq!(r2.template_version, TemplateVersion::INITIAL);
    assert_eq!(r2.template_state, TemplateState::Active);
}

#[test]
fn get_template_by_id_missing_returns_none() {
    let cache = Arc::new(TemplateCache::new());
    let api = TemplateCacheApi::new(cache);
    assert!(api.get_template_by_id(RecordId::new_v4()).unwrap().is_none());
}
