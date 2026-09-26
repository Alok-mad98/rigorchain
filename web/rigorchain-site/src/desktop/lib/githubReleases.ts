import { GITHUB_API_RELEASES } from "../data/links";

export type GithubAsset = {
  name: string;
  browser_download_url: string;
  size: number;
};

export type GithubRelease = {
  tag_name: string;
  name: string | null;
  html_url: string;
  published_at: string;
  body: string | null;
  assets: GithubAsset[];
};

export type GithubFetch = {
  releases: GithubRelease[] | null;
  error: string | null;
  fetched_at: string;
};

export async function fetchGithubReleases(): Promise<GithubFetch> {
  const fetched_at = new Date().toISOString();
  try {
    const res = await fetch(GITHUB_API_RELEASES, {
      headers: { Accept: "application/vnd.github+json" },
    });
    if (!res.ok) {
      return { releases: null, error: `GitHub HTTP ${res.status}`, fetched_at };
    }
    const body = (await res.json()) as GithubRelease[];
    return { releases: body, error: null, fetched_at };
  } catch (e) {
    return {
      releases: null,
      error: e instanceof Error ? e.message : String(e),
      fetched_at,
    };
  }
}
