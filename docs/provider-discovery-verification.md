# General provider discovery verification

This verification addresses the request to discover provider names from documents instead of shipping company-specific provider lists. It does not treat a larger count as proof of better detection. Published relationships remain separate from software observed at the inspected address.

## Behavioral checks

`tests/provider_discovery_integration.py` creates a new `.localhost` hostname and new provider names after compilation. Its local HTTP service controls the returned documents. The executable has never seen those names before the run. The scenarios cover direct sources, opaque document links, legal/privacy link chains behind ordinary product links, duplicate references, changed documents on the same hostname, failed sources, customer tables, hidden content, script examples, unrelated origins and absent disclosures. Each collected name must retain its source, original row and collection date; it must not become a software finding.

Run against any CLI build:

```powershell
python tests/provider_discovery_integration.py path/to/vector-cli.exe --out docs/reports/provider-discovery/current
```

On Windows this check is registered as `provider-discovery-network` in CTest. The latest isolated build passed that test in 8.59 seconds (`discovery-check-ctest.log`). This is a targeted check, not an all-suite claim.

The old `dist/Vector-Google-Providers/vector-cli.exe` passed 10 of 12 cases. It already extracted new names from direct provider tables. It failed the opaque-link and legal-chain cases when unrelated page links exhausted the crawl budget. The saved baseline contains the executable hash, generated names, actual requests and complete reports: `docs/reports/provider-discovery/baseline/verification.json`.

## Captured public documents

The current [Default provider document](https://www.default.com/legal/subprocessors) contains 22 provider heading sections. Its original HTML, independent heading inventory, source URL, date and SHA-256 are retained under `docs/reports/provider-discovery/default*`. The old parser and local replay both returned zero providers from this document.

`tests/provider_document_replay.py` serves the captured provider text and structure on a newly generated loopback hostname and compares the output with the independently recorded headings. For local replay, only `href` attributes are changed to `#`, preventing discovery from following links to public sites. Browser execution is disabled. The original capture and adapted replay each retain their own SHA-256; the expected names are verification inputs only, never packaged data.

Replay preparation was checked to preserve every text node and every non-`href` attribute while neutralizing 88 link attributes. Both the old and new executable were checked against this adapted document.

```powershell
python tests/provider_document_replay.py path/to/vector-cli.exe --out docs/reports/provider-discovery/document-current
```

Other captured pages show limits that must not be hidden: the [Linear trust page](https://trust.linear.app/subprocessors) initially returns an application shell, and the [Supabase provider page](https://supabase.com/legal/customer-resources/subprocessor-list) links to PDFs. Those captures do not contain inline provider rows. An HTML extraction success on other pages does not establish support for these sources, their runtime data or their PDF content. No public-host Vector test scan was performed.

## Result status

The replacement CLI passed all 12 generic discovery scenarios, including the two baseline failures. Complete reports and the executable hash are in `docs/reports/provider-discovery/corrected/verification.json`. The legal/privacy case retains all three discovered links in both source metadata and each provider's evidence.

The captured Default replay still returned zero of 22 names (`docs/reports/provider-discovery/document-corrected/verification.json`). Its structure is a disclosure `h2`, followed by sibling `h3` provider names, `Address:` paragraphs and a service-description paragraph. The current structured-row helper expects a containing card/list/definition row and clears disclosure context when it reaches each provider heading. It also does not recognize Address as a supporting field. This is a concrete unsupported format, not a successful verification or a reason to add those names to production data. Broader document coverage is therefore not established by the 12 passing cases.
