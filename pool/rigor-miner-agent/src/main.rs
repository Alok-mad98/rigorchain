use std::time::{Duration, Instant};

use clap::Parser;
use rigor_miner_agent::{AgentConfig, MinerAgent, MinerTelemetry};

#[derive(Parser, Debug)]
#[command(name = "rigor-miner-agent", about = "RIGOR pool CPU miner agent (TASK-MINING-023)")]
struct Cli {
    /// Pool address host:port (default port 3333)
    #[arg(long, default_value = "127.0.0.1:3333")]
    pool: String,

    /// Worker name for mining.authorize
    #[arg(long)]
    worker: String,

    /// Pool password
    #[arg(long, default_value = "x")]
    password: String,

    /// CPU mining threads (independent nonce partitions)
    #[arg(long, default_value_t = 1)]
    threads: u32,

    /// Print metrics snapshot and exit after handshake
    #[arg(long)]
    stats: bool,

    /// Status log interval seconds
    #[arg(long, default_value_t = 30)]
    log_interval_secs: u64,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let cli = Cli::parse();
    let config = AgentConfig::from_cli(&cli.pool, &cli.worker, &cli.password, cli.threads)?;
    let endpoint = config.pool.clone();

    eprintln!(
        "rigor-miner-agent connecting to {} as {} (threads={})",
        endpoint.addr(),
        cli.worker,
        cli.threads
    );

    let mut agent = MinerAgent::connect(config)?;
    agent.handshake()?;
    eprintln!("handshake OK (agent={})", agent.client.agent);

    if cli.stats {
        let snap = agent.metrics.snapshot();
        println!("{}", serde_json::to_string_pretty(&snap)?);
        return Ok(());
    }

    agent.start_mining()?;
    eprintln!("mining started with {} thread(s)", cli.threads);

    let log_every = Duration::from_secs(cli.log_interval_secs);
    let telemetry = MinerTelemetry::default();
    let mut last_log = Instant::now();
    let mut hashes_at_last_log = agent.metrics.hashes();

    loop {
        let _ = agent.poll_once();
        let _ = agent.drain_shares();

        if last_log.elapsed() >= log_every {
            let elapsed = last_log.elapsed().as_secs_f64();
            let hashrate = agent
                .metrics
                .hashrate_over(elapsed, hashes_at_last_log);
            let snap = agent.metrics.snapshot();
            eprintln!(
                "status worker={} hashrate={:.1} H/s hashes={} shares_found={} submitted={} accepted={} rejected={} stale_jobs={} uptime={}s",
                cli.worker,
                hashrate,
                snap.hashes,
                snap.shares_found,
                snap.shares_submitted,
                snap.shares_accepted,
                snap.shares_rejected,
                snap.stale_jobs,
                snap.uptime_secs
            );
            telemetry.record_snapshot(&cli.worker, hashrate, &snap);
            last_log = Instant::now();
            hashes_at_last_log = agent.metrics.hashes();
        }

        std::thread::sleep(Duration::from_millis(10));
    }
}
