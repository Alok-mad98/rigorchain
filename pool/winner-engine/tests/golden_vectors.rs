use winner_engine::{
    compute_convergence_root_with_encoding, compute_sigma_hash, convergence_draw_inequality_holds,
    draw_input_from_golden, hash_hex_to_native, replay_convergence_draw, TxidEncoding,
};

fn load_vectors() -> serde_json::Value {
    let path = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../formal_verification/phase3_golden_vectors.json"
    );
    let raw = std::fs::read_to_string(path).expect("phase3_golden_vectors.json");
    serde_json::from_str(&raw).expect("parse golden json")
}

#[test]
fn convergence_root_golden_vectors() {
    let data = load_vectors();
    for v in data["vectors"].as_array().expect("vectors array") {
        if v["module"] != "ConvergenceRoot" {
            continue;
        }
        let txids: Vec<String> = v["input"]["txids_hex"]
            .as_array()
            .unwrap_or(&vec![])
            .iter()
            .map(|x| x.as_str().unwrap().to_string())
            .collect();
        let encoding = if v["input"].get("encoding").and_then(|e| e.as_str()) == Some("sethex") {
            TxidEncoding::SetHex
        } else {
            TxidEncoding::HashHex
        };
        let root = compute_convergence_root_with_encoding(&txids, encoding);
        let expected = v["expected"]["convergence_root"].as_str().unwrap();
        assert_eq!(hex::encode(root), expected, "vector {}", v["id"]);
    }
}

#[test]
fn sigma_pow01_golden_vectors() {
    let data = load_vectors();
    for v in data["vectors"].as_array().unwrap() {
        if v["module"] != "SigmaPoW01" {
            continue;
        }
        let inp = &v["input"];
        let txid = hash_hex_to_native(inp["txid_hex"].as_str().unwrap()).unwrap();
        let prev = hash_hex_to_native(inp["prev_block_hash_hex"].as_str().unwrap()).unwrap();
        let c = hash_hex_to_native(inp["convergence_root_hex"].as_str().unwrap()).unwrap();
        let height = inp["height"].as_u64().unwrap() as u32;
        let sigma = compute_sigma_hash(&txid, &c, &prev, height);
        let expected = hash_hex_to_native(v["expected"]["sigma"].as_str().unwrap()).unwrap();
        assert_eq!(sigma, expected, "vector {}", v["id"]);
    }
}

#[test]
fn inequality_golden_vectors() {
    let data = load_vectors();
    for v in data["vectors"].as_array().unwrap() {
        if v["module"] != "Inequality" {
            continue;
        }
        let inp = &v["input"];
        let h = hash_hex_to_native(inp["h_hex"].as_str().unwrap()).unwrap();
        let t = hash_hex_to_native(inp["T_hex"].as_str().unwrap()).unwrap();
        let f = inp["f"].as_u64().unwrap();
        let eligible = convergence_draw_inequality_holds(&h, &t, f);
        let expected = v["expected"]["eligible"].as_bool().unwrap();
        assert_eq!(eligible, expected, "vector {}", v["id"]);
    }
}

#[test]
fn run_selection_golden_vectors() {
    let data = load_vectors();
    for v in data["vectors"].as_array().unwrap() {
        if v["module"] != "RunSelection" {
            continue;
        }
        let inp = &v["input"];
        let txids: Vec<String> = inp["txids_hex"]
            .as_array()
            .unwrap_or(&vec![])
            .iter()
            .map(|x| x.as_str().unwrap().to_string())
            .collect();
        let fees: Vec<u64> = inp["fees"]
            .as_array()
            .unwrap_or(&vec![])
            .iter()
            .map(|x| x.as_u64().unwrap())
            .collect();
        let height = inp["height"].as_u64().unwrap() as u32;
        let prev_hex = inp["prev_block_hash_hex"].as_str().unwrap();
        let t_hex = inp["T_hex"].as_str().unwrap();
        let input = draw_input_from_golden(&txids, &fees, height, prev_hex, t_hex).unwrap();
        let result = replay_convergence_draw(&input);
        let exp = &v["expected"];
        assert_eq!(
            hex::encode(result.convergence_root),
            exp["convergence_root"].as_str().unwrap(),
            "root {}",
            v["id"]
        );
        let sigmas: Vec<String> = exp["sigmas"]
            .as_array()
            .unwrap_or(&vec![])
            .iter()
            .map(|s| s.as_str().unwrap().to_string())
            .collect();
        for (i, exp_sigma) in sigmas.iter().enumerate() {
            let expected = hash_hex_to_native(exp_sigma).unwrap();
            assert_eq!(result.candidates[i].h, expected, "sigma {} {}", v["id"], i);
        }
        let eligible: Vec<bool> = exp["eligible"]
            .as_array()
            .unwrap_or(&vec![])
            .iter()
            .map(|b| b.as_bool().unwrap())
            .collect();
        for (i, exp_el) in eligible.iter().enumerate() {
            assert_eq!(
                result.candidates[i].draw_eligible, *exp_el,
                "eligible {} {}",
                v["id"], i
            );
        }
        assert_eq!(
            result.winner_index,
            exp["winner_index"].as_i64().unwrap() as i32,
            "winner {}",
            v["id"]
        );
    }
}

#[test]
fn equivalence_claimed_is_false() {
    assert!(!winner_engine::EQUIVALENCE_CLAIMED);
}
