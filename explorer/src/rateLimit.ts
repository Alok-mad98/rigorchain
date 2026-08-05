import type { Request, Response, NextFunction } from "express";
import type { Config } from "./config";

type Bucket = {
  count: number;
  windowStart: number;
};

const buckets = new Map<string, Bucket>();

export type RateLimitProfile = {
  windowMs: number;
  maxPerWindow: number;
};

function clientIp(req: Request): string {
  const fwd = req.headers["x-forwarded-for"];
  if (typeof fwd === "string" && fwd.length > 0) return fwd.split(",")[0].trim();
  if (Array.isArray(fwd) && fwd[0]) return fwd[0].split(",")[0].trim();
  return req.socket.remoteAddress ?? "unknown";
}

function routeKey(req: Request): string {
  const p = req.path;
  if (p.startsWith("/api/tx/")) return "/api/tx/:txid";
  if (p.startsWith("/api/block/")) return "/api/block/:hash";
  if (p.startsWith("/api/address/")) return "/api/address/:addr";
  if (p.startsWith("/api/lineage/")) return "/api/lineage/:txid";
  if (p.startsWith("/tx/") && !p.endsWith(".html")) return "/tx/:txid";
  if (p.startsWith("/block/") && !p.endsWith(".html")) return "/block/:hash";
  if (p.startsWith("/address/") && !p.endsWith(".html")) return "/address/:addr";
  return p;
}

export function resolveRateLimitProfile(config: Config, req: Request): RateLimitProfile {
  const key = routeKey(req);
  const specific = config.rateLimitRoutes[key];
  if (specific) {
    return { windowMs: config.rateLimitWindowMs, maxPerWindow: specific };
  }
  return { windowMs: config.rateLimitWindowMs, maxPerWindow: config.rateLimitGlobalMax };
}

function bucketKey(ip: string, route: string): string {
  return `${ip}|${route}`;
}

export function checkRateLimit(
  ip: string,
  route: string,
  profile: RateLimitProfile,
  nowMs = Date.now(),
): { allowed: boolean; retryAfterSec: number; remaining: number } {
  const key = bucketKey(ip, route);
  const cur = buckets.get(key);
  if (!cur || nowMs - cur.windowStart >= profile.windowMs) {
    buckets.set(key, { count: 1, windowStart: nowMs });
    return { allowed: true, retryAfterSec: 0, remaining: profile.maxPerWindow - 1 };
  }
  if (cur.count >= profile.maxPerWindow) {
    const retryAfterSec = Math.ceil((cur.windowStart + profile.windowMs - nowMs) / 1000);
    return { allowed: false, retryAfterSec: Math.max(1, retryAfterSec), remaining: 0 };
  }
  cur.count += 1;
  buckets.set(key, cur);
  return { allowed: true, retryAfterSec: 0, remaining: profile.maxPerWindow - cur.count };
}

export function resetRateLimitBucketsForTests(): void {
  buckets.clear();
}

export function createRateLimitMiddleware(config: Config) {
  return (req: Request, res: Response, next: NextFunction): void => {
    if (!config.rateLimitEnabled) {
      next();
      return;
    }

    const ip = clientIp(req);
    const route = routeKey(req);
    const profile = resolveRateLimitProfile(config, req);
    const result = checkRateLimit(ip, route, profile);

    res.setHeader("X-RateLimit-Limit", String(profile.maxPerWindow));
    res.setHeader("X-RateLimit-Remaining", String(Math.max(0, result.remaining)));

    if (!result.allowed) {
      res.setHeader("Retry-After", String(result.retryAfterSec));
      res.status(429).json({
        error: "rate limit exceeded",
        route,
        retry_after_seconds: result.retryAfterSec,
      });
      return;
    }

    next();
  };
}
