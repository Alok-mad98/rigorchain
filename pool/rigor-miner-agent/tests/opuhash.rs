use rigor_miner_agent::{build_header_bytes, CpuOpuHashWorker, OpuHashWorker};
use share_validator::{get_opus_hash_from_base_hash, hash256, rgrpow_hash80};

#[test]
fn opuhash_golden_is_not_production_pow() {
    let mut base = [0u8; 32];
    base[0] = 1;
    let pow = get_opus_hash_from_base_hash(&base);
    let mut expect =
        hex::decode("0cc4ff55b1598a19689b97bd06cdb7c6abe8799bfd5e1554fbe44b606c265780").unwrap();
    expect.reverse();
    assert_eq!(pow.as_slice(), expect.as_slice());
    assert_ne!(pow, rgrpow_hash80(&[0u8; 80]));
    let _ = hash256(&[0u8; 80]);
}

#[test]
fn cpu_worker_hashes_header_with_rgrpow01() {
    let notify = rigor_miner_agent::MiningNotify {
        job_id: "t".into(),
        prevhash: "00".repeat(32),
        coinb1: "01".into(),
        coinb2: "02".into(),
        merkle_branch: vec![],
        version: "20000000".into(),
        nbits: "207fffff".into(),
        ntime: "65000000".into(),
        clean_jobs: false,
    };
    let header = build_header_bytes(&notify, &[0xaa], &[0, 0, 0, 1], 0x6500_0000, 7).unwrap();
    let worker = CpuOpuHashWorker;
    let pow = worker.hash_header(&header);
    assert_eq!(pow, rgrpow_hash80(&header));
    assert_ne!(pow, [0u8; 32]);
}
