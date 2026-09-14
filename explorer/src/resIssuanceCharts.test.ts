import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { buildRfc020Curve } from "./resIssuanceSchedule";
import { renderCumulativeIssuanceChartSvg, renderResIssuanceChartsHtml, renderUnitPriceChartSvg } from "./resIssuanceCharts";

describe("RES issuance chart year grid", () => {
  it("plots RFC-020 years and the 638-year pole, not the 251-epoch calendar", () => {
    const points = buildRfc020Curve();
    const svg = renderUnitPriceChartSvg(points, 0);
    assert.match(svg, /data-year="0"/);
    assert.match(svg, /data-year="638"/);
    assert.match(svg, /data-year="2000"/);
    assert.doesNotMatch(svg, /data-epoch=/);
    assert.doesNotMatch(svg, /now k=/);
    assert.match(svg, /pole 638/);
    assert.match(svg, />638<\/text>/);
    assert.match(svg, />2000<\/text>/);
    assert.equal((svg.match(/data-year="/g) || []).length, 13);
    assert.match(renderResIssuanceChartsHtml(points, 0), /status-grid--charts/);
    assert.doesNotMatch(renderResIssuanceChartsHtml(points, 0), /status-grid--pair/);
  });

  it("marks the current year on the ceiling chart the same way", () => {
    const points = buildRfc020Curve();
    const svg = renderCumulativeIssuanceChartSvg(points, 4);
    assert.match(svg, /data-mark="now"/);
    assert.match(svg, /data-year="638"/);
    assert.doesNotMatch(svg, /data-epoch=/);
  });
});
