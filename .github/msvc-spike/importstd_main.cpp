// `import std;` functional smoke for the MSVC 14.52 gather spike — confirms the gathered
// toolset's std module compiles and that import std + std::jthread/std::stop_token work
// (a capability our module-based code relies on). Compiled with /c (no link needed).
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
