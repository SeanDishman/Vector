# API endpoints

The operator asked for Vector to also get the API endpoints a site uses and display them on
Overview. This records what is collected, how each row is labeled and what was verified.

## What it shows

Overview has an **API endpoints** subsection under Infrastructure, after the technology rows
and before Published providers. It starts with five compact rows; **Load more** reveals ten
more. Each row shows the method, the path (with the host when it is not the inspected origin
and the scheme when it is not HTTPS) and one word for how the endpoint was found. A line under
the heading counts the rows by source: seen in network requests (Observed, Answered,
Attempted), found in code (In code), or named by API documents, page markup or robots.txt.
Rows stay ordered with confirmed ones first, so the counts show code findings that sit
behind Load more. Double-click
a row, or focus it and press Enter, to read its evidence; **Back to API endpoints** returns to
the list. The subsection is left out when no web collection ran. A web inspection that found
none says so in one line.

| Row word | Basis in the report | What it means |
| --- | --- | --- |
| Observed | Observed request | The page's own code made this request in Vector's isolated browser and a response arrived. |
| Answered | Collected response | One of Vector's own page reads was answered with JSON. |
| Attempted | Request attempt | The page's code tried this request and no response was observed. |
| Described | API description | Listed in an OpenAPI or Swagger document the inspection had already collected. |
| Declared | Page declaration | Named by page markup or a response header. |
| In code | Code reference | A string literal in collected script text. |
| robots.txt | robots.txt | An API-shaped Allow or Disallow rule in that file. |

Only Observed and Answered rows are confirmed to respond. Vector sends no request to a listed
endpoint. It does not probe, replay or guess API paths, and the existing page, script, browser
and time caps are unchanged.

## Where rows come from

1. **Browser requests.** Requests of type XHR, Fetch, EventSource and Ping made by an inspected
   page. WebSocket connections are now recorded too, from the browser's own handshake events,
   as request type `WebSocket` with state `open`, `closed` or `failed`. Requests made by a
   separately rendered disclosure page (a trust center, for example) are left out.
2. **Vector's own reads.** A 2xx JSON answer to a page Vector already reads, including the
   existing `/swagger.json`, `/openapi.json`, `/v2/api-docs`, `/v3/api-docs` and `/graphql`
   probes. A 400 or 405 JSON body with an `errors` list at `/graphql` also counts, because that
   is how a GraphQL server answers a request without a query.
3. **API descriptions.** If one of those reads is an OpenAPI or Swagger document, each listed
   path and method becomes a row, resolved against the document's server URL or, for Swagger 2,
   its `host` and `basePath`. At most 128 operations per document.
4. **Page markup.** A `rel="https://api.w.org/"` link or `Link` header (the WordPress REST
   root), JSON and oEmbed alternate links, the `EditURI` link, an API-shaped form action with
   its method, and htmx request attributes (`hx-get`, `hx-post`, `hx-put`, `hx-patch`,
   `hx-delete`, and their `data-` forms). Static pages and the rendered document are both read.
5. **Script text.** String literals in static scripts, browser-loaded scripts, inline scripts
   and JSON data blocks. See the rules below.
6. **robots.txt.** Allow and Disallow rules whose path is API-shaped, at most 48 per file.

A reference that matches an observed request joins that row as further evidence and does not
add a row. `/pets/{petId}` in a description matches an observed `/pets/42`.

## What counts as an endpoint

A browser request is listed unless the answer shows it is something else: an HTML page, a React
Server Component payload, a stylesheet, script, image, font, media file or WebAssembly module,
or a path ending in a file extension such as `.json` or `.lottie` with no API word in it. CORS
preflights are not listed. A GET that got no response is listed only when its URL is
API-shaped; a write that got no response is listed as an attempt.

A URL is **API-shaped** when a path segment or hostname label is exactly one of `api`, `apis`,
`graphql`, `gql`, `rest`, `rpc`, `jsonrpc`, `trpc`, `ajax`, `wp-json`, `xmlrpc`, `webapi`,
`openapi`, `oauth`, `grpc` or `odata`, optionally with one leading word and a version number
(`x-api`, `sirius-api`, `api2`, `admin-ajax.php`), or when the first or second segment is a
version such as `v1`. A documentation route (`/docs/api/...`, `/en/api/...`) is not API-shaped.

A string literal in code is kept when it is API-shaped, or when it is the URL argument of
`fetch`, `axios`, `XMLHttpRequest.open`, `sendBeacon`, `new WebSocket`, `new EventSource`,
`$.getJSON` or a `.post`, `.put` or `.patch` call, or the value of a key such as `baseURL` or
`apiUrl`. The method is kept when the call states it.

Since November 11, 2026 the code scan also keeps:

- A `.get`, `.delete`, `.head` or `.options` call on an HTTP client object: `api`, `http`,
  `$http`, `httpClient`, `client`, `request`, `ky`, `superagent`, or an axios instance named
  `instance` or `service`, including `this.` forms of these. The same goes for any object
  name of two or more characters that also makes a `.post`, `.put` or `.patch` call in the
  same script; this catches minified client names such as `o.Z`.
- The first argument of `$fetch`, `ofetch`, `ky`, `got`, `useFetch`, `useSWR`, `request`,
  `$.ajax` or `new Request`.
- A `url` or `uri` in an options object that also has a method, `type`, `data`, `headers`,
  `body`, `params` or a similar request key, before or after the URL.
- A value under a name that contains `endpoint` or starts with `api` (`apiPath`,
  `userEndpoint`), including endpoint tables such as `ENDPOINTS = {LOGIN: "/auth/login"}`.

A `.get` or `.delete` on any other object (a `Map`, a cache, a router or a one-letter
minified name) still needs an API-shaped path. A `url` next to only a title or image stays
out, as do page-link tables and names such as `redirectUrl`. The evidence says which rule
matched, for example `.get() argument on an HTTP client` or
`property LOGIN of endpoints`. Comments and regular expressions are
skipped. These are rejected: text with spaces, asset files, wildcard and `..` paths,
`example.com`-style placeholder hosts, documentation hosts such as `docs.` or `developers.`,
IP addresses and `localhost`, and anything the browser
fetched and received as an HTML page.

A relative path found inside a named provider's own script (a tag manager or payment library,
for example) is that provider's business and is not listed against the inspected site.
Scripts on a CDN or object store are not treated that way, because sites host their own
bundles there.

## How rows are written

- Query strings are always dropped, from the row and from its evidence.
- Identifier-like parts of an observed URL are shown as `{id}`: UUIDs, runs of six or more
  digits, and long mixed letter-and-digit tokens, in a path segment or a subdomain label (the
  registered domain itself is never shortened). Several
  requests that differ only there become one row. The evidence keeps each URL as recorded.
- `{}` marks a value the code fills in at run time, from a template literal.
- A path with no host means the code did not state one. It is labeled "Host not stated" and is
  not resolved against the page.
- Rows are ordered: the inspected host and its subdomains, then paths with no host, then other
  hosts, with hosts of a named provider last. Within each group observed rows come first, and
  API-shaped URLs come before others of the same kind.

## Report and limits

The JSON report adds `api_endpoints` (method, url, host, path, scope, provider, basis, status,
meaning and source/value evidence) and `api_endpoint_coverage`. CSV and TXT carry both through
the existing complete-report exporter. WebSocket rows also appear in `browser.requests`.

Limits: 256 rows, eight evidence items per row with the remainder counted, 768 stored
references, 64 per script, the first 2 MiB of each script, 128 operations per description and
48 rules per robots.txt.
When a limit is reached the list says so and `api_endpoint_coverage.truncated` is true.

What it does not do: it does not confirm that a described, declared or in-code endpoint exists;
it does not see requests made only after a click, scroll or sign-in; it does not read request
or response bodies; and it does not find endpoints on pages the inspection did not reach. A
code reference can be inactive code or a bundled library's own endpoint.

## Verification

Checked on Windows on November 11, 2026, with the build in `build/`.

- **Unit tests** (`tests/api_endpoints_tests.cpp`, ctest `api-endpoints`): 125 checks pass. They
  cover the code rules with misleading negatives (comments, regular expressions, prose,
  documentation routes and hosts, assets, placeholder hosts, wildcard paths), request
  classification, identifier collapsing, merging, ordering, query removal, markup, robots.txt,
  OpenAPI and Swagger documents, the limits and the JSON export.
- **Loopback test** (`tests/api_endpoints_integration.py`, ctest `api-endpoints-network`): a
  local page whose code makes a fetch, an XHR POST, an event stream, a beacon and a WebSocket
  connection, with an API description, a robots.txt and a GraphQL-style answer on the same
  server. Without the browser the report has 17 rows and none is marked observed. With the
  browser it has 18 rows and all five kinds of request are marked observed, the WebSocket with
  HTTP 101. The server's own request log confirms that endpoints which were only named in code,
  markup, the description or robots.txt received no request at all.
- **Interface test** (`tests/api_endpoints_gui.py`): example data, a static inspection, the two
  empty states and a browser inspection, at 1280x800 and 1000x700 where it applies. It checks
  the five rows and Load more, single click, double-click and Enter for evidence, Back and its
  scroll position, New search, the ellipsis on a long row and that the host and DNS lists keep
  their own column and counts. Five rows and Load more fit the default window without scrolling.
  At 1000x700 the heading and the first row are visible and the rest needs a scroll.
- **Existing tests**: 20 related ctest suites pass on the same build, among them `core`,
  `report-export`, `report-export-formats`, `endpoint-connections`, `vendors` and
  `browser-network`. `tests/endpoint_connections_gui.py` (24 scenarios) and
  `tests/infrastructure_more_gui.py` pass, so the lists that share the row code are unchanged.
- **Live check, www.kraken.com**: 39 rows. Four requests the page itself made are marked
  observed (`GET iapi.kraken.com/api/internal/meta`, `GET iapi.kraken.com/api/internal/session/expiration`,
  `POST sg.kraken.com/v1/initialize`, `POST sg.kraken.com/v1/rgstr`), with third-party
  collection endpoints after them and 31 references from the site's code.
- **Live check, www.bybit.com**: three runs while the rules were being finished gave 64, 67
  and 71 rows. The last run, on the final build, has 27 API paths from the site's robots.txt
  and 44 from its code, and no observed call. The two earlier runs also had five observed
  calls and ten attempted calls to `api2.bybit.com`. The difference is the site, not the
  rules: its edge ends the headless browser's connection with `net::ERR_HTTP2_PROTOCOL_ERROR`,
  so how far the page gets differs between runs. Vector does not work around that. When the
  page's calls fail they are shown as attempts with the error, not as observed.

- **Packaged copy** (`dist/Vector`): its two programs are byte-identical to the tested build.
  `tests/package_smoke.py` passes, and the example-data, static and empty-state interface
  scenarios pass against the packaged `Vector.exe`.

Not checked: display scaling other than 100%, sites that need a sign-in, and inspections
through a proxy. A full ctest run was not repeated after the last change; teammates' full runs
earlier the same night passed this suite's unit test and failed only on browser-profile
cleanup and time limits while several runs shared the machine.

Evidence: `docs/reports/api-endpoints/` holds the loopback report (`integration/report.json`), the
interface captures (`gui/`), the two live reports and the live Overview capture (`live/`), and
the Overview before the change (`before/demo.png`). Test output is in `build/logs/api-endpoints-*.log`.
