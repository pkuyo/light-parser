# LightParser - A Lightweight C++ Parser-Combinator Library

![C++](https://img.shields.io/badge/C++-20-blue.svg)  ![License](https://img.shields.io/badge/License-MIT-green.svg)

LightParser is a lightweight parser combinator library based on C++20, designed using the parser combinator pattern to help developers quickly build flexible recursive descent parsers.

**⚠️ Note:** The library is currently in the testing phase and may be unstable; please exercise caution when using it in production environments.

## Features

- **Parser Composition**: Easily combine parsers using operators like `>>` (then), `|` (or), and `>>=` (map).
- **Lazy Parsing**: Supports lazy initialization for recursive parser definitions.
- **Compile-Time Construction**: Syntax parsers can be constructed at compile-time, enabling efficient and optimized parsing logic without runtime overhead.
- **Customizable Parsers**: Create parsers that return custom types, including smart pointers and user-defined types.
- **Panic Mode Recovery**: Provides a mechanism for recovering from parsing errors by synchronizing to a known point in the input.
- **Header-Only Library with Zero Dependencies**: The library is entirely header-based, ensuring portability across platforms and build systems. 

## Getting Started

### Installation

To use this library, simply include the `parser.h` header file in your project. 

```cpp
#include "parser.h"
```

### Basic Usage

Here's a simple example of how to use the library to parse a sequence of tokens:

```cpp
#include "parser.h"
#include <iostream>

int main() {
    using namespace pkuyo::parsers;

    // Create the parsers to match 'a' followed by 'b'
    constexpr auto ab_parser = SingleValue<char>('a') >> 'b';

    // Parse the input "ab"
    std::string input = "ab";
    auto result = ab_parser.Parse(input);

    // type of result is 'std::optional<std::tuple<char,char>>'
    if (result) {
        std::cout << "Parsing succeeded!" << std::endl;
    } else {
        std::cout << "Parsing failed!" << std::endl;
    }

    return 0;
}
```

### Advanced Usage

#### Error Handling

```cpp
// Custom error handling
parser_error_handler<YourToken>::DefaultError([](auto & parser, auto && token) {
    if (token) {
        std::cerr << "Unexpected token: " << *token
                  << " at parser: " << parser.Name() << std::endl;
    } else {
        std::cerr << "Unexpected EOF at parser: " << parser.Name() << std::endl;
    }
});

// Set panic mode recovery points
parser_error_handler<YourToken>::DefaultRecovery([](const YourToken & c) {
    return c == ';';  // Recover parsing at semicolons
});
```



#### Lazy Parsing

```cpp

struct lazy_parser;

constexpr auto parser = Lazy<char,lazy_parser>();

constexpr auto real_parser = SingleValue<char>('a') >> 'b';

struct lazy_parser : public base_parser<char,lazy_parser> {
    
    //The return type MUST be the exact type name; auto cannot be used.
    std::optional<char> parse_impl(auto& begin, auto end) const {
        return real_parser.Parse(begin,end);
    }
    bool peek_impl(auto begin, auto end) const {
        return real_parser.Peek(begin,end);
    }
};

std::string input  = "aab";
auto result = parser.Parse(input);

```


#### Semantic Actions
```cpp
// The parameter types of the lambda expression MUST be the exact type names; auto cannot be used.

// Value transformation 
constexpr auto IntParser = SingleValue<char,int>([](char c){ return isdigit(c); })
    >>= [](int val) { return val * 2; };

// Side-effect handling
constexpr auto LogParser = Check<char>('[')
    <<= [](nullptr_t) { std::cout << "Start array" << std::endl; };
```

### Examples
For more complex usage scenarios, refer to the examples in the [examples](examples) directory.

## API Reference


### Global Method

| Method              | Description                                         |
|---------------------|-----------------------------------------------------|
| `Regex()`           | Create a regex-matching parser   (only for runtime) |
| `Check()`           | Create a parser only check single token             |
| `SeqCheck()`        | Create a parser only check multi tokens             |
| `Str()`             | Create a string-matching parser                     |
| `SingleValue()`     | Create a value parser                               |
| `SinglePtr()`       | Create a value parser (return unique_ptr<>)         |
| `SeqValue()`        | Create a multi-value parser                         |
| `SeqPtr()`          | Create a multi-value parser (return unique_ptr<>)   |
| `DefaultError()`    | Sets a default error handler for all parsers        |
| `DefaultRecovery()` | Sets a default panic mode recovery function         |
| `Name()`            | Sets an alias for parser.                           |


### base_parser
base_parser class used for constructing parser combinators and actual expression parsing.

| Operator / Method | Function                                                                                    |
|-------------------|---------------------------------------------------------------------------------------------|
| `>>`              | Sequential composition (return `tuple<left,right>`)                                         |
| `\|`              | Choice composition     (return `std::variant<l_type,r_type>(l_type != r_type)` or `l_type`) |
| `>>=`             | Value transformation mapping                                                                |
| `<<=`             | Side-effect actions                                                                         |
| `!`               | Create a negation Parser (Prediction Only)                                                  |
| `&`               | Create a Parser (Prediction Only)                                                           |
| `~`               | Create a optional parser      (return `optional<t>`)                                        |
| `*`               | Create a zero-or-more repetition parser  (return `vector<t>` or `basic_string<t>`)          |
| `+`               | Create a one-or-more repetition parser   (return `vector<t>` or `basic_string<t>`)          |
| `-`               | Create a parser ignore original result and return `nullptr`                                 |
| `&&`              | Creates a parser_where to filter results.                                                   |



## Contributing

Contributions are welcome! Please open an issue or submit a pull request if you have any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Author

- **pkuyo** - [GitHub](https://github.com/pkuyo)

