import { DarkCard } from "./DarkCard";
import type { SiteSection } from "../ia/sitemap";

export function SectionHub({ section }: { section: SiteSection }) {
  return (
    <section className="section-hub" aria-label={`${section.title} modules`}>
      <h2 className="section-title">{section.title} modules</h2>
      <div className="card-grid">
        {section.children.map((child) => (
          <DarkCard key={child.path} title={child.title} to={child.path} linkLabel="Open">
            <p>
              <span className={`tag tag-${child.preservation.toLowerCase()}`}>{child.preservation}</span>{" "}
              {child.purpose}
            </p>
          </DarkCard>
        ))}
      </div>
    </section>
  );
}
