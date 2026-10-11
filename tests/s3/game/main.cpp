// Runner for the skate-core port tests. Usage: s3_game_tests [substring-filter]
#include <cstring>
#include <exception>
#include <iostream>

#include "harness.hpp"

namespace s3test {
std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}
}  // namespace s3test

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int passed = 0, failed = 0;
    for (const auto& t : s3test::registry()) {
        if (filter && !std::strstr(t.name, filter) && !std::strstr(t.file, filter)) continue;
        try {
            t.fn();
            ++passed;
        } catch (const s3test::Failure& f) {
            ++failed;
            std::cout << "FAIL " << t.name << "\n  " << f.message << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "FAIL " << t.name << " (panic)\n  " << e.what() << "\n";
        }
    }
    std::cout << "s3-game: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
