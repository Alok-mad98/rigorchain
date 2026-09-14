import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { ORU_COIN, RES_COIN } from "./amount";
import {
  CUMULATIVE_ISSUANCE_CAPTION,
  ISSUANCE_GENERATION,
  RES_DERIVED_MAX_SUPPLY,
  RES_MAX_SUPPLY,
  RES_DESTROY_POLE,
  RES_DESTROY_SHIFT,
  RES_DESTROY_COST_GENESIS,
  UNIT_PRICE_CURVE_CAPTION,
  buildRfc020MilestoneRows,
  confirmHeightForTip,
  cumulativeResAtHeight,
  nextHeightDestroyCostSentence,
  resIssuanceSchedulePayload,
  resDestroyCostForHeight,
  yearToHeight,
} from "./resIssuanceSchedule";

describe("RFC-020 explorer issuance schedule", () => {
  it("pins continuous destroy-to-mint amounts and the closed-form cap", () => {
    assert.equal(resDestroyCostForHeight(0), 1024n * ORU_COIN);
    assert.equal(resDestroyCostForHeight(1), 102400006103n);
    assert.equal(resDestroyCostForHeight(210000), 103685749152n);
    assert.equal(resDestroyCostForHeight(420000), 104979520049n);
    assert.ok(resDestroyCostForHeight(210000 * 251) > 0n);
    assert.equal(cumulativeResAtHeight(210000), 2608673648056n);
    assert.ok(cumulativeResAtHeight(250 * 210000) < RES_MAX_SUPPLY);
    assert.equal(RES_MAX_SUPPLY, 419430400000000n);
    assert.equal(RES_DERIVED_MAX_SUPPLY, RES_MAX_SUPPLY);
    assert.equal(RES_COIN, ORU_COIN);
    assert.equal(
      ISSUANCE_GENERATION,
      `rfc020:cap=${RES_MAX_SUPPLY}:p0=${RES_DESTROY_COST_GENESIS}:pole=${RES_DESTROY_POLE}:shift=${RES_DESTROY_SHIFT}`,
    );
    assert.doesNotMatch(ISSUANCE_GENERATION, /613454/);
    assert.equal(confirmHeightForTip(528), 529);
    assert.equal(resDestroyCostForHeight(529), 102403228785n);
  });

  it("states future destroy amounts, not appreciation", () => {
    const sentence = nextHeightDestroyCostSentence(210000, 103685749152n);
    assert.equal(sentence, "From block 210000, minting 1 RES destroys 1036.85749152 ORU.");
    assert.match(UNIT_PRICE_CURVE_CAPTION, /destroyed/i);
    assert.doesNotMatch(UNIT_PRICE_CURVE_CAPTION, /price\(h\)|183x|appreciat|ROI|value grows|mint before/i);
    assert.doesNotMatch(CUMULATIVE_ISSUANCE_CAPTION, /scarce|upside|invest|price\(h\)/i);
  });

  it("pins the RFC-020 year table (pole 638, cap 2^22)", () => {
    assert.equal(yearToHeight(1), 52594);
    assert.equal(yearToHeight(4), 210379);
    const byYear = new Map(buildRfc020MilestoneRows().map((row) => [row.year, row]));
    assert.equal(byYear.get(0)?.destroy_oru_atoms, (1024n * ORU_COIN).toString());
    assert.equal(byYear.get(0)?.cumulative_res_atoms, "0");
    const y1 = byYear.get(1);
    assert.equal(y1?.height, 52594);
    assert.equal(y1?.destroy_oru_atoms, resDestroyCostForHeight(52594).toString());
    const pole = byYear.get(638);
    assert.ok(pole);
    assert.equal(pole.height, 33555558);
    assert.equal(pole.cumulative_res_atoms, cumulativeResAtHeight(33555558).toString());
    assert.equal(yearToHeight(638), 33555558);
    const payload = resIssuanceSchedulePayload();
    assert.equal(payload.pole_year, 638);
    assert.equal(payload.chart_end_year, 2000);
    assert.ok(payload.curve.length > 13);
    assert.equal(payload.milestones.length, 13);
    assert.doesNotMatch(JSON.stringify(payload), /price\(h\)/);
  });
});
