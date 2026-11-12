# Complete report export

On any completed, cancelled, failed or example report, choose the export icon
and select **JSON**, **CSV**, **TXT**, or **Copy to clipboard**. File export saves
the report already in memory without starting an inspection, making network
requests, opening another program or changing its findings. Copy to clipboard
places the same JSON on the clipboard and does not write a file. The current
view and filters do not restrict the data.

Files go in the `reports` folder beside `Vector.exe`. The app shows the complete
saved path or a write error. Timestamped filenames include the selected extension;
repeated exports use a numbered suffix instead of replacing an existing file.
Files use UTF-8, including Unicode filenames and collected text.

All formats derive from the same report serialization. They include the target,
addresses and reverse DNS, status and timings, services, web headers and
certificate metadata, software findings and their evidence, scripts, browser
request metadata, provider relationships and sources, registry and geolocation
records, reachability, notes, coverage limits and activity. Collection limits and
evidence qualifications stay attached. Existing privacy omissions are unchanged:
browser request cookies, headers, bodies and query values are not exported. Raw
page/script bodies are not part of the report; their metadata and matched evidence
are included.

- **JSON:** the existing nested report schema, also used by the CLI.
- **CSV:** `Field,Type,Value`, with one row for each report value. `Field` is a JSON
  Pointer such as `/findings/0/evidence/0/value`. Array indexes preserve repeated
  observations. Empty arrays and objects get rows too. `Value` is a JSON literal:
  strings keep their surrounding quotes and escape sequences, preserving commas,
  newlines, Unicode and empty strings without spreadsheet formula execution.
  CSV double quotes are escaped according to the CSV format.
- **TXT:** an indented hierarchy of the same fields, with numbered array entries.
  Values use JSON literals so multiline evidence and control characters remain
  unambiguous. There is no results cutoff or text preview truncation.

The existing CLI `--output` option and stdout remain JSON. The three file
formats and Copy to clipboard are available in the desktop export menu. Copy
uses the JSON serialization.
