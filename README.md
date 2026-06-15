<div align="center">

# args.hpp

single-header argument parser for C++17

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![header-only](https://img.shields.io/badge/header--only-yes-brightgreen?style=flat-square)
![license](https://img.shields.io/badge/license-MIT-blue?style=flat-square)

</div>

copy `args.hpp` into your project, done. no cmake, no install.

```cpp
#include "args.hpp"

int main(int argc, char** argv) {
    args::Parser p("mytool", "does a thing");
    p.flag("verbose", "print more stuff", 'v');
    p.option("output", "output file", "out.txt", false, 'o', "FILE");
    p.option("count", "how many", "1", false, 'n');
    p.positional("input", "input file");

    try {
        p.parse(argc, argv);
    } catch (const args::ParseError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    if (p.get_flag("verbose"))
        std::cout << "verbose mode\n";

    std::string outfile = p.get("output");
    int n = p.get_as<int>("count");
    std::string infile = p.get("input");
}
```

running `./mytool --help` gives you:

```
Usage: mytool <input> [options]
does a thing

Options:
  -h, --help           show this message
  -v, --verbose        print more stuff
  -o, --output <FILE>  output file (default: out.txt)
  -n, --count <val>    how many (default: 1)

Positional arguments:
  input               input file [required]
```

**API:**

`p.flag(name, help, short='\0')` — boolean, present or not  
`p.option(name, help, default, required, short='\0', metavar="")` — takes a value  
`p.positional(name, help, required)` — positional arg  
`p.parse(argc, argv)` — throws `ParseError` on bad input  
`p.get(name)` — returns string value  
`p.get_as<T>(name)` — returns T via `>>` conversion  
`p.get_flag(name)` — returns bool  
`p.has(name)` — was it actually provided  
`p.get_opt(name)` — returns `std::optional<std::string>`, nullopt if not provided  
`p.get_all(name)` — returns all values (for repeated options)  

`metavar` controls the placeholder shown in `--help`. `"FILE"` → `--output <FILE>`. defaults to `val`.

short flags must be registered explicitly — duplicate short chars throw `std::logic_error` at setup time, not silently collide at runtime.

supports `--key=value` syntax and `--help` is built in.

**License:** MIT
