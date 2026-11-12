# Offline disclosure response fixtures

`provider-tables.json` contains factual table rows retained from official X,
Google Cloud, Google Workspace/AppSheet and Chrome Enterprise documents on
November 9, 2026 UTC. Each record keeps the source URL and retrieval date. The
Google records also retain the original document SHA-256.

These are **test inputs only**. Test HTTP servers render their rows into responses
on loopback. The fixtures have no company-to-target mappings, are never linked
into Vector, and must never be copied into a release package. A provider may
appear in an inspection only after its supporting document is collected.

The varied column labels, captions, section roles, Unicode, footnotes and long
documents exercise the parser. Known fixture counts measure extraction of those
particular responses; they do not promise a live site's result count.

The independent discovery tests use newly generated names and other target hosts
to check that collection does not depend on these fixtures.
