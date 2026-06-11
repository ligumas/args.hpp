#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <optional>

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
    Parser& option(const std::string& name, const std::string& help = "",
                   const std::string& def = "", bool required = false, char short_name = '\0') {
        Arg a;
        a.name = name;
        a.help = help;
        a.default_val = def;
        a.required = required;
        a.is_flag = false;
        a.short_name = short_name;
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

    void parse(int argc, char** argv) {
        if (prog_.empty() && argc > 0) prog_ = argv[0];
        std::vector<std::string> remaining;

        for (int i = 1; i < argc; i++) {
            std::string s = argv[i];

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
                    if (!a->found) a->values.clear();
                    a->values.push_back(val);
                    a->found = true;
                } else {
                    auto* a = find(key);
                    if (!a) throw ParseError("unknown option: --" + key);
                    if (a->is_flag) {
                        a->found = true;
                    } else {
                        if (i + 1 >= argc) throw ParseError("--" + key + " requires a value");
                        if (!a->found) a->values.clear();
                        a->values.push_back(argv[++i]);
                        a->found = true;
                    }
                }
            } else if (s.size() == 2 && s[0] == '-') {
                auto* a = find_short(s[1]);
                if (!a) throw ParseError(std::string("unknown flag: -") + s[1]);
                if (a->is_flag) {
                    a->found = true;
                } else {
                    if (i + 1 >= argc) throw ParseError(std::string("-") + s[1] + " requires a value");
                    if (!a->found) a->values.clear();
                    a->values.push_back(argv[++i]);
                    a->found = true;
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
        std::istringstream(v) >> result;
        return result;
    }

    std::vector<std::string> get_all(const std::string& name) const {
        auto it = opts_.find(name);
        if (it != opts_.end()) return it->second.values;
        return {};
    }

    bool has(const std::string& name) const {
        auto it = opts_.find(name);
        if (it != opts_.end()) return it->second.found;
        for (auto& a : positionals_)
            if (a.name == name) return a.found;
        return false;
    }

    void print_help() const {
        std::cout << "Usage: " << prog_;
        for (auto& a : positionals_) std::cout << " <" << a.name << ">";
        std::cout << " [options]\n\n";
        if (!desc_.empty()) std::cout << desc_ << "\n\n";
        std::cout << "Options:\n";
        std::cout << "  -h, --help           show this message\n";
        for (auto& k : order_) {
            auto& a = opts_.at(k);
            std::string line = "  ";
            if (a.short_name != '\0') line += std::string("-") + a.short_name + ", --" + a.name;
            else                      line += "    --" + a.name;
            if (!a.is_flag) line += " <val>";
            while (line.size() < 26) line += ' ';
            line += a.help;
            if (!a.default_val.empty()) line += " (default: " + a.default_val + ")";
            if (a.required) line += " [required]";
            std::cout << line << "\n";
        }
        if (!positionals_.empty()) {
            std::cout << "\nPositional arguments:\n";
            for (auto& a : positionals_) {
                std::string line = "  " + a.name;
                while (line.size() < 22) line += ' ';
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
