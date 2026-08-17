import { Link, useLocation } from "react-router-dom";
import { sectionByPath } from "../ia/sitemap";

export function SubNav() {
  const { pathname } = useLocation();
  const section = sectionByPath(pathname);
  if (!section) return null;

  return (
    <nav className="subnav" aria-label={`${section.title} sections`}>
      <Link to={section.path} className={pathname === section.path ? "active" : ""}>
        {section.title}
      </Link>
      {section.children.map((child) => (
        <Link
          key={child.path}
          to={child.path}
          className={pathname === child.path ? "active" : ""}
        >
          {child.title}
        </Link>
      ))}
    </nav>
  );
}
