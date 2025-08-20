export const config = { runtime: 'edge' };

const CORS = {
  'access-control-allow-origin': '*',
  'access-control-allow-headers': '*',
  'access-control-allow-methods': 'GET, HEAD, OPTIONS',
  'access-control-expose-headers': 'content-length, accept-ranges, content-type',
};

function pickHeaders(req: Request) {
  const out: Record<string,string> = {};
  for (const [k, v] of req.headers.entries()) {
    if (/^(range|user-agent|accept|accept-language|accept-encoding|content-type)$/i.test(k)) {
      out[k] = v;
    }
  }
  return out;
}

export default async function handler(req: Request) {
  if (req.method === 'OPTIONS') return new Response(null, { status: 204, headers: CORS });

  const u = new URL(req.url);
  const target = u.searchParams.get('u');
  if (!target) return new Response('Missing ?u=…', { status: 400, headers: CORS });

  try {
    const t = new URL(target);
    if (!t.hostname.endsWith('huggingface.co')) return new Response('Only huggingface.co allowed', { status: 403, headers: CORS });

    const method = req.method === 'HEAD' ? 'HEAD' : 'GET';
    const upstream = await fetch(t.toString(), { method, headers: pickHeaders(req), redirect: 'follow' });

    const headers = new Headers(upstream.headers);
    for (const [k, v] of Object.entries(CORS)) headers.set(k, v);
    return new Response(method === 'HEAD' ? null : upstream.body, { status: upstream.status, headers });
  } catch (e: any) {
    return new Response(`Proxy error: ${e?.message || e}`, { status: 502, headers: CORS });
  }
}
