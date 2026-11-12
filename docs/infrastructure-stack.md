# Infrastructure and technical information

The default Infrastructure column lists named technologies and integrations, such as nginx and YouTube. The separate Info section contains protocol names and port numbers, service software and distribution-package versions. OS/device hints remain in Endpoint. All technical evidence remains available through the Information sidebar, and JSON, CSV and TXT exports retain the full report.

Recognized third-party resources now appear in Infrastructure instead of being available only in Vendors. A provider appears once, with the strongest available evidence selected for its detail view. Published company relationships retain their separate subsection. Policy allowlists, preconnect/preload hints, code URL literals and unknown owners alone do not become infrastructure entries.

YouTube attribution covers `youtube.com` and `youtube-nocookie.com` resources. Player scripts and embedded frames keep their reference or observed-response evidence. An active form input whose URL-shaped placeholder requests a YouTube `/watch?v=` URL identifies YouTube as a **supported source**. This covers converter-style interfaces without submitting a form or claiming a server-side connection was observed. Plain prose, ordinary hyperlinks, disabled/hidden/readonly inputs, inactive templates and deceptive hostnames do not establish this source relationship. Query values are omitted from retained evidence.

The source rule was based on the visible form structure of [YTMP3](https://ytmp3.gl/); the player resources follow the [YouTube IFrame API](https://developers.google.com/youtube/iframe_api_reference) and [privacy-enhanced embedding documentation](https://support.google.com/youtube/answer/171780). No third-party detection catalog was copied.

Regression coverage is in `tests/vendor_tests.cpp` and `tests/infrastructure_gui_smoke.py`. The latter runs loopback fixtures with hidden SDL windows at 1280×800 and 1000×700. It does not scan a public site or execute page scripts.
