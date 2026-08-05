use std::net::TcpListener;
use std::sync::Arc;
use std::thread;
use std::time::Duration;

use rigor_gbt::{GbtTemplate, TemplateCache};
use rigor_miner_agent::{MinerAgent, PoolEndpoint, StratumClient, DEFAULT_AGENT};
use stratum_server::{PayoutEngine, StratumOrchestrator, StratumServer};

fn fixture(longpoll: &str) -> (GbtTemplate, String) {
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{"data": "0100000000010000000001", "txid": "{a}", "hash": "{a}"}},
                {{"data": "0100000000020000000001", "txid": "{b}", "hash": "{b}"}}
            ],
            "coinbasevalue": 5000000000,
            "longpollid": "{longpoll}",
            "weightlimit": 4000000,
            "bits": "207fffff",
            "prev_nbits": "1c00ff00",
            "height": 300,
            "version": 536870912,
            "curtime": 1700000300,
            "default_witness_commitment": "6a24aa21a9ed",
            "default_convergence_commitment": "6a2d",
            "convergence_txids": ["{a}", "{b}"]
        }}"#,
        prev = "00".repeat(32),
        a = "aa".repeat(32),
        b = "bb".repeat(32),
    );
    (serde_json::from_str(&raw).unwrap(), raw)
}

fn tip(b: u8) -> rigor_gbt::TipHash {
    let mut bytes = [0u8; 32];
    bytes[31] = b;
    rigor_gbt::TipHash(hex::encode(bytes))
}

fn stack() -> Arc<StratumOrchestrator> {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    Arc::new(StratumOrchestrator::new(
        templates,
        jobs,
        Arc::new(PayoutEngine::new(1)),
    ))
}

fn spawn_server(orch: Arc<StratumOrchestrator>) -> u16 {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let port = listener.local_addr().unwrap().port();
    drop(listener);
    let addr = format!("127.0.0.1:{port}");
    let server = StratumServer::new(orch, addr);
    thread::spawn(move || {
        let _ = server.run();
    });
    thread::sleep(Duration::from_millis(50));
    port
}

#[test]
fn stratum_handshake_subscribe_authorize() {
    let orch = stack();
    orch.sessions.register_worker("alice.worker1", "x");
    let port = spawn_server(orch);

    let endpoint = PoolEndpoint {
        host: "127.0.0.1".into(),
        port,
    };
    let mut client = StratumClient::connect(&endpoint).unwrap();
    let session = client.subscribe(Some(DEFAULT_AGENT)).unwrap();
    assert!(!session.extranonce1.is_empty());
    assert!(client.authorize("alice.worker1", "x").unwrap());
}

#[test]
fn share_submission_via_tcp() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let orch = Arc::new(StratumOrchestrator::new(
        templates.clone(),
        jobs.clone(),
        Arc::new(PayoutEngine::new(1)),
    ));

    let (t, raw) = fixture("miner-agent/1");
    templates.ingest(t, raw, tip(1)).unwrap();
    let envelopes = orch.publish_jobs_from_active_template().unwrap();
    assert_eq!(envelopes.len(), 1);

    orch.sessions.register_worker("bob.worker", "x");
    let port = spawn_server(orch);

    let config = rigor_miner_agent::AgentConfig::from_cli(
        &format!("127.0.0.1:{port}"),
        "bob.worker",
        "x",
        1,
    )
    .unwrap();
    let mut agent = MinerAgent::connect(config).unwrap();
    agent.handshake().unwrap();

    let job_id = &envelopes[0].job_id;
    let ntime = envelopes[0]
        .notify
        .params
        .as_array()
        .and_then(|a| a.get(7))
        .and_then(|v| v.as_str())
        .unwrap_or("65000000");

    let accepted = agent
        .client
        .submit_share(
            "bob.worker",
            job_id,
            "00000000",
            ntime,
            "00000001",
            &agent.metrics,
        )
        .unwrap();
    let snap = agent.metrics.snapshot();
    assert!(
        accepted || snap.shares_rejected >= 1,
        "expected accept or reject, got metrics {snap:?}"
    );
    assert!(snap.shares_submitted >= 1);
}

#[test]
fn cpu_mining_loop_hashes_after_notify() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let orch = Arc::new(StratumOrchestrator::new(
        templates.clone(),
        jobs.clone(),
        Arc::new(PayoutEngine::new(1)),
    ));

    let (t, raw) = fixture("miner-agent/mining/1");
    templates.ingest(t, raw, tip(1)).unwrap();
    let envelopes = orch.publish_jobs_from_active_template().unwrap();
    assert_eq!(envelopes.len(), 1);

    orch.sessions.register_worker("carol.worker", "x");
    let port = spawn_server(orch);

    let config = rigor_miner_agent::AgentConfig::from_cli(
        &format!("127.0.0.1:{port}"),
        "carol.worker",
        "x",
        2,
    )
    .unwrap();
    let mut agent = MinerAgent::connect(config).unwrap();
    agent.handshake().unwrap();
    agent.start_mining().unwrap();

    let deadline = std::time::Instant::now() + Duration::from_secs(3);
    while std::time::Instant::now() < deadline {
        let _ = agent.poll_once();
        let _ = agent.drain_shares();
        if agent.metrics.hashes() >= 200 {
            break;
        }
        thread::sleep(Duration::from_millis(20));
    }

    assert!(
        agent.metrics.hashes() >= 200,
        "expected mining threads to hash, got {}",
        agent.metrics.hashes()
    );
    assert!(
        agent.jobs.get(&envelopes[0].job_id).is_some(),
        "expected notify ingested into job cache"
    );
}

#[test]
fn rejects_non_opushash_agent_on_subscribe() {
    let orch = stack();
    let port = spawn_server(orch);
    let endpoint = PoolEndpoint {
        host: "127.0.0.1".into(),
        port,
    };
    let mut client = StratumClient::connect(&endpoint).unwrap();
    let err = client.subscribe(Some("bad-miner/1.0")).unwrap_err();
    assert!(err.to_string().contains("opushash"));
}
