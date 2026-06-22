#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <optional>
#include <algorithm>

namespace args {

struct ParseError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Arg {
    std::string name;
    std::string help;
    std::string metavar;
    std::string default_val;
    char short_name = '\0';
    bool required = false;
    bool is_flag  = false;
    bool found    = false;
    std::vector<std::string> values;
    std::vector<std::string> choices;
};

class Parser {
public:
    explicit Parser(std::string prog = "", std::string desc = "")
        : prog_(std::move(prog)), desc_(std::move(desc)) {}

    // --flag / -X  (boolean, no value)
    Parser& flag(const std::string& name, const std::string& help = "", char short_name = '\0') {
        Arg a;
        a.name = name;
        a.help = help;
        a.short_name = short_name;
        a.is_flag = true;
        add(std::move(a));
        return *this;
    }

    // --opt value / -X value
    // metavar controls the placeholder shown in --help (e.g. "FILE" → --output <FILE>)
    Parser& option(const std::string& name, const std::string& help = "",
                   const std::string& def = "", bool required = false, char short_name = '\0',
                   const std::string& metavar = "") {
        Arg a;
        a.name = name;
        a.help = help;
        a.default_val = def;
        a.required = required;
        a.is_flag = false;
        a.short_name = short_name;
        a.metavar = metavar.empty() ? "val" : metavar;
        if (!def.empty()) a.values.push_back(def);
        add(std::move(a));
        return *this;
    }

    // positional arg
    Parser& positional(const std::string& name, const std::string& help = "",
                       bool required = true) {
        Arg a;
        a.name = name;
        a.help = help;
        a.required = required;
        a.is_flag = false;
        positionals_.push_back(std::move(a));
        return *this;
    }

    // restrict an option to a fixed set of valid values
    Parser& choices(const std::string& name, std::vector<std::string> vals) {
        auto it = opts_.find(name);
        if (it == opts_.end())
            throw std::logic_error("unknown option for choices(): " + name);
        if (it->second.is_flag)
            throw std::logic_error("choices() not valid for flags: " + name);
        it->second.choices = std::move(vals);
        return *this;
    }

    void parse(int argc, char** argv) {
        if (prog_.empty() && argc > 0) prog_ = argv[0];
        std::vector<std::string> remaining;
        bool end_of_opts = false;

        for (int i = 1; i < argc; i++) {
            std::string s = argv[i];

            if (end_of_opts) {
                remaining.push_back(s);
                continue;
            }

            if (s == "--") {
                end_of_opts = true;
                continue;
            }

            if (s == "--help" || s == "-h") {
                print_help();
                std::exit(0);
            }

            if (s.size() > 2 && s[0] == '-' && s[1] == '-') {
                std::string key = s.substr(2);
                auto eq = key.find('=');
                if (eq != std::string::npos) {
                    std::string val = key.substr(eq + 1);
                    key = key.substr(0, eq);
                    auto* a = find(key);
                    if (!a) throw ParseError("unknown option: --" + key);
                    set_value(a, val);
                } else {
                    auto* a = find(key);
                    if (!a) throw ParseError("unknown option: --" + key);
                    if (a->is_flag) {
                        a->found = true;
                    } else {
                        if (i + 1 >= argc) throw ParseError("--" + key + " requires a value");
                        set_value(a, argv[++i]);
                    }
                }
            } else if (s.size() >= 2 && s[0] == '-' && s[1] != '-') {
                // POSIX combined short flags: -abc, -vn 5, -n5, -Ifoo
                for (size_t ci = 1; ci < s.size(); ci++) {
                    auto* a = find_short(s[ci]);
                    if (!a) throw ParseError(std::string("unknown flag: -") + s[ci]);
                    if (a->is_flag) {
                        a->found = true;
                    } else {
                        // option takes a value: rest of token (-n5) or next arg (-n 5)
                        if (ci + 1 < s.size()) {
                            set_value(a, s.substr(ci + 1));
                        } else {
                            if (i + 1 >= argc) throw ParseError(std::string("-") + s[ci] + " requires a value");
                            set_value(a, argv[++i]);
                        }
                        break; // option with value consumes the rest of the token
                    }
                }
            } else {
                remaining.push_back(s);
            }
        }

        size_t pi = 0;
        for (auto& sv : remaining) {
            if (pi < positionals_.size()) {
                positionals_[pi].values.push_back(sv);
                positionals_[pi].found = true;
                pi++;
            } else {
                remaining_.push_back(sv);
            }
        }

        for (auto& [k, a] : opts_) {
            if (a.required && !a.found)
                throw ParseError("required option missing: --" + a.name);
        }
        for (auto& a : positionals_) {
            if (a.required && !a.found)
                throw ParseError("required argument missing: " + a.name);
        }
    }

    bool get_flag(const std::string& name) const {
        auto it = opts_.find(name);
        if (it == opts_.end()) return false;
        return it->second.found;
    }

    std::string get(const std::string& name) const {
        auto it = opts_.find(name);
        if (it != opts_.end() && !it->second.values.empty())
            return it->second.values[0];
        for (auto& a : positionals_)
            if (a.name == name && !a.values.empty())
                return a.values[0];
        return "";
    }

    std::optional<std::string> get_opt(const std::string& name) const {
        if (!has(name)) return std::nullopt;
        return get(name);
    }

    template<typename T>
    T get_as(const std::string& name) const {
        std::string v = get(name);
        if (v.empty()) return T{};
        T result;
        std::istringstream iss(v);
        if (!(iss >> result) || !iss.eof())
            throw ParseError("bad value for --" + name + ": " + v);
        return result;
    }

    std::vector<std::string> get_all(const std::string& name) const {
        auto it = opts_.find(name);
        if (it != opts_.end()) return it->second.values;
        return {};
    }

    const std::vector<std::string>& get_remaining() const { return remaining_; }

    bool has(const std::string& name) const {
        auto it = opts_.find(name);
        if (it != opts_.end()) return it->second.found;
        for (auto& a : positionals_)
            if (a.name == name) return a.found;
        return false;
    }

    void print_help() const {
        std::cout << "Usage: " << prog_;
        for (auto& a : positionals_)
            std::cout << (a.required ? " <" : " [") << a.name << (a.required ? ">" : "]");
        std::cout << " [options]\n\n";
        if (!desc_.empty()) std::cout << desc_ << "\n\n";
        std::cout << "Options:\n";
        std::cout << "  -h, --help           show this message\n";
        for (auto& k : order_) {
            auto& a = opts_.at(k);
            std::string line = "  ";
            if (a.short_name != '\0') line += std::string("-") + a.short_name + ", --" + a.name;
            else                      line += "    --" + a.name;
            if (!a.is_flag) {
                if (!a.choices.empty()) {
                    std::string cs;
                    for (size_t i = 0; i < a.choices.size(); i++) {
                        if (i) cs += "|";
                        cs += a.choices[i];
                    }
                    line += " <" + cs + ">";
                } else {
                    line += " <" + a.metavar + ">";
                }
            }
            line.resize(std::max(line.size() + 1, (size_t)26), ' ');
            line += a.help;
            if (!a.default_val.empty()) line += " (default: " + a.default_val + ")";
            if (a.required) line += " [required]";
            std::cout << line << "\n";
        }
        if (!positionals_.empty()) {
            std::cout << "\nPositional arguments:\n";
            for (auto& a : positionals_) {
                std::string line = "  " + a.name;
                line.resize(std::max(line.size() + 1, (size_t)22), ' ');
                line += a.help;
                if (a.required) line += " [required]";
                std::cout << line << "\n";
            }
        }
    }

private:
    std::string prog_, desc_;
    std::unordered_map<std::string, Arg> opts_;
    std::vector<std::string> order_;
    std::vector<Arg> positionals_;
    std::vector<std::string> remaining_;

    void set_value(Arg* a, const std::string& val) {
        if (!a->choices.empty()) {
            auto it = std::find(a->choices.begin(), a->choices.end(), val);
            if (it == a->choices.end()) {
                std::string list;
                for (size_t i = 0; i < a->choices.size(); i++) {
                    if (i) list += ", ";
                    list += a->choices[i];
                }
                throw ParseError("--" + a->name + ": invalid value '" + val +
                                 "', expected one of: " + list);
            }
        }
        if (!a->found) a->values.clear();
        a->values.push_back(val);
        a->found = true;
    }

    void add(Arg a) {
        if (a.short_name != '\0') {
            for (auto& [k, existing] : opts_)
                if (existing.short_name == a.short_name)
                    throw std::logic_error(std::string("duplicate short flag '-") + a.short_name + "'");
        }
        order_.push_back(a.name);
        opts_[a.name] = std::move(a);
    }

    Arg* find(const std::string& name) {
        auto it = opts_.find(name);
        return it != opts_.end() ? &it->second : nullptr;
    }

    Arg* find_short(char c) {
        for (auto& [k, a] : opts_)
            if (a.short_name == c) return &a;
        return nullptr;
    }
};

} // namespace args
