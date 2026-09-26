import { useEffect, useState } from "react";
import { GITHUB_RELEASES, GITHUB_REPO, isMainnetAssetName, NETWORK_NAME } from "../data/links";
import { fetchGithubReleases, type GithubRelease } from "../lib/githubReleases";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

function productionReleases(releases: GithubRelease[]): GithubRelease[] {
  return releases
    .filter((r) => isMainnetAssetName(r.tag_name) && isMainnetAssetName(r.name ?? ""))
    .map((r) => ({
      ...r,
      assets: r.assets.filter((a) => isMainnetAssetName(a.name)),
    }));
}

export function DownloadManager({ win }: { win: WindowRecord }) {
  const { closeWindow, openApp } = useDesktop();
  const [releases, setReleases] = useState<GithubRelease[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let alive = true;
    void fetchGithubReleases().then((r) => {
      if (!alive) return;
      setReleases(r.releases ? productionReleases(r.releases) : null);
      setError(r.error);
    });
    return () => {
      alive = false;
    };
  }, []);

  return (
    <Window
      win={{ ...win, title: "Downloads - C:\\RIGOR\\DOWNLOADS" }}
      menu={
        <MenuBar
          menus={[
            {
              label: "File",
              items: [
                {
                  label: "Open GitHub Releases",
                  action: () => window.open(GITHUB_RELEASES, "_blank", "noreferrer"),
                },
                { label: "Close", action: () => closeWindow(win.id) },
              ],
            },
          ]}
        />
      }
      status={error ? error : `${NETWORK_NAME}  |  GitHub Releases`}
    >
      <div className="app-page">
        <h1>C:\RIGOR\DOWNLOADS</h1>
        <p>
          Network: {NETWORK_NAME}. Verify published checksums before running anything. This desktop
          does not execute binaries.
        </p>

        <h2>RIGOR Node</h2>
        <p>Full node. Stores the chain and checks every block.</p>
        <button
          type="button"
          className="win95-btn"
          onClick={() => openApp({ kind: "node", title: "RIGOR Node", icon: "node" })}
        >
          Open RIGOR Node
        </button>

        <h2>RIGOR Wallet</h2>
        <p>Spending tool. Not a consensus authority.</p>
        <button
          type="button"
          className="win95-btn"
          onClick={() => openApp({ kind: "wallet", title: "RIGOR Wallet", icon: "wallet" })}
        >
          Open RIGOR Wallet
        </button>

        <h2>RIGOR Miner</h2>
        <p>Proof of Work. OpuHash. Pays ORU subsidy.</p>
        <button
          type="button"
          className="win95-btn"
          onClick={() => openApp({ kind: "miner", title: "RIGOR Miner", icon: "miner" })}
        >
          Open RIGOR Miner
        </button>

        <h2>Source Code</h2>
        <p>
          <a href={GITHUB_REPO} target="_blank" rel="noreferrer">
            {GITHUB_REPO}
          </a>
        </p>

        <h2>Checksums</h2>
        <p>
          Published with each release.{" "}
          <a href={GITHUB_RELEASES} target="_blank" rel="noreferrer">
            Open checksums on GitHub
          </a>
        </p>

        <h2>Release Notes</h2>
        <p>
          <a href={GITHUB_RELEASES} target="_blank" rel="noreferrer">
            {GITHUB_RELEASES}
          </a>
        </p>

        <h2>GitHub Releases</h2>
        {error ? <p className="err">{error}</p> : null}
        {releases && releases.length === 0 ? <p>No production releases returned.</p> : null}
        {releases && releases.length > 0 ? (
          <table className="browser-table">
            <thead>
              <tr>
                <th>Tag</th>
                <th>Published</th>
                <th>Assets</th>
              </tr>
            </thead>
            <tbody>
              {releases.slice(0, 8).map((r) => (
                <tr key={r.html_url}>
                  <td>
                    <a href={r.html_url} target="_blank" rel="noreferrer">
                      {r.tag_name}
                    </a>
                  </td>
                  <td>{r.published_at.slice(0, 10)}</td>
                  <td>
                    {r.assets.length
                      ? r.assets.map((a) => (
                          <div key={a.browser_download_url}>
                            <a href={a.browser_download_url}>{a.name}</a>
                          </div>
                        ))
                      : "—"}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        ) : null}
      </div>
    </Window>
  );
}
