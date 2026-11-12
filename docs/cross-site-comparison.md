# Cross-site StackFull comparison

On November 9, 2026, the operator explicitly requested live comparisons on random
public sites. Six sites were sampled without replacement from 20 public software
sites using seed `158825077150378182`. This was a bounded web-only comparison, with
one StackFull request and one initial Vector inspection per site, followed by one
updated Vector inspection. No LAN targets, vulnerability probes or authenticated
sessions were used.

## API and retained evidence

StackFull's delivered client submits:

```http
POST https://www.stackfull.lol/api/scan
Content-Type: application/json

{"domain":"example.com"}
```

The six responses succeeded. The private implementation is not exposed by that
client. This API is used only by `scripts/compare_stackfull.py`; Vector does not
call it during an inspection. The helper records the sample, DNS addresses,
timestamps, executable hash, request outcomes and response hashes. Its `rerun`
action reuses the saved API responses rather than making additional API calls.

The baseline is `dist/Vector-Navigation/vector-cli.exe`, SHA-256
`c74641721bf678db2192e290a11968447736931f97c453a1d71c3d237c3ff4dc`.
All captures are under `docs/reports/cross-site-comparison/`; initial results are in
`comparison.json`, updated results in `updated/comparison.json`, and concise
before/after data in `summary.json`.

## Results

| Site | StackFull detected claims | StackFull provider claims | Vector named entries before → after | Published names before → after |
| --- | ---: | ---: | ---: | ---: |
| nuxt.com | 11 | 0 | 3 → 4 | 0 → 0 |
| plausible.io | 5 | 5 | 1 → 3 | 0 → 4 |
| astro.build | 3 | 0 | 3 → 5 | 0 → 0 |
| netlify.com | 28 | 35 | 6 → 9 | 26 → 26 |
| grafana.com | 32 | 2 | 7 → 11 | 0 → 8 |
| sentry.io | 16 | 9 | 7 → 9 | 0 → 0 |

Vector's counts use the Overview evidence eligibility rules, including its DNS
roles. They exclude raw fingerprints, unattributed origins, policy-only hints,
preconnects and code literals. Published company relationships are counted
separately. Nuxt/Nuxt.js, Vue/Vue.js and Plausible/Plausible Analytics are explicit
name aliases in the comparison. These are name inventories, not accuracy scores
or directly equivalent measurements. A provider's legal entity name also need
not equal StackFull's product label.

The updated live run includes concurrent generic frontend/CSS work from Sorrel
and provider extraction work from Mica. The increase from 27 to 41 named entries
across the six inspections is therefore an integrated result. Sentry's later run
did not repeat its earlier Google Fonts contact; both outcomes are retained.

## Changes made from this comparison

- Known resource paths on shared hosts now identify Fathom Analytics, OneTrust,
  Plausible Analytics, reCAPTCHA and LinkedIn Insight Tag. Host boundaries, exact
  paths or bounded path families are required; query strings cannot supply a
  match. The matching path stays in evidence with query values omitted.
- An advertised Docusaurus generator tag supplies its name and exact version.
  Ordinary text and unrelated metadata do not qualify.
- Linked documentation landing pages precede robots.txt and ordinary links.
  Guessed sitemap paths follow known page links. Entry-page priority and all
  page/request/time limits remain intact; sitemap discovery remains available.
- Disclosure discovery recognizes compliance labels and requires document-like
  labels or relevant path segments. Astro's showcased Proton and NordVPN titles
  no longer consume disclosure transfers. Repeated accessible/visible labels
  still work, including hosted privacy links.

The captured Plausible documentation advertises `Docusaurus v3.9.2`. Its initial
inspection never reached that page; the updated inspection does. Astro's new
report retains Fathom's matching resource evidence and makes only the two actual
legal-source attempts, instead of adding two unrelated showcase destinations.

An offline replay of **unchanged baseline browser requests** isolates seven added
named integrations across four sites: Fathom on Astro; OneTrust and reCAPTCHA on
Netlify; OneTrust and LinkedIn Insight Tag on Grafana; Plausible and reCAPTCHA on
Sentry. This requires no network access and does not depend on the later pages
changing. See `replay/verification.json` and each replay report.

The attribution references are the providers' own documentation:
[Fathom embed](https://usefathom.com/docs/script/embed),
[OneTrust SDK](https://developer.onetrust.com/onetrust/docs/using-tcf-with-header-bidding),
[Plausible script delivery](https://plausible.io/docs/proxy/introduction),
[reCAPTCHA resources](https://developers.google.com/recaptcha/docs/display),
[alternate reCAPTCHA hosts](https://developers.google.com/recaptcha/docs/faq), and
[LinkedIn Insight Tag](https://learn.microsoft.com/en-us/linkedin/marketing/conversions/deduplication).

## Verification and remaining gaps

`tests/cross_site_tests.cpp` checks resource attribution, host/path lookalikes,
query redaction, weak evidence exclusion, source labels, crawl priorities and
generator evidence. `tests/cross_site_integration.py` generates a previously
unknown supplier and opaque disclosure path at runtime. The old package misses
both the documentation page and that supplier; the updated collector finds them
within the existing cap. A second scenario checks sitemap fallback.

One focused review covered these changes and their integration. It found and
corrected repeated accessibility-label handling, with positive and misleading
negative regression cases. It did not audit unrelated teammate changes.

Remaining differences are retained item by item. Documentation subdomains remain
outside the ordinary same-host crawl. Some pages, rendered sources and delayed
or consent-dependent resources exceed the request/time limits. Product names
inside arbitrary bundles still need independent fingerprints. StackFull supplies
aggregate source URLs but no per-item matched bytes, so a larger returned list
alone cannot validate every claim. No unverified difference was filled with a
per-company answer list.

Routine CTest verification remains offline or loopback. The public comparison
helper is an explicitly invoked research action, not an automatic test.

## Delivered build

`dist/Vector-Cross-Site/Vector.exe` and `vector-cli.exe` contain these changes.
The final Release build completed without compiler warnings. Verification passed:
63 cross-site evidence checks, 43 existing vendor checks, 54 disclosure checks,
two new loopback scenarios, the existing vendor network cases, 12 generic source
discovery scenarios, and the shared web-cap network suite. Two native SDL
scenarios verified the new names and matching path evidence at 1280x800 and
1000x700. The packaged GUI and CLI launch with only Windows System32 on PATH.

The live rerun precedes the final narrow correction for duplicated accessibility
labels; that correction was verified locally without another public-site run.
Evidence: `cross-site-reviewed-build.log`, `cross-site-reviewed-tests.log`,
`cross-site-release-tests.log`, `cross-site-gui.log`, `cross-site-package.log`,
and `docs/reports/cross-site-comparison/package-verification.json`.
