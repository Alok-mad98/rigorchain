/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_RPC_PROXY_URL?: string;
  readonly VITE_EXPLORER_API_URL?: string;
  readonly VITE_VERIFICATION_MANIFEST_URL?: string;
  readonly VITE_SITE_VERSION?: string;
  readonly VITE_ZERO_TRUST_VERIFIER_PUBLIC_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
