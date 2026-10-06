# Masking — token classification → template identity

Masking turns a line's `content` into a **stable template** (`template_str`) and its identity
(`template_id`). It is what lets the engine say "this is the same *kind* of line as before" while a request id,
a timestamp, or a latency value varies. This doc is the authoritative current-state reference for **what canon
masks, what it keeps, and which markers it knows** (the rules generation named by
`kCanonicalizationVersion` in `core/api/canon.api.cppm`, which owns the value).

---

## 1. The model — stateless, per-line, keep-class / mask-instance

- **Stateless & per-line.** A template is a pure function of **one line's own tokens**, given the stream's
  declarations — no cross-line learning, no clustering memory. The same logical line under the same declarations
  yields the same template in any run, any order, on any machine. (This is why identity is reproducible and why a
  "phantom pair" from learned wildcards cannot occur.) The declarations are fixed before the line is read: the
  composed dialect, the `MaskConfig`, and the stream's **declared context** — values the acquirer supplies about
  the run, such as its own pull-request number (`StreamContext`). A declared value is a supplied
  fact, never learned state; a stream that declares none templates byte-identically to one that has no context
  at all.
- **Keep-class / mask-instance.** The unifying idea behind every rule: keep the **stable class marker**, mask
  the **varying instance**. `#42 → #<*>` (keep the counter marker, mask the number); `file.cc:408 →
  file.cc:<*>` (keep the source file, mask the line); `order=123 → order=<*>` (keep the key, mask the value).
- **The discriminator is `digit-leading`.** A token (or sub-part) that begins with a digit is a
  number/measurement/version/timestamp — intrinsically high-cardinality — and masks; a token that begins with
  a letter is a word/keyword and is kept. One rule subsumes numbers-with-separators, decimals, number+unit, and
  versions, with **no unit lexicon**: `512MB`/`6.2s`/`76.5%` mask (digit-leading); `sha256`/`utf8`/`x86` keep
  (letter-leading).

- **A line's ending is not content.** Masking never sees it: canon removes the run of carriage returns closing
  each line at its doors, before format detection ([formats.md](formats.md) § 1), so a CRLF line masks exactly as
  its LF twin. A carriage return anywhere else in a line is content, and a rule that reads a token's last bytes
  keeps it literal.

The wildcard placeholder is **`<*>`**. Tokens are split on **whitespace only** for masking (the structural
tokenizer used by classification is separate — see [classification.md](classification.md)). Kept/normalized
tokens contribute their text to `template_str`; **fully-masked** tokens contribute a `<*>` *and* push their raw
value into `params` (a normalized composite that embeds `<*>` contributes **no** param — it is a kept class,
not a masked instance). A token whose **final normal form is exactly `<*>`** — a whole dashed UUID the
embedded-identity rule claims, a source token that is literally `<*>`, a form a later step reduces to the bare
wildcard — is fully masked too: it contributes `<*>` and pushes its source token. So the template states the
binding by itself: **a template token that is exactly `<*>` is a param and every param is one, param *i* being the
(*i* + 1)-th such token**, and a `<*>` inside a token is a normalization with no param.

---

## 2. The total precedence order

Each whitespace token is classified by the **first** matching rule:

| # | Rule | Disposition |
|---|---|---|
| 1 | **Status-value KEEP** — an all-digit token, ≤ 3 digits, immediately after a **status keyword** | KEEP literal |
| 2 | **Composite** — the token carries a structural delimiter; one of the normalizers (§4) matches. The hash counter reads its token through a **complete** wrapper shell (§3.2), and the ephemeral-root reader finds a path behind a declared **lead** (§3.1) | KEEP normalized (embeds `<*>`, no param) |
| 3 | **UUID / long hash** | MASK `<*>` |
| 4 | **IPv4**, bare or inside a declared **wrapper shell** (§3.2), with at most two trailing closers or `,;:.` | MASK `<*>` when `mask_ip_addresses` is on; KEEP literal when it is off — rule 4 decides every token it accepts, so no later rule reaches one |
| 5 | **Digit-leading numeric**, or a digit-leading core inside a **complete** wrapper shell (§3.2) | MASK `<*>` — this also carries `0x`-hex: a `0x…` token starts with a digit. The shelled form masks whole and its raw token is the param; a short status value behind a status keyword stays literal through the shell, as rule 1 keeps it bare |
| 6 | **Literal** — none of the above | KEEP literal |

Before the first token is read, a `content` that is, whole, one JSON object or array is given its **member-order
normal form**: the members of every object are permuted into name order, every other byte staying where it was
([formats.md](formats.md) § 5). Every other `content` is tokenized as it is.

After rules 2 and 6 — on the token's **normal form**, the literal token or the form a composite gave it, and
never on a token rules 3–5 masked whole — two further steps run, in this order, and neither claims the token or
contributes a param (§4, the last two rows):

- the **`;`-segment** step: the form is cut at `;`, and each `<key>=<digit-leading value>` segment has its value
  masked over its number's **extent** (§4), the kv-value rule's disposition applied per segment: a remainder of
  closers and `,;:.` is swallowed, any other remainder stays literal behind `<*>`;
- the **declared-run** step, only on a stream that declares a value under a key its dialect declares: each
  maximal digit run equal to the declared value, directly behind a declared marker, becomes `<*>`.

Rule 1 wins first on purpose: it protects the **green→red distinction** that downstream diffing depends on —
`exit code 0` and `exit code 1` must stay *different* templates, so a short status value after a status keyword
is never masked (see §3). The composite layer (rule 2) is gated by a cheap pre-check: a token is only tried
against the normalizers if it contains one of `: / # - =`, a **wrapper byte** (§3.2), or a declared marker
prefix; everything else skips straight to the fixed masks.

---

## 3. The declared catalogs (the "which markers")

Every catalog below is **frozen and declared** — a closed set, extended only on measured evidence, never
data-learned. This is what keeps masking decidable and deterministic.

| Catalog | Contents | Purpose |
|---|---|---|
| **Status keywords** | `code`, `status`, `exit`, `signal` (case-insensitive) | Rule 1 — the keyword before a short numeric value that must stay split (status codes, exit codes). |
| **Max status digits** | `3` | Rule 1 — a status value masks if longer (it's an id, not a code). |
| **Currency markers** | `$` (ASCII; structured to add `€`/`£`/`¥` as declared byte sequences if a corpus shows them) | §4 marker-number — a leading currency symbol glued to a number. |
| **Ephemeral roots** | `/tmp`, `/var/tmp`, `/var/folders`, `/private/var/folders`, `/private/tmp`, `.conan2/p/b`, `/nix/store`, `AppData/Local/Temp` — each carrying a declared **anchor** + **scope** (§3.1) | §4 ephemeral-root — a path segment directly under a per-run build/temp root is an instance and masks; the root is kept. |
| **Min hash length** | `16` | Rule 3 / §4 embedded-identity — a hex-only run this long is an instance hash, not a word. |
| **Wrapper pairs** | `[]` `()` `{}` `<>` `""` `''` | §3.2 — the punctuation shell a producer wraps a whole token in. Read by rule 4's grammar, by rule 5's shelled form, by the hash counter's shelled form, by the ephemeral-root lead (§3.1) and by the rule-2 pre-gate. |
| **Wildcard** | `<*>` | The mask placeholder. |
| **Declared-value markers** | none in core: each key and its markers are a dialect's data (`DeclaredValueRow`); the GitHub dialect declares `pull_request` behind `PR-`, `pr-`, `Pr-`, `pull-`, `PULL-`, `Pull-`, `PR#`, `pulls/` | §4 declared-run — the markers a run's own declared value is masked behind. Core holds the mechanism and no marker. |

`mask_ip_addresses` is the one `MaskConfig` knob (default **on**) gating a rule — rule 4 — and it decides
rule 4's **whole** acceptance set: `10.20.30.40`, `(10.20.30.40)` and `[10.20.30.40],` mask with the knob on
and all stay **literal** with it off. Rule 4 runs before rule 5, so neither the digit-leading rule (which would
mask the bare form) nor rule 5's shelled form (which would mask the complete shell) can reach an address the
switch keeps: a switch named for IP addresses that left some masked would break its own name. An address a
composite claims first (`10.0.0.1:8080` as a location, a URL, `ip=10.0.0.1`) is rule 2's, outside the knob's
domain, and is unchanged by it.

### 3.1 The ephemeral-root catalog — the root is the decidable thing

**No hex/length rule can separate an ephemeral token from content:** the same 40-character SHA is a *pinned
dependency* in one path and *per-run junk* in another — same length, same alphabet, opposite class. What **is**
decidable is the **root** — an enumerable, byte-exact catalog of build/temp directories whose immediate child
is a per-run instance *by construction* (a conan build dir, a nix store hash, a randomized `/tmp` dir). Each
entry declares two axes; both are **explicit, never inferred** — a mis-declared root over-masks, and
over-masking destroys signal irrecoverably, so the dangerous choices are named on purpose:

- **anchor** — `TokenStart`: the root's first component is the **path's** first component. A path starts at byte 0
  or right after a declared **lead**, and the byte where it starts is a separator. The lead grammar is closed: a run
  of wrapper openers (§3.2, read from the same table), then optionally `<key>=` (the key one or more of
  `[A-Za-z0-9_.-]`, optionally followed by more openers), then optionally `file://` (a file URL with an empty
  authority, whose path is the absolute path). So `/tmp/run-a1`, `(/tmp/run-a1/x.ts:12:5)`, `"/tmp/run-a1/s.json",`,
  `TMPDIR=/tmp/run-a1` and `file:///tmp/run-a1/x.ts` all start their path at `/tmp`. A lead moves **where** a root may
  sit, never what it matches: `/home/u/proj/tmp/x` and `build/tmp/x` stay literal, because `tmp` is not the path's
  first component. `Floating`: the root matches at **any** component boundary, so a mid-path root stays visible.
- **scope** — `Subtree`: everything under the root is ephemeral, so the whole remainder collapses to `<*>` (a
  namespace of ephemeral *trees*). `Instance`: exactly the one component under the root masks and the tail
  resumes normal classification (a content-addressed *store* whose subtree is structurally stable).

| Root | anchor | scope |
|---|---|---|
| `/tmp` · `/var/tmp` · `/var/folders` | `TokenStart` | `Subtree` |
| `/private/var/folders` · `/private/tmp` (the macOS real paths of two roots above) | `TokenStart` | `Subtree` |
| `.conan2/p/b` | `Floating` | `Instance` |
| `/nix/store` | `TokenStart` | `Instance` |
| `AppData/Local/Temp` (the Windows per-user temp folder; the drive and user before it vary by machine) | `Floating` | `Subtree` |

**Separators.** For the root, `/` and a run of `\` both separate path components (JSON escaping doubles the
backslash, so `C:\\Users\\u\\AppData\\Local\\Temp\\run-a1` reads as one path), and every separator from the root's first
component onward must be one of them — a `:` never joins a root. The source-location walk still segments at `:` and
`/`; inside a segment, the root reads the `\`-separated components, and only the one component directly under a
matched root masks. Splitting that walk itself at `\` would also mask every digit-led component between backslashes
(a tool version in a tool-cache path) and split a JSON escape glued after a location, so it is not done.

The catalog is a **single source of truth** consulted from **two** call sites: the standalone ephemeral-root
normalizer (§4) **and**, as a per-segment predicate, from inside the source-location normalizer's segment walk
— so an instance directory inside a compiler diagnostic masks even though it is letter-leading, while the
`file:line` tail survives (§4). `bazel-out` is deliberately **excluded**: its component is the build
*configuration* (`k8-fastbuild`, `ppc-opt`), which is stable per config and carries drift signal we want
surfaced — it holds no hash, so masking it would destroy signal to fix nothing. Three more forms stay literal
beside it, each measured:

- **The runner's own temp folder** (`$RUNNER_TEMP`: `…/work/_temp`, `…/_work/_temp`, `D:\a\_temp`,
  `/github/runner_temp`) is not a per-run root: its children are tool-chosen, stable names (the runner's command
  files, a setup action's cache folder, a code-scanning database), so masking it would merge distinct stable
  folders. Its per-run children already mask, because their names carry a UUID that embedded-identity reads.
- **`host:/tmp/…`** (a remote copy target): a `:` lead also ends a label or a location, so it is not a path start
  by its bytes.
- **`\"/tmp/…`** (a JSON string inside a JSON string): reading an escaped opener would be a second escaping layer in
  the lead grammar.

Adding a root is a **core** masking change (it is syntactic, not ecosystem vocabulary) and owes an entry in the
generation ledger ([canonicalization_generations.md](canonicalization_generations.md)).

### 3.2 The wrapper-shell catalog — a shell is punctuation, never part of the value

A producer wraps a whole token in punctuation and means nothing by it: `(10.100.0.250)`, `[10.20.30.40]`
and `"10.0.0.1"` are one address in three dresses. The six pairs are the declared, frozen set, read from
**one table** (`kWrapperPairs`) by every surface that needs them — rule 4's grammar, rule 2's pre-gate, the
complete-shell reader and the ephemeral-root lead (§3.1) — because a second copy is how two maskers diverge.

**This catalog exists because its absence shipped.** Rule 4 had *already* decided a shell does not defeat
the class — it spelled `\[?…\]?` — and then implemented that decision over the single pair the first corpus
happened to show. Nothing chose `[` over `(`. So a parenthesised address failed at byte 0, was not
digit-leading, carried no byte in the pre-gate's separator set, and fell to rule 6 literal KEEP — and six
template rows of a **published** render carry a real third-party IPv4 for that reason, inside the section
where a reader has been told the addresses are gone.

Two facts about its shape, both measured rather than assumed:

- **Only the OPENING byte was ever the defect.** A trailing closer leaves byte 0 a digit, so `10.0.0.1)`
  was always masked by the digit-leading rule. An opener destroys digit-leading and left rule 4 the only
  rule that could see the token. The closers matter as the shell's trailing half, never as an entry point.
- **The hash class needed the *pre-gate*, not a second shell.** A hex run ≥ 16 inside `[…]` already
  normalized to `[<*>]` — not because rule 3 tolerates a shell (it requires the whole token) but because
  `[` sat in the pre-gate's separator set, so `embedded-identity` got a look. `(` did not, so the same hash
  in parentheses was kept whole. A wrapped *UUID* escaped only by accident (a UUID carries `-`, which was
  in the set), and that accident is what hid the hex case. Widening the pre-gate reproduces the normal form
  the bracketed and UUID forms already produced, instead of minting a second one for one class.

The shell widens **which punctuation** a rule tolerates, never **what it matches**: `(anonymous)`,
`(reserved)` and `(v1.2.3.4)` stay literal. **Rule 5 reads a COMPLETE shell too:** a token made of a catalog
opener at byte 0, a digit-leading core holding neither byte of that pair, the opener's own closer, and at most
two trailing bytes from `,;:.` takes rule 5's disposition, so `(1.7s)`, `[02:16:00]`, `(96.4%),`, `"2220"` and
`(1.2.3)` mask as their bare forms do. Its acceptance set is, by construction, the set rule 5 masks bare, so it
merges nothing canon does not already merge when the value is printed without the punctuation. The shell must be
complete: `(25 warnings)` splits into `(25` and `warnings)`, neither a shell, so that count stays literal; a core
of at most 3 digits behind a status keyword stays literal (`exit code (1)`), and a composite rule still wins
(`[42]` is the bracket index's `[<*>]`). Measured on 32 000 lines of real third-party
logs across 16 producers, the repair moves **3 templates out of 6712**.

**The hash counter reads a complete shell too.** A token made of a catalog opener at byte 0, a core that the bare
hash counter claims (`#`, a digit run, then no letter or digit) holding neither byte of that pair, the opener's own
closer and at most two trailing bytes from `,;:.` takes the counter's normal form inside the kept shell:
`(#9767):` → `(#<*>):`, `[#42]` → `[#<*>]`, `"#7",` → `"#<*>",`. It is a normalization, as `#42` → `#<*>` is bare,
so no param is pushed. The acceptance set is the set the bare counter already masks, so it merges nothing the bare
form does not. Its boundary: `(#42a)` (a letter follows the run), `(#42` (incomplete), `fix(#123):` (byte 0 is not an
opener) and `(#)` stay literal, and `[42]` is still the bracket index's `[<*>]`, which runs first.

**The complete-shell grammar is one reader, exported.** `complete_shell_core` (canon's public API) returns the core
of a complete shell or nothing. Rule 5 and the hash counter call it, and a consumer that classifies a param's value
by its syntax (Sift's value-slot gates) calls the same function, so the shell is read one way on both sides of the
package boundary and the catalog is never copied out of canon.

---

## 4. The composite normalizers (keep-class, mask-instance)

A composite token carries a structural delimiter; each normalizer keeps the stable part and masks the varying
instance. Tried in order; first match wins.

| Normalizer | Matches | Keeps / masks | Example |
|---|---|---|---|
| **source-location** | `<path-like>:<digits>[:<digits>]` (prefix contains `.` or `/`) | keep the file/path, mask each `:<digit-run>` | `tokenizer.cpp:4500:30:` → `tokenizer.cpp:<*>:<*>:` |
| **ephemeral-root** | a path (components separated by `/` or a run of `\`) whose components match a declared ephemeral root (§3.1), a `TokenStart` root sitting at the path's start behind an optional declared lead; reached only when no earlier rule claimed the token | keep the root and the separator after it, mask the instance component; **Subtree** collapses the whole remainder, **Instance** keeps the tail | `/tmp/pw-electron-userdata-Kw9v4a` → `/tmp/<*>` (Subtree) · `~/.conan2/p/b/insig247…/lib/x.so` → `~/.conan2/p/b/<*>/lib/x.so` (Instance) · `file:///tmp/run-a1/x.ts` → `file:///tmp/<*>` · `TMPDIR=/tmp/run-a1` → `TMPDIR=/tmp/<*>` · `C:\Users\u\AppData\Local\Temp\run-a1\x.ts` → `C:\Users\u\AppData\Local\Temp\<*>` |
| **versioned-ref** | `<name>/<numeric-version>` (digit after the last `/`, only punctuation may trail) | keep the name, mask the version | `zlib/1.3` → `zlib/<*>` |
| **bracket-index** | `<word>[<short-alpha?><digits>]` | keep the word + class marker, mask the index | `make[2]:` → `make[<*>]:` · `[gw0]` → `[gw<*>]` |
| **hash-counter** | `#<digits>` (no alnum may trail), bare or inside a complete wrapper shell (§3.2) | keep `#` and the shell, mask the index; no param | `step #26` → `step #<*>` · `INFO (#9767): watcher` → `INFO (#<*>): watcher` · `[#42]` → `[#<*>]` |
| **marker-number** | `<currency-marker><digit-core>[.<digits>]` | keep the marker, mask the number | `$463.50` → `$<*>` |
| **embedded-identity** | a UUID (`8-4-4-4-12`), a hex run ≥ 16, or a **compact UTC instant** (`YYYY-MM-DDTHHMMSSZ` — exactly 18 bytes, colon-free time, mandatory `Z`, non-alphanumeric on both sides), *inside* a larger token not under a declared ephemeral root — including one whose only structure is a wrapper shell (§3.2) | mask the id in place, keep surrounding structure | `~/.cache/gradle/f7f6…2680/lib.jar` → `~/.cache/gradle/<*>/lib.jar` · `(d41d…427e)` → `(<*>)` · `/home/runner/work/_temp/2026-06-09T185733Z.json` → `/home/runner/work/_temp/<*>.json` |
| **sanitizer-pid** | `==<digits>==` opening the token, followed by nothing or by a letter (the process tag AddressSanitizer, libFuzzer and valgrind print) | keep both fences and a glued letter-leading tail, mask the pid; a digit after the closing fence, or any byte before the opening one, declines | `==4242==` → `==<*>==` · `==77==ABORTING` → `==<*>==ABORTING` |
| **kv-value** | `<key>=<digit-leading-value>` (strips a leading currency marker first) | keep the key (+ marker), mask the value | `order=100000` → `order=<*>` · `total=$18` → `total=$<*>` |
| **`;`-segment** | not a claiming rule: applied to the token's normal form after every rule above and before declared-run, when the form holds `;` and `=`. Each `;`-segment `<key>=<value>` whose key is a letter- or `_`-led run of `[A-Za-z0-9_.-]` opening the segment (the first segment may open with wrapper openers) and whose value is digit-leading after an optional currency marker | keep every key, every `;` and a word value; mask a digit-leading value over its **extent**: from its first digit over `[A-Za-z0-9._+%-]` and any `<*>` an earlier rule wrote, across one `,` `:` or `/` directly followed by a digit or `<*>`. The **remainder** after the extent is decided whole: swallowed when it is empty or only wrapper closers and `,;:.`, else kept byte for byte behind `<*>`; a carriage return is content (§1). A short status value behind a status key stays literal per segment, read on the extent; no param. `,` is not a delimiter | `##[end-action id=build;outcome=success;duration_ms=12]` → `##[end-action id=build;outcome=success;duration_ms=<*>` · `item=book;total=$18` → `item=book;total=$<*>` · `Import-Package=okio;version=1.15,javax.annotation;version=1.3,*` → `Import-Package=okio;version=<*>,javax.annotation;version=<*>,*` · `id=a;n=5&amp;m=6` → `id=a;n=<*>&amp;m=<*>` · `FREQ=WEEKLY;BYHOUR=8,11,14;BYMINUTE=0` → `FREQ=WEEKLY;BYHOUR=<*>;BYMINUTE=<*>` · `id=a;t=12:30:01` (normal form `id=a;t=12:<*>:<*>`) → `id=a;t=<*>` · `id=a;n=5]`+CR keeps `]`+CR and `id=a;n=5]`+CR+`,` keeps `]`+CR+`,` · `id=build;status=200`, `id=a;status=200]` and `id=build,duration_ms=12` stay literal |
| **declared-run** | not a claiming rule: applied once to the token's normal form after every rule above, on a stream that declares a value V under a key its dialect declares. A digit run masks when it is the maximal run directly behind a declared marker, the marker opens the form or follows a byte that is neither a letter nor a digit, the run equals V byte for byte, and the byte after it is the form's end or neither a letter nor a digit | keep everything else in the token, mask the run; no param | V = 6656: `/stirling/V2-PR-6656/docker-compose.yml` → `/stirling/V2-PR-<*>/docker-compose.yml` · `PR#6656` → `PR#<*>` · `PR-6657`, `XPR-6656`, `PR-66560` and `python3` (V = 3) stay literal. The predicate is one exported function (`claim_declared_runs`); Sift's job and step instance key reads the same claims (`declared_discriminant_of`) |

**The ephemeral-root catalog is also consulted from inside source-location.** A per-run instance directory in
a compiler diagnostic masks even though it is letter-leading (it would otherwise be *kept* as a class anchor),
while the `file:line` tail — what makes a diagnostic actionable — survives:
`/home/runner/.conan2/p/b/insig247e3d1dffc33/…/span_unpack.cpp:72:5:` →
`/home/runner/.conan2/p/b/<*>/…/span_unpack.cpp:<*>:<*>:`. Two runs whose only difference is that per-run
directory (`insig247…` vs `insigea56…`) now collapse to **one** template instead of manufacturing a phantom
new/vanished pair every run — the defect this catalog exists to kill. The scope is clamped to `Instance` here
regardless of what the entry declares, so the location tail is never masked.

The kv-value normalizer **declines** a status value rather than claiming it: on `status=200` / `code=0`
(a status keyword + short numeric value) `normalize_kv_value` returns *no match*, the token falls through to
**rule 6** and stays literal — so the green→red flip survives in `key=value` form, but the rule that keeps it is
rule 6, not this normalizer (the golden's `status=200` witness is a `literal_keep` row for exactly that reason). It masks
**numeric** values only — `user=alice` (letter-leading) stays literal, because masking *all* values would
collapse `status=ok` and `status=failed` (telling an instance key from a categorical key needs cardinality,
which a stateless per-line masker cannot see — see §6). The kv-value normalizer reads ONE `key=value` per token
and declines when the first value is a word, so `id=build;outcome=success;duration_ms=12]` is not its; the
`;`-segment step applies the same disposition to each segment of the token's normal form instead, the status
carve-out included — and it also reaches a token `embedded-identity` already claimed
(`id=__<*>.step;duration_ms=<*>`), because it reads the normal form after every claiming rule. The step masks a
value over its number's extent, where the kv-value normalizer masks to the TOKEN's end. So the same bytes mask
differently by which rule reaches them: `version=1.15,javax.x` is the kv-value normalizer's (`version=<*>`), while
`x;version=1.15,javax.x` declines it and reaches the step (`x;version=<*>,javax.x`). The asymmetry is deterministic,
a pure function of the token.

---

## 5. Template identity

`template_id` is the **first 16 bytes of SHA-256(`template_str`)** — a 128-bit content address of the masked
line. It renders on the wire as `h:` + 32 lowercase hex digits (the only place the id materializes as text).
Same masked sequence → same id, byte-for-byte, on every machine.

Two lines collapse to one template **iff** their masked token sequences are byte-identical. That is the entire
contract: masking decides identity, identity decides "same kind of line," and everything downstream
(frequency, novelty, drift) rides on it.

---

## 6. The boundary — what canon deliberately does NOT mask

Masking is **syntactic and decidable by construction**. It masks classes it can recognize from a single line's
bytes; it stops where recognition would require cross-line knowledge. This boundary is a design guarantee, not
a gap to be closed with more rules:

- **Arbitrary varying words.** `User alice` / `User bob` — letter-leading, not a number/hash. canon keeps
  them. Knowing `alice` and `bob` are "the same field varying" needs cardinality across lines.
- **Word-prefixed ids.** `ORD-123`, `pod-x7f`, `order=ORD-123` — syntactically indistinguishable from a
  versioned keyword (`arm64`, `gpt-4`, `utf-8`): same `<alpha><sep?><suffix>` shape. Only *cardinality*
  separates an id from a keyword, and a stateless masker cannot see cardinality. Masking these by a syntactic
  rule would either over-mask real keywords (`arm64 → arm<*>`) or require an ad-hoc prefix allow/deny list.
  The one exception is a value the run **declares**: a run's own pull-request number behind a marker its
  dialect declares is a supplied fact, not a cardinality guess, so the declared-run step (§4) masks it and
  nothing else of the same shape.
- **An instant printed across whitespace tokens.** An RFC 1123 or RFC 2822 date (`Thu, 02 Jul 2026 06:43:50 GMT`)
  or a ctime date in a line's content keeps its weekday and month names literal while its numbers mask, so one line
  kind spreads over one template per weekday and month. Masking decides each whitespace token from its own bytes,
  and a weekday or month name is a word by its bytes (`May`, `Sat`, `Sun`); only its neighbours make it part of an
  instant. Masking the names token by token would also spread one value over several params; treating the instant
  as one value is a question about the masking unit (`ADR-16.D5`), not a missing rule.
- **Categorical numbers that should split.** An HTTP `404` vs `500` is handled by **extending the status-KEEP
  context** (rule 1 / the kv carve-out), never by weakening the digit-leading mask.

The decidability test for adding a rule: *is there a low-cardinality keyword of this exact shape worth
protecting?* If no (e.g. `$<digits>` — there is no `$`-prefixed keyword), the class is decidably a number and
earns a rule. If yes (e.g. `<word>-<digits>`), it is undecidable per line and stays out — the home for that is
a future data-informed, cardinality-aware, **frozen** classifier (the deferred `SemanticClassRegistry`), not
more syntactic rules. A standing cardinality monitor watches for any format where over-splitting erodes
compression past the healthy band — that signals masking is too weak *for that format*, surfaced as a measured
warning rather than guessed at.

---

*See also: [classification.md](classification.md) (the failure/level signals computed on the same line) ·
[determinism.md](determinism.md) (why every rule here is byte-exact and what the `canonicalization_version`
gate protects) · [formats.md](formats.md) (ordinals are carried beside the template, never masked into it).*
