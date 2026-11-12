# Structured provider rows

The provider collector can extract names from HTML tables and from additional
structured disclosure formats. `src/disclosure_rows.hpp` implements the latter;
it contains no company names, domain mappings, provider catalog or logo lookup.
The collector supplies a document whose discovery chain establishes its source.

The additional formats are:

- A definition list naming each organization, with labeled purpose or location
  fields in its description.
- A list item, article or card with a provider name and labeled service, purpose,
  product or location fields. The name may be an h3–h6 heading, bold text or an explicit
  Company/Provider/Name field.
- A disclosure card whose logo alt text exactly matches the visible name beside
  a separate description. The logo address is not used to identify a company.
- A list item that starts with one bold provider name followed by a colon and a
  description. The original name, including any country suffix, is preserved.
- A complete group of adjacent two-column rows: a name in the first text cell
  and a description in the second. At least two adjacent rows must share that
  structure. CSS class names and provider-name dictionaries are not consulted.

An explicit visible disclosure heading must precede the rows, or a visible
panel/region must have an accessible disclosure label. `aria-labelledby` resolves
the actual visible label text; a URL, DOM ID or CSS class is not a provider cue.
Later headings and semantic section boundaries end that context. Customer sections, update
notices, ordinary prose, scripts, examples, hidden nodes and incomplete rows do
not supply names. The existing table parser remains responsible for tables.
Table name columns also accept `Entity`, `Sub-processor Name`, `Subprocessor name`,
`Provider name`, `Vendor name` and `Service provider name`. They still need a
disclosure heading and a separate processing, service or location column.
For tables, a deeper column-description heading such as `Entity Name & Location`
retains its explicit disclosure parent. Empty formatting headings are ignored;
same-level headings and unrelated subsections still end the table context.
A semantic `header` can contain document content. Navigation elements and
elements with `role="navigation"` remain excluded by the structured reader.

Every extracted row retains its name, section label, and the document's field
labels and values. Unlabeled card descriptions use the normalized field label
`Description`. Rows for the same name with different purposes remain separate
evidence. The caller adds the source URL, discovery chain and collection date,
and keeps these published relationships separate from observed software.

Parsing is bounded to the first 3 MiB, 150,000 tokens, 24,000 visible nodes,
96 nested elements and 128 distinct structured rows. A row contains at most
12 fields, each with a value of at most 1,024 bytes. Hitting an overall extraction
bound is reported through `Result.truncated`. This is a limited extractor, not a
complete HTML browser or an interpretation of arbitrary prose. External styles,
PDFs, images of tables and provider data present only inside application scripts
are outside this helper. Source discovery and browser collection have their own
limits.

The generic card format was checked against a captured official
[Notion disclosure page](https://trust.notion.com/subprocessors). Capture metadata
and the unmodified document are stored locally under `docs/reports/disclosure-rows/`;
that research capture is not a bundled provider database. Definition-list
structure follows the [HTML standard](https://html.spec.whatwg.org/multipage/grouping-content.html#the-dl-element).

## Verification

Verified with an isolated Release engine build on November 9, 2026 UTC:

- 49 structured-row checks and 36 existing disclosure checks pass.
- Three loopback HTTP scenarios pass using new randomly generated company names
  on every run: collected rows, HTTP-rejected content and hidden/customer content.
- Offline replay of the captured Notion document through `identify_disclosures`
  returns all 37 names in its active panel, including affiliate entries. Every
  description and source URL was compared with the corresponding visible card;
  none became a software finding. This verifies document extraction, not live
  discovery from Notion's homepage or universal coverage.
- The final targeted build emitted no compiler warnings.

The single focused review found wrapper containers could merge same-name cards
or skip a later customer heading. The corrected parser preserves the separate
purposes and respects that boundary; regressions cover both cases and nested
name formatting. No additional broad review was performed.

Evidence: `disclosure-rows-tests.log`, `disclosure-rows-build-final.log`,
`docs/reports/disclosure-rows/verification.json`, and
`docs/reports/disclosure-rows/notion-report.json`. Research captures remain outside
the production build.

```powershell
cmake --build build-disclosure-rows --target vector-disclosure-row-tests vector-disclosure-tests vector-cli
ctest --test-dir build-disclosure-rows -R '^(disclosure-rows|disclosures|disclosure-row-network)$' --output-on-failure
```
