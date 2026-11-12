# Detection coverage corrections

Verified on Windows on November 8, 2026. This change addresses collection and matching gaps reproduced against the previous `Vector-WebCap` release. The user's exact X.com report was unavailable at that release. It has since been recovered, with the cause and further corrections documented in [X inspection diagnosis](x-inspection-diagnosis.md).

## Behavior

- A rejected or failed static HTTP read no longer prevents a normal isolated browser navigation. TLS verification, navigation boundaries and authentication handling still apply. Browser HTTP rejections retain the main-document status in `browser.pages[].status`, the reason and activity; a settled HTTP error page reports `http_error`.
- Entry pages across eligible web ports precede deeper static crawl links. Bare hosts prefer HTTPS on port 443, while explicit endpoints retain priority. The existing shared 12-page, 40-static-script and 30-second limits remain in force; reaching the time limit can still leave endpoints unvisited.
- JavaScript matching uses the browser's advertised 2 MiB analysis window. It previously silently clipped that sample to 512 KiB. Static response collection remains limited to 512 KiB. Browser bundles up to 4 MiB can supply an analyzed, explicitly truncated 2 MiB prefix; larger bodies are skipped.
- Production React can be identified from paired renderer properties on actual DOM elements, including a fiber whose `stateNode` is that element. The walk is bounded and checks data properties without reading arbitrary getters. This finding has Medium confidence and no inferred version. Mismatched properties are a negative fixture.
- Selected main-document browser response headers feed the existing server/framework/CDN matcher. Each finding retains the matched value and a `Browser response` source. A narrow Fastly delivery-node header signature was added; network ownership alone does not establish the serving technology.

The desktop remains automatic. Its main report still contains only Infrastructure and Endpoint. Full evidence is under **Show all information**. No vendor inventory, company catalog, authenticated permissions or additional scan controls were added.

## Before and after

`tests/coverage_integration.py` serves genuine, unmodified MIT-licensed React 18.3.1 production builds locally. Their sources, hashes and licenses are retained in `tests/fixtures/react-18.3.1/`. Test variants remove globals, put code after 600 KiB, load an oversized renderer bundle, reject static requests, and put a link flood before a second web endpoint.

| Check group | Previous packaged build | Updated build |
| --- | --- | --- |
| Nine coverage scenarios, 14 assertions | 10 failed, 4 passed | 14 passed |
| Full CTest run | Not rerun as a baseline | All 9 suites passed |
| New native SDL scenarios | Not run as a baseline | Both passed and captures inspected |

The failures in the previous build covered the second endpoint, browser fallback after HTTP/transport failure, matching later script bytes, retaining an oversized bundle prefix, finding React without globals or downloaded source, reporting browser HTTP errors and identifying browser-only headers. Positive results are limited to the served fixtures; they do not establish a match count on X.com or parity with StackFull.

Validation commands:

```powershell
python tests/coverage_integration.py dist/Vector-WebCap/vector-cli.exe --baseline
cmake --build build-runtime -j 8
ctest --test-dir build-runtime --output-on-failure
python tests/coverage_gui_smoke.py build-runtime/bin/Vector.exe .impeccable/review/coverage
```

The full run includes 78 core, 28 vendor and 83 library checks; the new coverage scenarios; shared web-cap, browser and library fixtures; and existing transport tests. Evidence: `build-runtime/Testing/Temporary/LastTest.log` and `.impeccable/review/coverage/` (native captures and JSON). One focused review checked collection ordering, bounds, cancellation/error handling, attribution and the resulting screens. No additional required defect was found in that pass.

`dist/Vector-Coverage` passed the packaged GUI and CLI launch check with only Windows System32 on PATH. The packaged CLI also executed the real production-renderer fixture and identified React without globals under that restricted PATH. Its retained report is `.impeccable/review/coverage/packaged-react.json`.

## Remaining limits and research

An identified technology needs observed evidence. Unrecognized libraries, blocked or interaction-gated resources, loads after the observation window and private server dependencies can remain unidentified. Company disclosures are a different source from code served by an endpoint; the earlier [StackFull investigation](stackfull-research.md) explains that distinction. Comparing only total hit counts mixes those sources.

The screenshot alone did not establish which responses or scripts were collected. The subsequent recovery of the existing report and retained HTML resolved that evidence gap; see [X inspection diagnosis](x-inspection-diagnosis.md). X.com was not re-scanned during these tests.

Implementation references:

- [React's DOM component tree implementation](https://github.com/facebook/react/blob/main/packages/react-dom-bindings/src/client/ReactDOMComponentTree.js) defines paired renderer properties. The local production fixture confirms the actual shape used here.
- [Fastly X-Served-By documentation](https://www.fastly.com/documentation/reference/http/http-headers/X-Served-By/) describes the observed delivery-node header.
- [Browser runtime behavior and limits](browser-runtime.md) documents the full collection contract.
