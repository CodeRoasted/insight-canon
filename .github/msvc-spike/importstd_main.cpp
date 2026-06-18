// `import std;` smoke for the MSVC 14.52 gather spike — the second of the three bugs
// the 14.52 toolset fixes (the `import std` build failure). Informational only.
import std;

int main() {
    std::vector<int> v{1, 2, 3};
    return static_cast<int>(v.size()) - 3;
}
