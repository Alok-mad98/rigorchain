/** Blocks list pagination. Presentation query only. */

export const BLOCKS_PAGE_DEFAULT = 50;
export const BLOCKS_PAGE_MAX = 200;

export type BlockListQuery = {
  limit: number;
  page: number;
  offset: number;
};

export type BlockListPage = BlockListQuery & {
  total: number;
  pages: number;
  has_prev: boolean;
  has_next: boolean;
};

export function parseBlockListQuery(query: {
  limit?: unknown;
  page?: unknown;
}): BlockListQuery {
  const limitN = Number(query.limit ?? BLOCKS_PAGE_DEFAULT);
  const limit = Number.isFinite(limitN)
    ? Math.min(BLOCKS_PAGE_MAX, Math.max(1, Math.floor(limitN)))
    : BLOCKS_PAGE_DEFAULT;
  const pageN = Number(query.page ?? 1);
  const page = Number.isFinite(pageN) ? Math.max(1, Math.floor(pageN)) : 1;
  return { limit, page, offset: (page - 1) * limit };
}

export function blockListPageMeta(query: BlockListQuery, total: number): BlockListPage {
  const safeTotal = Number.isFinite(total) && total > 0 ? Math.floor(total) : 0;
  const pages = safeTotal === 0 ? 1 : Math.ceil(safeTotal / query.limit);
  const page = Math.min(query.page, pages);
  return {
    limit: query.limit,
    page,
    offset: (page - 1) * query.limit,
    total: safeTotal,
    pages,
    has_prev: page > 1,
    has_next: page < pages,
  };
}
