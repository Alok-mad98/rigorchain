import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  ADDRESS_READONLY_NOTICE,
  atomsToOruDisplay,
  atomsToResDisplay,
  classifyAddress,
  classifyAddressWithHint,
  inferAddressTypeFromBech32,
  isValidAddressFormat,
} from "./addressIntelligence";

describe("addressIntelligence", () => {
  it("validates checksum-correct RIGOR bech32 addresses", () => {
    assert.equal(isValidAddressFormat("trg1nsxfhxfv3axth3dd53w0r7a8ptu9xejs02fq3j"), true);
    assert.equal(
      isValidAddressFormat("trg1pflfvwsh8p5fuykxnrddzp8jd53fgnpsnuhvv5xrj5rfy6vs34ydqywruva"),
      true,
    );
    assert.equal(isValidAddressFormat("trg1qnotavalidchecksum"), false);
    assert.equal(isValidAddressFormat("bc1qinvalid"), false);
    assert.equal(isValidAddressFormat(""), false);
  });

  it("infers ORU vs RES from bech32 vs bech32m checksum", () => {
    assert.equal(inferAddressTypeFromBech32("trg1nsxfhxfv3axth3dd53w0r7a8ptu9xejs02fq3j"), "ORU (Witness v0)");
    assert.equal(
      inferAddressTypeFromBech32("trg1pflfvwsh8p5fuykxnrddzp8jd53fgnpsnuhvv5xrj5rfy6vs34ydqywruva"),
      "RES (Witness v1)",
    );
  });

  it("classifies ORU and RES addresses separately", () => {
    assert.deepEqual(classifyAddress(true, false, true), {
      address_type: "ORU (Witness v0)",
      classification: "ORU Address",
    });
    assert.deepEqual(classifyAddress(false, true, true), {
      address_type: "RES (Witness v1)",
      classification: "RES Address",
    });
    assert.deepEqual(classifyAddress(true, true, true), {
      address_type: "Mixed ORU/RES",
      classification: "Mixed Address",
    });
  });

  it("hints unused address type from bech32 without merging balances", () => {
    assert.deepEqual(
      classifyAddressWithHint(false, false, false, "trg1nsxfhxfv3axth3dd53w0r7a8ptu9xejs02fq3j"),
      {
        address_type: "ORU (Witness v0)",
        classification: "ORU Address",
      },
    );
    assert.deepEqual(
      classifyAddressWithHint(false, false, false, "trg1pflfvwsh8p5fuykxnrddzp8jd53fgnpsnuhvv5xrj5rfy6vs34ydqywruva"),
      {
        address_type: "RES (Witness v1)",
        classification: "RES Address",
      },
    );
  });

  it("formats ORU and RES atom amounts", () => {
    assert.equal(atomsToOruDisplay(100000000), "1.00000000 ORU");
    assert.equal(atomsToResDisplay(100), "1.00 RES");
  });

  it("readonly notice forbids transaction creation", () => {
    assert.match(ADDRESS_READONLY_NOTICE, /does not create, sign, or broadcast/);
    assert.match(ADDRESS_READONLY_NOTICE, /indexed rigord data/);
  });
});
