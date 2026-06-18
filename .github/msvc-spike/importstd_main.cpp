// `import std;` + std::stop_token / std::jthread — the 14.51-specific C1116 fatal
// (dev-community 11075026 / 11090254), per bugs.md the SECOND blocker on top of the
// operator== miscompile. This is the clean discriminator: 14.51 GA fails to compile this,
// 14.52 fixes it. Compiled with /c (the failure is a compile-time fatal, no link needed).
import std;

int main() {
    std::stop_source source;
    std::stop_token token = source.get_token();
    std::jthread worker([](std::stop_token st) {
        while (!st.stop_requested()) { break; }
    });
    worker.request_stop();
    return token.stop_requested() ? 1 : 0;
}
