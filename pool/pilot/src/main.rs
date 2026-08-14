//! `rigor-pool` / `rigor-pool-pilot` — Alpha ORU-only mining pool binary.
//!
//! Responsibilities: Stratum coordination, share validation, block assembly,
//! submitblock, ORU payout accounting.
//! Does NOT create / trigger / allocate / simulate RES mint.

use std::io::Write;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use clap::{Parser, Subcommand};
use rigor_pool_pilot::{
    load_resolved, OruPayoutStatus, PilotStatusReport, PoolPilot, PoolPilotConfig, PoolProfile,
};

#[derive(Parser, Debug)]
#[command(
    name = "rigor-pool",
    about = "RIGOR Alpha mining pool (ORU-only; no pool RES mint)"
)]
struct Cli {
    /// Path to pool TOML config (see pool/config/example.toml)
    #[arg(long, short = 'c', global = true)]
    config: Option<PathBuf>,

    /// Override status JSON path for `status` (default: config / PILOT_STATUS_FILE)
    #[arg(long, global = true)]
    status_file: Option<PathBuf>,

    #[command(subcommand)]
    command: Option<Commands>,
}

#[derive(Subcommand, Debug)]
enum Commands {
    /// Run the pool (default when no subcommand is given)
    Run,
    /// Print pool status (miners, hashrate proxy, shares, blocks, ORU payout)
    Status,
}

fn main() {
    if let Err(e) = run() {
        eprintln!("rigor-pool error: {e}");
        std::process::exit(1);
    }
}

fn run() -> rigor_pool_pilot::PilotResult<()> {
    let cli = Cli::parse();
    let cmd = cli.command.unwrap_or(Commands::Run);

    match cmd {
        Commands::Status => cmd_status(cli.config.as_deref(), cli.status_file.as_deref()),
        Commands::Run => cmd_run(cli.config.as_deref()),
    }
}

fn cmd_status(
    config_path: Option<&std::path::Path>,
    status_override: Option<&std::path::Path>,
) -> rigor_pool_pilot::PilotResult<()> {
    let resolved = load_resolved(config_path)?;
    let path = status_override
        .map(|p| p.to_path_buf())
        .unwrap_or_else(|| PathBuf::from(&resolved.status_file));

    let raw = std::fs::read_to_string(&path).map_err(|e| {
        rigor_pool_pilot::PilotError::Config(format!(
            "cannot read status file {}: {e}\n\
             hint: start the pool first, e.g.\n\
               rigor-pool --config pool/config/example.devtest.toml run\n\
             (status.interval_secs must be > 0; default example writes {})",
            path.display(),
            resolved.status_file
        ))
    })?;
    let report: PilotStatusReport = serde_json::from_str(&raw).map_err(|e| {
        rigor_pool_pilot::PilotError::Config(format!("invalid status JSON {}: {e}", path.display()))
    })?;
    print_status(&report, &path);
    Ok(())
}

fn print_status(report: &PilotStatusReport, path: &std::path::Path) {
    let OruPayoutStatus {
        fee_percent,
        payout_interval_secs,
        total_liability_oru,
        pool_fee_retained_oru,
        pending_workers,
        rounds_finalized,
        active_share_count,
        res_involvement,
    } = &report.oru_payout;

    println!("=== RIGOR Pool Status (ORU-only) ===");
    println!("status_file:           {}", path.display());
    println!("miners connected:      {}", report.miners_connected);
    println!(
        "hashrate (diff proxy): {:.3}  (prefer worker agent --stats for exact H/s)",
        report.hashrate_estimate_hs
    );
    println!("shares accepted:       {}", report.shares_accepted);
    println!(
        "shares rejected:       {}",
        report.metrics.shares_rejected
    );
    println!("blocks found:          {}", report.blocks_found);
    println!(
        "blocks submitted:      {}",
        report.metrics.blocks_submitted
    );
    println!(
        "blocks submit failed:  {}",
        report.metrics.blocks_submit_failed
    );
    println!("active jobs:           {}", report.active_jobs);
    println!(
        "current tip:           {}",
        report.current_tip.as_deref().unwrap_or("(none)")
    );
    println!("--- ORU payout ---");
    println!("fee_percent:           {fee_percent}");
    println!("payout_interval_secs:  {payout_interval_secs}");
    println!("total_liability_oru:   {total_liability_oru}");
    println!("pool_fee_retained_oru: {pool_fee_retained_oru}");
    println!("pending_workers:       {pending_workers}");
    println!("rounds_finalized:      {rounds_finalized}");
    println!("active_share_count:    {active_share_count}");
    println!("RES involvement:       {res_involvement}");
    println!(
        "res_mint_events:       {} (must remain 0)",
        report.metrics.res_mint_events
    );
}

fn cmd_run(config_path: Option<&std::path::Path>) -> rigor_pool_pilot::PilotResult<()> {
    let resolved = load_resolved(config_path)?;
    resolved.export_security_env();

    let config: PoolPilotConfig = resolved.pilot.clone();
    let profile = PoolProfile::from_env();
    eprintln!(
        "rigor-pool: rpc={} stratum={} profile={:?} fee={}% (ORU-only; no pool RES mint)",
        config.rpc_url, config.stratum_listen, profile, config.oru_fee_percent
    );

    let pilot = match profile {
        PoolProfile::Devtest => PoolPilot::devtest_in_process(config)?,
        PoolProfile::Production => PoolPilot::new(config).map_err(|e| {
            rigor_pool_pilot::PilotError::Config(format!(
                "{e}\n\
                 hint: for local Alpha/regtest use:\n\
                   rigor-pool --config pool/config/example.devtest.toml run\n\
                 for production set [payout].payout_script_hex and [security].worker_credentials \
                 (or RIGOR_POOL_PAYOUT_SCRIPT_HEX / RIGOR_POOL_WORKER_CREDENTIALS)"
            ))
        })?,
    };
    let shared = Arc::new(Mutex::new(pilot));
    PoolPilot::spawn_gbt_poll_thread(shared.clone());

    let status_interval = resolved.status_interval_secs;
    let status_file = resolved.status_file.clone();
    if status_interval > 0 {
        let status_pilot = shared.clone();
        let path = status_file.clone();
        std::thread::spawn(move || loop {
            if let Ok(p) = status_pilot.lock() {
                if let Ok(report) = p.status_report() {
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
            std::thread::sleep(std::time::Duration::from_secs(status_interval));
        });
    }

    let (orchestrator, stratum_listen) = {
        let guard = shared.lock().map_err(|_| {
            rigor_pool_pilot::PilotError::Config("pilot lock poisoned".into())
        })?;
        (
            guard.orchestrator.clone(),
            guard.config.stratum_listen.clone(),
        )
    };

    // Write one status snapshot immediately so `status` works without waiting a full interval.
    if status_interval > 0 {
        if let Ok(p) = shared.lock() {
            if let Ok(report) = p.status_report() {
                if let Some(parent) = std::path::Path::new(&status_file).parent() {
                    let _ = std::fs::create_dir_all(parent);
                }
                if let Ok(json) = serde_json::to_string_pretty(&report) {
                    if let Ok(mut f) = std::fs::File::create(&status_file) {
                        let _ = f.write_all(json.as_bytes());
                    }
                }
            }
        }
        eprintln!(
            "rigor-pool: status file {} (interval={}s); query with: rigor-pool status",
            status_file, status_interval
        );
    }

    eprintln!(
        "rigor-pool: stratum listening on {} (foreground — Ctrl+C to stop; use another terminal for miner/status)",
        stratum_listen
    );
    let server = stratum_server::StratumServer::new(orchestrator, &stratum_listen);
    server.run().map_err(rigor_pool_pilot::PilotError::from)
}
