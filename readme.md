# LightParser - A Lightweight C++ Parser-Combinator Library

![C++](https://img.shields.io/badge/C++-20-blue.svg)  ![License](https://img.shields.io/badge/License-MIT-green.svg)

LightParser is a lightweight parser combinator library based on C++20, designed using the parser combinator pattern to help developers quickly build flexible recursive descent parsers.

**⚠️ Note:** The library is currently in the testing phase and may be unstable; please exercise caution when using it in production environments.

## Features

- **Parser Composition**: Easily combine parsers using operators like `>>` (then), `|` (or), and `[]` (map).
- **Error Handling**: Built-in support for error recovery and custom error handling.
- **Lazy Parsing**: Supports lazy initialization for recursive parser definitions.
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

    // Create a parser container
    parser_container<char> container;

    // Define a parser that matches the character 'a'
    auto a_parser = container.SingleValue('a');

    // Define a parser that matches the character 'b'
    auto b_parser = container.SingleValue('b');

    // Combine the parsers to match 'a' followed by 'b'
    auto ab_parser = a_parser >> b_parser;

    // Parse the input "ab"
    std::string input = "ab";
    auto result = ab_parser.Parse(input.begin(), input.end());

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
container.DefaultError([](auto& parser, auto&& current, auto && end) {
    if (current != end) {
        std::cerr << "Unexpected token: " << *token
                  << " at parser: " << parser.Name() << std::endl;
    } else {
        std::cerr << "Unexpected EOF at parser: " << parser.Name() << std::endl;
    }
});

// Set panic mode recovery points
container.DefaultRecovery([](char c) {
    return c == ';';  // Recover parsing at semicolons
});
```



#### Lazy Parsing

```cpp
parser_container<char> container;

auto lazy_parser = container.Lazy([&container]() {
    auto a_parser = container.SingleValue('a');
    auto b_parser = container.SingleValue('b');
    return (a_parser >> lazy_parser.Get()) | b_parser;
});

std::string input  = "aab";
auto result = lazy_parser.Parse(input.begin(), input.end());
```


#### Semantic Actions
```cpp
// Value transformation
auto IntParser = container.SingleValue<int>([](char c){ return isdigit(c); })
    .Map([](int val) { return val * 2; });

// Side-effect handling
auto LogParser = container.Check('[')
    >>= [](auto) { std::cout << "Start array" << std::endl; };
```

### Examples
For more complex usage scenarios, refer to the examples in the [examples](examples) directory.

## API Reference


### parser_container
parser_container class used for storing parsers, managing the lifecycle of parsers.

| Method              | Description                                       |
|---------------------|---------------------------------------------------|
| `Regex()`           | Create a regex-matching parser                    |
| `Check()`           | Create a parser only check single token           |
| `SeqCheck()`        | Create a parser only check multi tokens           |
| `Str()`             | Create a string-matching parser                   |
| `SingleValue()`     | Create a value parser                             |
| `SinglePtr()`       | Create a value parser (return unique_ptr<>)       |
| `SeqValue()`        | Create a multi-value parser                       |
| `SeqPtr()`          | Create a multi-value parser (return unique_ptr<>) |
| `DefaultError()`    | Sets a default error handler for all parsers      |
| `DefaultRecovery()` | Sets a default panic mode recovery function       |


### parser_wrapper
parser_wrapper class used for constructing parser combinators and actual expression parsing.

| Operator/Method | Function                                                   |
|-----------------|------------------------------------------------------------|
| `>>`            | Sequential composition                                     |
| `\|`            | Choice composition                                         |
| `[]`            | Value transformation mapping                               |
| `>>=`           | Side-effect actions                                        |
| `Not()`         | Create a "Not" Parser (Prediction Only)                    |
| `Many()`        | Create a zero-or-more repetition parser                    |
| `More()`        | Create a one-or-more repetition parser                     |
| `Ignore()`      | Create a parser ignore orignal result and return `nullptr` |
| `Optional()`    | Create a optional parser                                   |
| `OnError()`     | Sets a error handler for parsers                           |               |
| `Recovery()`    | Sets a panic mode recovery function.                       |
| `Name()`        | Sets an alias for parser.                                  |



## Contributing

Contributions are welcome! Please open an issue or submit a pull request if you have any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Author

- **pkuyo** - [GitHub](https://github.com/pkuyo)

