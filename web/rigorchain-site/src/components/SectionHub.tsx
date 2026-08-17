import { Link } from "react-router-dom";
import type { SiteSection } from "../ia/sitemap";

export function SectionHub({ section }: { section: SiteSection }) {
  return (
    <section className="section-hub" aria-label={`${section.title} modules`}>
      <h2 className="section-title">{section.title} modules</h2>
      <div className="card-grid">
        {section.children.map((child) => (
          <article key={child.path} className="card hub-card">
            <header className="card-header">
              <span className={`tag tag-${child.preservation.toLowerCase()}`}>{child.preservation}</span>
              <h3>
                <Link to={child.path}>{child.title}</Link>
              </h3>
            </header>
            <p>{child.purpose}</p>
          </article>
        ))}
      </div>
    </section>
  );
}
