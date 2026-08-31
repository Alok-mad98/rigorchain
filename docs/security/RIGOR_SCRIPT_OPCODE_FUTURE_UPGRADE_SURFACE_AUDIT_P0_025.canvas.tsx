import {
  BarChart,
  Callout,
  Card,
  CardBody,
  CardHeader,
  Code,
  Divider,
  Grid,
  H1,
  H2,
  H3,
  Pill,
  Row,
  Stack,
  Stat,
  Table,
  Text,
  UsageBar,
  useHostTheme,
} from "cursor/canvas";

export default function RigorOpcodeUpgradeSurfaceAudit() {
  const theme = useHostTheme();

  return (
    <Stack gap={24} style={{ padding: 24, maxWidth: 1100 }}>
      <Stack gap={8}>
        <Row gap={8} align="center">
          <H1>P0-025 Opcode / VM / Upgrade Surface</H1>
          <Pill tone="success" active>
            AUDIT ONLY
          </Pill>
        </Row>
        <Text tone="secondary">
          Production consensus is classifier + fixed templates + Ed25519. There is
          no opcode VM. Mainnet has not launched. Source: core/src/script/interpreter.cpp,
          rigor_classify.cpp, tx_check.cpp, rigor_v01/*.v · 2026-08-27
        </Text>
      </Stack>

      <Callout tone="success" title="Architecture freeze">
        LIVE_OPCODE_EXECUTION_SET is empty. EvalScript is deleted. Unknown
        ScriptPubKey, unknown opcode, and unknown witness version fail closed.
        Future upgrades should add a dedicated predicate, not OP_NOP / OP_RIGOR_X.
      </Callout>

      <Grid columns={4} gap={12}>
        <Stat value="0" label="VM-executed opcodes" tone="success" />
        <Stat value="5" label="Live ScriptPubKey templates" tone="info" />
        <Stat value="91" label="Dead Bitcoin opcodes" tone="warning" />
        <Stat value="NO" label="General Script VM" tone="success" />
      </Grid>

      <Grid columns={4} gap={12}>
        <Stat value="LEGACY ONLY" label="Bitcoin opcode namespace" />
        <Stat value="PREDICATE" label="Future upgrade model" tone="info" />
        <Stat value="LOW" label="Live consensus risk" tone="success" />
        <Stat value="PARTIAL" label="Formal alignment" tone="warning" />
      </Grid>

      <Stack gap={8}>
        <H2>Named opcode enum (115) by role</H2>
        <Text size="small" tone="secondary">
          Distinct assigned values in opcodetype excluding aliases OP_FALSE / OP_TRUE /
          OP_NOP2 / OP_NOP3. Live set is byte markers and the push parser, not a VM.
        </Text>
        <UsageBar
          total={115}
          topLeftLabel="115 named opcodes in script.h"
          topRightLabel="0 executed as Forth ops"
          segments={[
            { id: "live", value: 23, color: "green" },
            { id: "dead", value: 91, color: "orange" },
            { id: "invalid", value: 1, color: "gray" },
          ]}
        />
        <Row gap={16} wrap>
          <Text size="small">23 live markers / push parser</Text>
          <Text size="small">91 dead Bitcoin</Text>
          <Text size="small">1 OP_INVALIDOPCODE</Text>
        </Row>
      </Stack>

      <BarChart
        categories={["VM execution", "Template markers", "Push parser", "Dead enum", "Tooling names"]}
        series={[
          {
            name: "Count",
            data: [0, 5, 23, 91, 115],
            tone: "info",
          },
        ]}
        height={220}
      />
      <Text size="small" tone="secondary">
        Opcode inventory by role. VM execution is empty. Template markers are v0/20,
        v1/32, RES_MINT, CONVERGE, OP_RETURN. Tooling names are the full GetOpName table.
        Source: core/src/script/script.h + interpreter.cpp
      </Text>

      <H2>VerifyScript dispatch</H2>
      <Table
        headers={["scriptPubKey", "Branch", "Authorization", "Opcode VM?"]}
        columnAlign={["left", "left", "left", "center"]}
        rowTone={["success", "success", "success", "warning", "neutral", "danger"]}
        rows={[
          ["canonical RES_MINT", "VerifyResMintSpend", "height + Ed25519", "no"],
          ["v0 / 20 P2WPKH", "VerifyP2WPKHSpend", "HASH160(pk) + Ed25519", "no"],
          ["v1 / 32 native RES", "key-path", "pk == program + Ed25519", "no"],
          ["CONVERGE 22-byte", "no spend branch", "burn; not a UTXO", "no"],
          ["OP_RETURN push-only", "not a spend", "datacarrier nValue=0", "no"],
          ["anything else", "RIGOR_SCRIPT_TYPE", "fail closed", "no"],
        ]}
        striped
        stickyHeader
      />

      <H2>Fail-closed truth table</H2>
      <Table
        headers={["Input", "Class", "Spendable", "Accepted", "Why"]}
        rowTone={[
          "success",
          "success",
          "info",
          "warning",
          "neutral",
          "danger",
          "danger",
          "danger",
        ]}
        rows={[
          ["ORU v0/20", "ORU", "yes", "yes", "Ed25519 P2WPKH"],
          ["RES v1/32 native", "RES", "yes", "yes", "pk == program"],
          ["RES_MINT", "RES", "after maturity", "coinbase only", "height + Ed25519"],
          ["CONVERGE", "ORU burn", "no", "yes", "skipped in UTXO"],
          ["OP_RETURN value 0", "NONE", "no", "yes", "datacarrier"],
          ["unknown template / opcode", "INVALID", "no", "no", "fail closed"],
          ["witness v2–16", "INVALID", "no", "no", "not a soft-fork hook"],
          ["P2PKH / P2SH / P2WSH", "INVALID", "no", "no", "bad-rigor-script-type"],
        ]}
        striped
        stickyHeader
      />

      <Callout tone="warning" title="Unknown witness version is not Bitcoin-upgradeable">
        Bitcoin used unknown witness versions as anyone-can-spend plus policy
        discourage. RIGOR rejects creation in CheckTransaction and rejects spend in
        VerifyScript (SCRIPT_ERR_RIGOR_SCRIPT_TYPE). The leftover error name
        DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM is wording only.
      </Callout>

      <H2>Consensus reachability</H2>
      <Grid columns={2} gap={16}>
        <Card>
          <CardHeader>Bitcoin opcode namespace</CardHeader>
          <CardBody>
            <Stack gap={8}>
              <Text>enum opcodetype / GetOpName / RPC asm / tests</Text>
              <Text tone="secondary">Not an execution engine. EvalScript deleted.</Text>
              <Text weight="semibold" style={{ color: theme.accent.primary }}>
                does not reach ConnectBlock as a VM
              </Text>
            </Stack>
          </CardBody>
        </Card>
        <Card>
          <CardHeader>Live RIGOR path</CardHeader>
          <CardBody>
            <Stack gap={6}>
              <Text>ScriptPubKey bytes</Text>
              <Text>ClassifyRigorTxout</Text>
              <Text>CheckTransaction fail-closed</Text>
              <Text>VerifyScript structural dispatch</Text>
              <Text>Ed25519 (or burn / datacarrier)</Text>
              <Text>ConnectBlock</Text>
            </Stack>
          </CardBody>
        </Card>
      </Grid>

      <H2>Future upgrade models</H2>
      <Table
        headers={["Model", "Mechanism", "Fork nature", "Formal cost", "Verdict"]}
        rowTone={["danger", "warning", "success"]}
        rows={[
          [
            "A Opcode extension",
            "OP_RIGOR_X / restore interpreter",
            "Soft only if NOP/SUCCESS/anyone-can-spend",
            "High — VM semantics",
            "Reject",
          ],
          [
            "B Versioned ScriptPubKey",
            "version + program, classifier dispatch",
            "Hard under fail-closed create",
            "One constructor per version",
            "Encoding only; do not open v2",
          ],
          [
            "C Dedicated predicate",
            "Classifier → Predicate(k) → verifier",
            "Coordinated hard fork for new k",
            "Add inductive constructor",
            "Recommended",
          ],
        ]}
        striped
      />

      <Stack gap={8}>
        <H3>NOP / reserved</H3>
        <Text>
          RIGOR does not need OP_NOP1–OP_NOP10. Soft-fork-by-NOP requires a VM that
          treats NOP as success. That VM is gone. Keeping the names invites
          reconnection. New assets, signatures, or covenants should be new templates
          and new predicates, optionally with an explicit RIGOR program version
          field, never Bitcoin opcode reservation.
        </Text>
      </Stack>

      <H2>Cleanup grades (not performed)</H2>
      <Grid columns={2} gap={12}>
        <Card>
          <CardHeader trailing={<Pill tone="deleted" active size="sm">P0 freeze</Pill>}>
            Do not touch
          </CardHeader>
          <CardBody>
            <Text size="small">
              OP_0, OP_1–16, PUSHDATA, OP_RETURN, OP_RES_MINT 0xbb, OP_CONVERGE 0xbd,
              burned 0xbc, GetOp, CScriptNum mint encoding, VerifyScript, classifier,
              MAX_SCRIPT_ELEMENT_SIZE
            </Text>
          </CardBody>
        </Card>
        <Card>
          <CardHeader trailing={<Pill tone="warning" active size="sm">P1</Pill>}>
            Reconnection magnets
          </CardHeader>
          <CardBody>
            <Text size="small">
              Wallet IsMine &lt;pk&gt; OP_CHECKSIG, DISCOURAGE_* error strings,
              stacktop macros, IsOpSuccess, STANDARD_NOT_MANDATORY flag split, stale
              P2WSH docs
            </Text>
          </CardBody>
        </Card>
        <Card>
          <CardHeader trailing={<Pill tone="info" active size="sm">P2</Pill>}>
            Dead code
          </CardHeader>
          <CardBody>
            <Text size="small">
              MAX_OPS_PER_SCRIPT, MAX_STACK_SIZE, CountScriptOpsLegacyTestOnly,
              unassigned SCRIPT_ERR_*, GetOpName cases for CHECKMULTISIG / CLTV / NOP
            </Text>
          </CardBody>
        </Card>
        <Card>
          <CardHeader trailing={<Pill size="sm">P3</Pill>}>
            Naming
          </CardHeader>
          <CardBody>
            <Text size="small">
              RPC asm Bitcoin names, decodescript P2SH wrap, functional
              test_framework/script.py opcode dictionary
            </Text>
          </CardBody>
        </Card>
      </Grid>

      <Divider />

      <Stack gap={8}>
        <H2>Formal / implementation mismatch</H2>
        <Text>
          rigor_v01 models ScriptPubKey, the unique classifier, unknown → INVALID,
          and three Ed25519 spend constructors. It does not model an opcode VM or
          NOP reservation. C++ still ships the full Bitcoin opcode enum. Live path:
          aligned. Leftover namespace: mismatch. equivalence_claimed remains false.
        </Text>
        <Row gap={8} wrap>
          <Pill tone="success" active>
            SAFE_TO_BEGIN_OPCODE_CLEANUP: YES
          </Pill>
          <Pill tone="warning" active>
            P2/P3 only — freeze VerifyScript
          </Pill>
          <Code>docs/security/RIGOR_SCRIPT_OPCODE_FUTURE_UPGRADE_SURFACE_AUDIT_P0_025.md</Code>
        </Row>
      </Stack>
    </Stack>
  );
}
