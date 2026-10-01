# Corpus: `loghub` — INTERNAL ONLY

**Internal R&D only, never published** (the Founder, 2026-10-01): CodeRoast publishes only its own
logs, and no LogHub byte, excerpt or rendering stands on a public surface. The bytes live in the
private warehouse's byte root alone.

Real-world log datasets from the academic **LogHub** collection — the structural ground truth for
cube measurement, the cross-stdlib determinism measurement, and the rich-format AMI re-measure
(decision #1). Registered in [../REGISTRY.md](../REGISTRY.md); governed by ADR-7, corpus storage &
governance (an internal CodeRoast record, not shipped here).

## Intent / test-purpose

- **Cube measurement** (concluded): the `b_native` oracle (BGL/Thunderbird alert classes), the
  all-format structural sweep, the template-lattice / format-relative gate. *This arc is closed*
  (the disposable `insight_cube` pkg was deleted at the 1.6.0 verdict; `ADR-19` owns the cube today) — LogHub is
  retained for what follows.
- **Cross-stdlib determinism measurement**: a large, messy, real input for the gcc-16.2/libstdc++ ≡
  clang-21/libc++ diagonal.
- **Rich-format AMI re-measure**: base-vs-lattice AMI on a genuinely rich format. The cube itself
  is no longer conditioned on it — it ships always-on since 1.7.2 (no opt-in flag).

## Provenance · license

- **Source:** Zenodo record [`8196385`](https://zenodo.org/records/8196385) — *Loghub: A Large
  Collection of System Log Datasets for AI-driven Log Analytics*, curated by LOGPAI, 2023
  (J. Zhu, S. He, P. He, J. Liu, M. R. Lyu, ISSRE 2023). It holds 19 archives of full logs and
  no sample files.
- **License:** the record's licence field declares **CC-BY-4.0**; its description also carries
  logpai's notice — free for research or academic work, on condition that any use or distribution
  refers to https://github.com/logpai/loghub and cites the paper, the notice included in all
  copies. Nothing derived from it is published.
- **Class:** big · **re-acquirable** — zero bytes in git; the pins below make any (re)download
  verifiable. Every pin was verified 2026-10-01 by a run of `download_logs.sh` from these files.
- **Byte root:** the warehouse's gitignored `coderoast-corpora/zenodo_corpora/loghub/data/`,
  ceiling in `coderoast-corpora/_shared/STORAGE.json` (`loghub`).

### Pins — the record's 19 archives (`data/loghub-full/`, kept compressed)

Bytes and md5 as Zenodo's API publishes them (read 2026-10-01); the sha256 is ours, taken over
bytes that matched both. `BGL.zip`'s sha256 was first pinned 2026-06-17 from an earlier download,
and the 2026-10-01 download reproduced it.

- `Android_v1.zip` — 24,858,007 B · md5 `3b35b3cc00d577a510cf9f416d59b3b3` · sha256 `0d04afa769e9eb59f9551151698127127e08c0c831b423c974024c78ce8c4d71`
- `Android_v2.zip` — 444,986,139 B · md5 `3195bb72252e98b031c5b9b62320e993` · sha256 `173aab2acf6331bb0423ead825a277dc666439e0ab928bb21eff88ca5122ebe0`
- `Apache.tar.gz` — 262,905 B · md5 `de9a42d12f9b60612631c67a5a9f8628` · sha256 `f2cb5014453c8a0f71596838e1639a2131ff6d360839ad280409f621a6cac452`
- `BGL.zip` — 57,489,019 B · md5 `4452953c470f2d95fcb32d5f6e733f7a` · sha256 `d67fd82a711aea0157a9b83175892c6ee60e384a2ddf5bc51f39118453816da8`
- `HDFS_v1.zip` — 186,645,559 B · md5 `76a24b4d9a6164d543fb275f89773260` · sha256 `04f919f2185821f23f045dca611a7586429bdabc601bd7b43f30005f8e289b01`
- `HDFS_v2.zip` — 823,700,713 B · md5 `fee2e2a1353abc5f9e30a4ab810d5b31` · sha256 `673fdb774d9b33f9cf3683eb3782d7c4e293f886d3f797a2e92e90bf652e4d91`
- `HDFS_v3_TraceBench.zip` — 567,419,398 B · md5 `c69999ff28933d2b163170d26c2faf64` · sha256 `38416acd0080c52d028eea74e9a4534b93902d75b6b07b0e11785387296fbc73`
- `HPC.zip` — 2,975,898 B · md5 `15b3bcf44cf5c98e35451efa22a0de77` · sha256 `3df2d31235c441083d8bee41f15bebd9ca5997f1a2ba9e6a967118865dfb1811`
- `Hadoop.zip` — 3,416,419 B · md5 `34e28a9943704fd54933e2b455829fcc` · sha256 `79e63c6521e90ae164754a29d13e04e0c6c5e782490a7fe1e2b124f4f5737ced`
- `HealthApp.tar.gz` — 2,296,618 B · md5 `cec2cca71da9c4f8e33eaa9b215f1b90` · sha256 `157f5ae299b2cc3ece65d61541ade6f5f4282d428bb0cddb50f6b466d3ee6ffd`
- `Linux.tar.gz` — 232,039 B · md5 `6d1802d7778126f21c001c6aa7b6b106` · sha256 `7e1f820d8d45ae086032e515d9ce8d079a102a9b37dcff5b41a8e60b1f857820`
- `Mac.tar.gz` — 1,489,372 B · md5 `e5d0558b6ec661c739e77c7ff1b71498` · sha256 `a3ebaae341af38f397924cc4f00f2e0e95ff9db4e97abd0dd23986eda239a5a3`
- `OpenStack.tar.gz` — 5,394,691 B · md5 `66bd42c07837a094d9b0ea2d036b5713` · sha256 `87c98c5ed03262e05cdb7a6f3717033df76d88fda0f7d2db23bd9fa4200f1879`
- `Proxifier.tar.gz` — 172,346 B · md5 `2612fce12cc3d16599ddb3db8e9c477a` · sha256 `b56d93eb87188e1284b5b35e92cb6f4151aec22975d82c5387d8e81a52d2a96d`
- `SSH.tar.gz` — 4,596,666 B · md5 `a44e40b4348697dabf2bba56885e0a38` · sha256 `296610a35773f58b7c295e5407fc5ccdf48beb3c544fc14a6309addf904a2712`
- `Spark.tar.gz` — 183,474,743 B · md5 `31ddaff179f6c1ae5203770138156b17` · sha256 `f11e5df5a98ce25d0f80adb7631b022ae6f1193f510079387447ef4e0fdccef7`
- `Thunderbird.tar.gz` — 2,016,100,298 B · md5 `0891b048df2919dc78c99c4428686b44` · sha256 `228f8589b7cd569b727c5da654c647aa538dd3dd95541e675a40523c5fff37cf`
- `Windows.tar.gz` — 1,670,098,945 B · md5 `8b994d947d30617d51e2a565152ae90f` · sha256 `1d6aac3cd52aeeb1f5b46534c166326586090a1aa5a20f1608ad64a80b64eca4`
- `Zookeeper.tar.gz` — 459,911 B · md5 `11458b4f9fa0911c238f5c1189d4f2d4` · sha256 `ea350b0d3ff22bca4764ae4a1521bd1b79c448db4ee99aac02fd82b9a0995f19`

### Pins — the extracted populations (`data/loghub-full/`)

- `BGL.log` — 743,185,031 B · 4,747,963 lines · sha256 `666130b15ef44eb32fd02bd053e6c6e007c37696b5e7e8b9d8e45b729876a5d2` — the member of the BGL archive, extracted whole (4.40 M normal + ~348 k alerts / ~30 classes, labels intact).
- `Thunderbird_5M.log` — 868,147,617 B · 5,000,000 lines · sha256 `6e0f52d45d639c76fc2f430e6ef609a915072c2328b862cb097dc59ac5694580` — the first 5 000 000 lines of the Thunderbird archive's 31.8 GB log; first pinned 2026-06-17 from a network stream, reproduced 2026-10-01 from the local archive.

### Pins — the 16 sample slices (`data/loghub/`, and the published `samples/` copies)

Every slice is 2 000 lines and ends with a line end. The CR column counts CRLF line ends: the
rest are LF, exactly as upstream. `Windows_2k.log` opens with the UTF-8 byte-order mark its log
opens with; `Hadoop_2k.log`'s line 828 is LF-terminated upstream inside a CRLF log.

- `Android_2k.log` — 230,456 B · 2,000 lines · 2,000 CR · sha256 `88b7d95c250a96cf627a73468db2dc575fa67c6a9a17c64f0018b21f2389903d`
- `Apache_2k.log` — 200,328 B · 2,000 lines · 0 CR · sha256 `22c51ca1d49d0354cfbb7aa70408a7a436e4c220763b44b1bb45c8fc4f5d09e1`
- `BGL_2k.log` — 297,323 B · 2,000 lines · 0 CR · sha256 `ad40824fd15a3e8f4b3e705eef0f5c80ef19190eff2ceee57e8cd6d83279f89a`
- `HDFS_2k.log` — 290,677 B · 2,000 lines · 2,000 CR · sha256 `2d8f5a07d6b4a8f94162ab488cf1ee649c02a1507776ffc6eebcc6ec67c476db`
- `HPC_2k.log` — 315,094 B · 2,000 lines · 2,000 CR · sha256 `6dbbe741c6f91cff04a0e4c50132de7a2a9881ea48862049c912a3291f83773b`
- `Hadoop_2k.log` — 259,015 B · 2,000 lines · 1,999 CR · sha256 `929fa2121c8d2ef1d6203254cdd893f501bffdf9cf0fa800b097e05a92058cd3`
- `HealthApp_2k.log` — 187,458 B · 2,000 lines · 2,000 CR · sha256 `d6fe07b1c5a0269576343fcbf3dd6fc839b7ffe1d153b0abf02c0f5791b37862`
- `Linux_2k.log` — 198,652 B · 2,000 lines · 0 CR · sha256 `ed0c6ca4535199dbfc1f11eadbd0103caf24a8862e6bf94fead05c4c3b471460`
- `Mac_2k.log` — 294,381 B · 2,000 lines · 2,000 CR · sha256 `1748ffb42b765f9c58f2be2beff1f215ef7d19e644d13dd9067e37419ef575bc`
- `OpenSSH_2k.log` — 225,218 B · 2,000 lines · 2,000 CR · sha256 `0a00ba2aa573839894022593339b5c4072e174e298316dbc1b06012ced81c5d7`
- `OpenStack_2k.log` — 593,121 B · 2,000 lines · 0 CR · sha256 `b1c0fae2669519691988bfe7e466dbc37e841f76ef30d72b46968d0d12d4c988`
- `Proxifier_2k.log` — 236,913 B · 2,000 lines · 2,000 CR · sha256 `8b9a5609e783c99dbe7b73b22f0247e1206ff1ca57e13761a79a7bddbc9edb8d`
- `Spark_2k.log` — 196,268 B · 2,000 lines · 2,000 CR · sha256 `2e8b9a37fc5c238253e0b8e18a8bd5e489671def91767ae1192d28c8e1f95901`
- `Thunderbird_2k.log` — 274,650 B · 2,000 lines · 0 CR · sha256 `9631c6877ee2e9ba176cbb57d14c75edd328ec6f28090149e9e26eb7b8753487`
- `Windows_2k.log` — 284,688 B · 2,000 lines · 2,000 CR · sha256 `89c6595eeaf60b51715d1a9fa7948bce998f0e553c0942a804aa623746bdc750`
- `Zookeeper_2k.log` — 266,118 B · 2,000 lines · 2,000 CR · sha256 `5d6ee62826132ed92e1714f35826fdc352751c9efd428a8aa30e06488b497ee0`

### The slice rule (declared 2026-10-01)

**A slice is 2 000 consecutive lines of one log inside one of the record's archives, every byte
and line end as upstream: the first 2 000 lines, `head -n 2000` of the member streamed out of its
archive** — so any reader can re-derive a slice from the record alone. **A first window is
degenerate when one message template covers 95 % or more of its lines** (a log opening on one
repeated message); then the slice is the first 2 000-line block, counted from line 1, in which no
template covers half the lines. A template is a line with every `0x` hex literal, every run of 8
or more hex characters and every digit run masked, in that order. Measured over the 16 first
windows on 2026-10-01, exactly one is degenerate: `BGL.log`'s first 2 000 lines are 1 999
repetitions of *instruction cache parity error corrected* (2 templates), so `BGL_2k.log` is lines
**4 001–6 000** (33 templates, the commonest at 44.1 %; lines 2 001–4 000 still hold one template at
99.2 %). The next most concentrated first windows keep the default and are named so the choice is
visible: `HPC.log`'s, one template at 80.3 % but 77 templates in all, the most of its first ten
blocks; and `Proxifier.log`'s, one template at 49.4 % among 20, with 1 979 of its 2 000 lines one
browser through one proxy.

Where an archive holds more than one log, the slice names the log and why:

| Slice | Archive | Log | Lines | Why this log |
|---|---|---|---|---|
| `Android_2k.log` | `Android_v1.zip` | `Android.log` | 1–2 000 | v1 is the record's single Android log; v2's `duplicate_type1/issue_1/applogcat.log` is byte-identical to it, and v2's other 24 logs are per-issue captures |
| `Apache_2k.log` | `Apache.tar.gz` | `Apache.log` | 1–2 000 | |
| `BGL_2k.log` | `BGL.zip` | `BGL.log` | 4 001–6 000 | the declared exception above |
| `HDFS_2k.log` | `HDFS_v1.zip` | `HDFS.log` | 1–2 000 | v1 is the one flat HDFS log (the format the showcase has carried); v2 is 31 per-node logs and v3 (TraceBench) is CSV traces, not log lines |
| `HPC_2k.log` | `HPC.zip` | `HPC.log` | 1–2 000 | |
| `Hadoop_2k.log` | `Hadoop.zip` | `application_1445144423722_0020/container_1445144423722_0020_01_000001.log` | 1–2 000 | the archive is 978 container logs; this is the one the previous sample's first line came from, an application master's log (58 831 lines) |
| `HealthApp_2k.log` | `HealthApp.tar.gz` | `HealthApp.log` | 1–2 000 | |
| `Linux_2k.log` | `Linux.tar.gz` | `Linux.log` | 1–2 000 | |
| `Mac_2k.log` | `Mac.tar.gz` | `Mac.log` | 1–2 000 | |
| `OpenSSH_2k.log` | `SSH.tar.gz` | `SSH.log` | 1–2 000 | |
| `OpenStack_2k.log` | `OpenStack.tar.gz` | `openstack_normal1.log` | 1–2 000 | the first of the archive's two normal logs, and the one the previous sample began at; the third log is the anomaly-injected run |
| `Proxifier_2k.log` | `Proxifier.tar.gz` | `Proxifier.log` | 1–2 000 | |
| `Spark_2k.log` | `Spark.tar.gz` | `application_1485248649253_0147/container_1485248649253_0147_02_000006.log` | 1–2 000 | the archive is 3 852 container logs; this one's first 2 000 lines are byte-identical to the previous sample, so the showcase keeps its Spark input |
| `Thunderbird_2k.log` | `Thunderbird.tar.gz` | `Thunderbird.log` | 1–2 000 | also the first 2 000 lines of `Thunderbird_5M.log` |
| `Windows_2k.log` | `Windows.tar.gz` | `Windows.log` | 1–2 000 | |
| `Zookeeper_2k.log` | `Zookeeper.tar.gz` | `Zookeeper.log` | 1–2 000 | |

## Acquisition

All tooling lives in the private warehouse **`coderoast-corpora`** (`ADR-7.D4`):
`zenodo_corpora/loghub/scripts/download_logs.sh` performs every step and checks every pin above,
skipping a file that already verifies.

1. The 19 archives, from `https://zenodo.org/records/8196385/files/<name>`, each checked against
   Zenodo's size and md5 and our sha256 before it is kept.
2. `BGL.log` (`unzip -p` of the BGL archive) and `Thunderbird_5M.log` (the Thunderbird archive's
   log streamed through `head -n 5000000`).
3. The 16 slices, by the rule above.
4. logpai/loghub's 16 `_2k` files at its commit `dd61d0952749ee7963bde24220d1be5ede023033`, into
   `data/logpai-2k/`, with the CR-stripped forms our public repositories published, into
   `data/logpai-2k/published/`; their pins are in the script. They are not the record's, their
   licence is logpai's research-or-academic notice, and they are never published again; they are
   kept private because coderoast-hub's history and insight-canon's tags `v1.5.4`–`v1.7.5` still
   carry the published forms, and the public-history sweep finds only a byte the byte root holds.

**Extraction policy:** archives stay compressed; nothing is extracted but the two populations and
the 16 slices. The whole record decompresses to 92,048,231,840 B (92.0 GB) by its archives' own
headers (measured 2026-10-01), against the root's 10 GiB ceiling: extracting more is a reviewed
raise first.

> Lesson baked into the pin: a "full" academic corpus can be silently reprocessed (labels dropped)
> or a partial download — **verify col-1 labels + byte-count + checksum** before trusting it
> (the Zenodo-18522101 "LogTrie" dead-end: truncated + label-stripped).

## Ground truth / labelling

BGL / Thunderbird carry an **alert-label column 1** (`-` = normal; `KERNDTLB`/`APPSEV`/… = alert
class). The loader strips col-1 **only** where the sentinel-rate detector confirms it exists (the 14
message-leading formats keep col-1 as real message).

## Withdrawn from publication (2026-10-01)

The 16 slices above are for internal measurement only, in `data/loghub/`; the corpus has no
published sample slice. A test that reads a slice mounts `data/loghub/` by the `loghub` registry id.

What was published before the withdrawal: logpai/loghub's own `_2k` files — not members of the
record, under logpai's notice, labelled CC-BY-4.0 in error, with the CR of every CRLF line end
stripped in 15 of the 16 — in this repository's `main` from 2026-06-17 to 2026-07-09 and in its tags
`v1.5.4`–`v1.7.5`, then on the public hub from 2026-07-09 with canon's rendering over them. The
ruling of 2026-10-01 withdrew them from every public surface without rewriting history; each
public disclosure carries a dated correction note.
