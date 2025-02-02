# LightParser - A Lightweight C++ Parser Library

![C++](https://img.shields.io/badge/C++-20-blue.svg)  ![License](https://img.shields.io/badge/License-MIT-green.svg)

LightParser is a lightweight parser combinator library based on C++20, designed using the parser combinator pattern to help developers quickly build flexible recursive descent parsers.

## Features

- **Parser Composition**: Easily combine parsers using operators like `>>` (then), `|` (or), and `[]` (map).
- **Error Handling**: Built-in support for error recovery and custom error handling.
- **Lazy Parsing**: Supports lazy initialization for recursive parser definitions.
- **Customizable Parsers**: Create parsers that return custom types, including smart pointers and user-defined types.
- **Panic Mode Recovery**: Provides a mechanism for recovering from parsing errors by synchronizing to a known point in the input.
- **Header-Only Library with Zero Dependencies**: The library is entirely header-based, ensuring portability across platforms and build systems. 
## Getting Started

### Installation

To use this library, simply include the `parser.h` header file in your project. The library is header-only, so no additional compilation steps are required.

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
    std::vector<char> input = {'a', 'b'};
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
container.DefaultError([](auto& parser, auto& token) {
    if (token) {
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

std::vector<char> input = {'a', 'a', 'b'};
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

## API Reference


### parser_container
parser_container class used for storing parsers, managing the lifecycle of parsers.

| Method              | Description                                   |
|---------------------|-----------------------------------------------|
| `Str()`             | Create a string-matching parser               |
| `SingleValue()`     | Create a value parser                         |
| `SinglePtr()`       | Create a value parser (return unique_ptr<>)   |
| `Then()`            | Create a sequential combinator parser         |
| `Or()`              | Create a choice combinator parser             |
| `Many()`            | Create a zero-or-more repetition parser       |
| `More()`            | Create a one-or-more repetition parser        |
| `Optional()`        | Create a optional parser                      |
| `DefaultError()`    | Sets a default error handler for all parsers  |
| `DefaultRecovery()` | Sets a default panic mode recovery function   |


### parser_wrapper
parser_wrapper class used for constructing parser combinators and actual expression parsing.

| Operator/Method | Function                                |
|-----------------|-----------------------------------------|
| `>>`            | Sequential composition                  |
| `\|`            | Choice composition                      |
| `[]`            | Value transformation mapping            |
| `>>=`           | Side-effect actions                     |
| `Many()`        | Create a zero-or-more repetition parser |
| `More()`        | Create a one-or-more repetition parser  |
| `Optional()`    | Create a optional parser                |
| `OnError()`     | Sets a error handler for parsers        |               |
| `Recovery()`    | Sets a panic mode recovery function.    |
| `Name()`        | Sets an alias for parser.               |


### Parser Types

- **`parser_check`**: Matches a single token and returns `nullptr_t`.
- **`parser_ptr`**: Matches a single token and returns a `std::unique_ptr`.
- **`parser_value`**: Matches a single token and returns a value.
- **`parser_str`**: Matches a string of tokens.
- **`parser_then`**: Combines two parsers sequentially.
- **`parser_or`**: Combines two parsers with an OR condition.
- **`parser_many`**: Matches zero or more occurrences of a parser.
- **`parser_more`**: Matches one or more occurrences of a parser.
- **`parser_optional`**: Matches zero or one occurrence of a parser.
- **`parser_lazy`**: Defines a parser lazily for recursive parsing.
- **`parser_map`**: Maps the result of a parser to a different type.


## Contributing

Contributions are welcome! Please open an issue or submit a pull request if you have any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Author

- **pkuyo** - [GitHub](https://github.com/pkuyo)

