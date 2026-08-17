/**
 * Browser WebCrypto SHA-256 / Hash256.
 * Do not use CryptoJS or hex-string hashing.
 */

export async function sha256(data: Uint8Array): Promise<Uint8Array> {
  if (typeof crypto === "undefined" || !crypto.subtle) {
    throw new Error("WebCrypto API is not available in this browser");
  }
  const copy = new Uint8Array(data.byteLength);
  copy.set(data);
  const digest = await crypto.subtle.digest("SHA-256", copy);
  return new Uint8Array(digest);
}

/** Hash256(x) = SHA256(SHA256(x)) — consensus digest. */
export async function hash256(data: Uint8Array): Promise<Uint8Array> {
  return sha256(await sha256(data));
}
