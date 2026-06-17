#include "args.hpp"
#include <cassert>
#include <cstring>

int ok = 0, fail = 0;
#define check(cond, msg) if(cond){ok++;}else{std::cerr<<"FAIL: "<<msg<<"\n";fail++;}

void make_argv(std::vector<const char*>& av, std::vector<std::string>& storage, std::initializer_list<const char*> args) {
    storage.clear(); av.clear();
    storage.reserve(args.size());
    av.reserve(args.size());
    for (auto s : args) { storage.push_back(s); av.push_back(storage.back().c_str()); }
}

void test_basic_flag() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "--verbose"});
    args::Parser p;
    p.flag("verbose", "enable verbose output");
    p.parse((int)av.size(), (char**)av.data());
    check(p.get_flag("verbose"), "verbose flag set");
    check(!p.get_flag("debug"), "debug flag not set");
}

void test_option_value() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "--output", "result.txt"});
    args::Parser p;
    p.option("output", "output file");
    p.parse((int)av.size(), (char**)av.data());
    check(p.get("output") == "result.txt", "option value");
}

void test_default_value() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog"});
    args::Parser p;
    p.option("port", "port number", "8080");
    p.parse((int)av.size(), (char**)av.data());
    check(p.get("port") == "8080", "default value");
}

void test_get_as_int() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "--count", "42"});
    args::Parser p;
    p.option("count", "count");
    p.parse((int)av.size(), (char**)av.data());
    check(p.get_as<int>("count") == 42, "get_as int");
}

void test_positional() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "input.txt"});
    args::Parser p;
    p.positional("file", "input file");
    p.parse((int)av.size(), (char**)av.data());
    check(p.get("file") == "input.txt", "positional arg");
}

void test_equals_syntax() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "--name=jasper"});
    args::Parser p;
    p.option("name", "name");
    p.parse((int)av.size(), (char**)av.data());
    check(p.get("name") == "jasper", "--key=val syntax");
}

void test_required_missing() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog"});
    args::Parser p;
    p.option("input", "input file", "", true);
    bool threw = false;
    try { p.parse((int)av.size(), (char**)av.data()); }
    catch (const args::ParseError&) { threw = true; }
    check(threw, "required option throws");
}

void test_unknown_flag() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "--unknown"});
    args::Parser p;
    bool threw = false;
    try { p.parse((int)av.size(), (char**)av.data()); }
    catch (const args::ParseError&) { threw = true; }
    check(threw, "unknown flag throws");
}

void test_has() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "--verbose"});
    args::Parser p;
    p.flag("verbose");
    p.option("output");
    p.parse((int)av.size(), (char**)av.data());
    check(p.has("verbose"), "has set flag");
    check(!p.has("output"), "has unset option");
}

void test_dashdash_makes_flag_positional() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "--", "-v"});
    args::Parser p;
    p.flag("verbose", "verbose", 'v');
    p.positional("input", "input file");
    p.parse((int)av.size(), (char**)av.data());
    check(!p.get_flag("verbose"), "flag after -- not parsed as flag");
    check(p.get("input") == "-v", "arg after -- becomes positional");
}

void test_dashdash_mixed() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "-v", "--", "--output"});
    args::Parser p;
    p.flag("verbose", "verbose", 'v');
    p.positional("input", "input file");
    p.parse((int)av.size(), (char**)av.data());
    check(p.get_flag("verbose"), "flag before -- still parsed");
    check(p.get("input") == "--output", "long-opt after -- is positional");
}

void test_dashdash_unknown_after_is_not_error() {
    std::vector<const char*> av;
    std::vector<std::string> st;
    make_argv(av, st, {"prog", "--", "--not-a-flag"});
    args::Parser p;
    p.positional("file", "some file");
    bool threw = false;
    try { p.parse((int)av.size(), (char**)av.data()); }
    catch (const args::ParseError&) { threw = true; }
    check(!threw, "-- prevents ParseError for unknown-looking arg");
    check(p.get("file") == "--not-a-flag", "positional has correct value after --");
}

int main() {
    test_basic_flag();
    test_option_value();
    test_default_value();
    test_get_as_int();
    test_positional();
    test_equals_syntax();
    test_required_missing();
    test_unknown_flag();
    test_has();
    test_dashdash_makes_flag_positional();
    test_dashdash_mixed();
    test_dashdash_unknown_after_is_not_error();

    std::cout << ok << "/" << (ok + fail) << " tests passed\n";
    return fail == 0 ? 0 : 1;
}
