# Technology icons

Vector places an identifying mark next to technology/provider names in the Infrastructure and Info overview rows, Software and Vendors lists, and their evidence headings. Existing names, qualifications and evidence navigation remain in place. Protocols and unrecognized products receive a neutral package, server or relationship icon.

The initial catalog contains 129 brand SVGs with 210 explicit aliases. Lookup trims surrounding whitespace and folds ASCII case; it does not match substrings, derive brands from arbitrary domains or infer new findings. For example, `YouTube` has its logo and `youtube.example` does not. Explicit legal-name aliases use the corresponding company mark for presentation only. The icon does not strengthen or change the underlying evidence.

A second pass (November 11, 2026) added marks for names Vector already reports that showed the neutral icon. It added 158 SVGs, and 95 more names now use existing marks. The catalog is now 290 SVGs with 580 names. Names were matched by the collection's own title or alias, or after removing a role or company-form ending: `HubSpot mail authorization` and `Snowflake, Inc.` use those companies' marks. A company's own named service uses the company mark: `Amazon S3`, `Amazon Route 53` and `AWS Amplify` show AWS, `Google Workspace mail` shows Google and `Azure DNS` shows Azure. Matches on a generic word were checked against the source of the name, and wrong ones are refused: `Crystal Computing SRL` is not the Crystal language and `Cinder Technologies Inc.` is not the libcinder library.

Some names deliberately keep the neutral icon:

- Fingerprints and origins that are not brands, such as `HTTP header order`, `HTTP cookies` and `Unattributed origin`.
- Adobe products such as Adobe Experience Platform Tags. Adobe's trademark terms do not allow third parties to use its logos, and Simple Icons removed them at Adobe's request.
- Brands with no openly licensed mark in any of the three collections, for example TrustArc and OneTrust.
- Open-source libraries without a mark of their own. These are not given a sponsor's or company's logo: `web-vitals`, `Turbopack`, `Floating UI`, `DOMPurify`, `Day.js` and `rlottie`.
- Subsidiary legal names from published provider lists whose parent brand has no openly licensed mark.

## Assets and provenance

- [Simple Icons 16.37.0](https://github.com/simple-icons/simple-icons/tree/16.37.0): brand paths with their catalog color applied to the SVG root. The repository license and disclaimer ship with Vector. Individual source, license and guideline metadata is retained per asset.
- [Devicon](https://github.com/devicons/devicon): original Google, Windows, Azure, AWS, AngularJS and Apache SVGs. Later additions come from v2.18.0, commit `ff435a0fff0391aefbf70d63a3fe7cfe64f82df0`: core-js, Envoy, Handlebars, Heroku, LinkedIn, Motion, OpenAPI, Salesforce, Slack and Twilio. These files are stored unmodified. The precise commit and source URL are recorded in the manifest; its MIT license ships with Vector.
- [SVG Logos](https://github.com/gilbarbara/logos) (gilbarbara/logos, CC0 1.0), commit `b9edc65f8946027cb5e2fe6c772e914cf3649fba`: used only where the first two collections have no mark. Examples are Tealium, OpenAI, Amplitude, Optimizely, Segment, SendGrid, reCAPTCHA, hCaptcha, LaunchDarkly and Braze. The square `-icon` variant is preferred and stored unmodified. Its license ships as `SVG-Logos-LICENSE.txt`, and its Adobe marks are not used.
- Every new file was screened for SVG features NanoSVG cannot draw: `use`, `mask`, `clipPath`, `filter`, `pattern`, `image`, `text`, `style` elements and `class` attributes. None of the added files needed any of them.
- Two Simple Icons catalog colors are near white: Base UI `#EDEDED` and TanStack `#ECE8D1`. Those two marks are drawn in black so they show on the light tile. Their manifest hash still records the catalog-colored source.
- [NanoSVG](https://github.com/memononen/nanosvg): unmodified parser and rasterizer headers, pinned in `src/third_party/nanosvg/SOURCE.md`, with their zlib license.

`src/assets/technology/manifest.json` records each asset's source URL, revision, display names and SHA-256. These are identification marks, not an affiliation claim. CMake embeds the SVGs and alias data into the executable. There are no runtime icon downloads, favicon lookups, new API keys or loose image files to deploy.

Icons rasterize on first use to cached 96-pixel RGBA textures. Rendering preserves aspect ratio, pads the mark inside a light neutral tile and scales dimensions through the existing display scale. Unknown names or failed rendering use a neutral vector fallback; the text is always retained. Textures are released before their renderer is destroyed.

## Verification

`vector-icon-tests` validates exact-name boundaries and all aliases, renders every bundled SVG in a hidden software renderer, then repeats after teardown and reinitialization. The gallery is drawn in pages of 132 icons. The first page is saved at the given bitmap path, and later pages get `-2`, `-3` and so on before the extension. `tests/technology_icon_smoke.py` covers both supported window sizes and evidence selection using only example data and a loopback fixture. Evidence is saved under `docs/reports/technology-icons`.
