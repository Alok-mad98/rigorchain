//! `rigor-pool-pilot` — closed testnet binary for the full RIGOR pool stack.

use std::io::Write;
use std::sync::{Arc, Mutex};

use rigor_pool_pilot::{PoolPilot, PoolPilotConfig};

fn main() {
    if let Err(e) = run() {
        eprintln!("rigor-pool-pilot error: {e}");
        std::process::exit(1);
    }
}

fn run() -> rigor_pool_pilot::PilotResult<()> {
    let config = PoolPilotConfig::from_env();
    eprintln!(
        "rigor-pool-pilot: rpc={} stratum={} mint_auto_submit={}",
        config.rpc_url, config.stratum_listen, config.mint_auto_submit
    );

    let pilot = PoolPilot::new(config)?;
    let shared = Arc::new(Mutex::new(pilot));
    rigor_pool_pilot::PoolPilot::spawn_gbt_poll_thread(shared.clone());

    if std::env::var("PILOT_STATUS_INTERVAL_SECS")
        .ok()
        .and_then(|s| s.parse::<u64>().ok())
        .is_some()
    {
        let status_pilot = shared.clone();
        std::thread::spawn(move || loop {
            std::thread::sleep(std::time::Duration::from_secs(30));
            if let Ok(p) = status_pilot.lock() {
                if let Ok(report) = p.status_report() {
                    let path = std::env::var("PILOT_STATUS_FILE").unwrap_or_else(|_| {
                        ".runtime/pool-pilot/status.json".to_string()
                    });
                    if let Some(parent) = std::path::Path::new(&path).parent() {
                        let _ = std::fs::create_dir_all(parent);
                    }
                    if let Ok(json) = serde_json::to_string_pretty(&report) {
                        if let Ok(mut f) = std::fs::File::create(&path) {
                            let _ = f.write_all(json.as_bytes());
                        }
                    }
                }
            }
        });
    }

    // Do not hold the pilot mutex while Stratum blocks — GBT poll thread needs it.
    let (orchestrator, stratum_listen) = {
        let guard = shared.lock().map_err(|_| {
            rigor_pool_pilot::PilotError::Config("pilot lock poisoned".into())
        })?;
        (
            guard.orchestrator.clone(),
            guard.config.stratum_listen.clone(),
        )
    };
    let server = stratum_server::StratumServer::new(orchestrator, &stratum_listen);
    server.run().map_err(rigor_pool_pilot::PilotError::from)
}
