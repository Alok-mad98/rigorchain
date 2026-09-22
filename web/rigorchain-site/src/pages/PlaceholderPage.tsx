import { useLocation } from "react-router-dom";
import { explorerBaseUrl } from "../lib/explorerClient";
import { nodeByPath, sectionByPath } from "../ia/sitemap";
import { SubNav } from "../components/SubNav";

export function PlaceholderPage() {
  const { pathname } = useLocation();
  const node = nodeByPath(pathname);
  const section = sectionByPath(pathname);
  const explorer = explorerBaseUrl();

  if (!node) {
    return (
      <div className="page">
        <SubNav />
        <header className="page-header">
          <h1>Section planned</h1>
          <p className="muted">This route is reserved for the official RIGOR portal.</p>
        </header>
      </div>
    );
  }

  const liveHref = node.explorerPath && explorer ? `${explorer}${node.explorerPath}` : null;

  return (
    <div className="page">
      <SubNav />
      <div className="placeholder-banner" role="status">
        This section is reserved. It is not a consensus authority.
      </div>
      <header className="page-header">
        <h1>{node.title}</h1>
        <p className="muted">{node.purpose}</p>
      </header>
      <section className="card prose-section">
        <p>
          Public section under <strong>{section?.title ?? "RIGOR"}</strong>. Material for this page
          will be published with the public documentation set.
        </p>
        {node.planned && node.planned.length > 0 && (
          <>
            <h2>Planned content</h2>
            <ul>
              {node.planned.map((item) => (
                <li key={item}>{item}</li>
              ))}
            </ul>
          </>
        )}
        {liveHref && (
          <p>
            Live explorer viewer (untrusted display):{" "}
            <a href={liveHref} rel="noreferrer" target="_blank">
              {liveHref}
            </a>
          </p>
        )}
        {node.explorerPath && !explorer && (
          <p className="muted">
            Explorer URL is not configured (<code>VITE_EXPLORER_API_URL</code>). Route reserved:{" "}
            <code>{node.explorerPath}</code>
          </p>
        )}
      </section>
    </div>
  );
}
