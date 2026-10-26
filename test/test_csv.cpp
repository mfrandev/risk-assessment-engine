#include <catch2/catch_test_macros.hpp>

#include <risk/csv.hpp>

TEST_CASE("trim strips surrounding whitespace and line endings") {
    REQUIRE(risk::trim("  abc \t\r\n") == "abc");
    REQUIRE(risk::trim("a b") == "a b");
    REQUIRE(risk::trim(" \t\r\n").empty());
}

TEST_CASE("split_csv_line trims fields and keeps a trailing empty field") {
    const auto fields = risk::split_csv_line(" a, b ,c\r");
    REQUIRE(fields == std::vector<std::string>{"a", "b", "c"});

    const auto trailing = risk::split_csv_line("1,2,");
    REQUIRE(trailing == std::vector<std::string>{"1", "2", ""});
}
