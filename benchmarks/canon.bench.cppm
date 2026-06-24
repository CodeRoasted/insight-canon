// insight.canon.bench — shared benchmark infrastructure (§11.9.11, the logcraft.bench pattern).
// All benchmark TUs import this instead of spelling out the full import block.
// Re-exports the complete canon module surface (public facade + the sealed detail shards), so a
// benchmark TU needs no further imports beyond google-benchmark (textual, third-party).
export module insight.canon.bench;
export import std;
export import insight.canon;
export import insight.canon.detail.scan;
export import insight.canon.detail.strategy;
export import insight.canon.detail.mask;
export import insight.canon.detail.parse;
