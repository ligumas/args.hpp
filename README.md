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
    p.flag("verbose", "print more stuff");
    p.option("output", "output file", "out.txt");
    p.option("count", "how many", "1");
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
  -h, --help       show this message
  --verbose        print more stuff
  --output <val>   output file (default: out.txt)
  --count <val>    how many (default: 1)

Positional arguments:
  input            input file [required]
```

**API:**

`p.flag(name, help)` — boolean, present or not  
`p.option(name, help, default, required)` — takes a value  
`p.positional(name, help, required)` — positional arg  
`p.parse(argc, argv)` — throws `ParseError` on bad input  
`p.get(name)` — returns string value  
`p.get_as<T>(name)` — returns T via `>>` conversion  
`p.get_flag(name)` — returns bool  
`p.has(name)` — was it actually provided  

supports `--key=value` syntax, short flags (-v), and `--help` is built in.

**License:** MIT
