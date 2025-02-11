
# LightParser - A Lightweight C++ Parser-Combinator Library

![C++](https://img.shields.io/badge/C++-20-blue.svg)  ![License](https://img.shields.io/badge/License-MIT-green.svg)

LightParser is a lightweight parser combinator library based on C++20, designed using the parser combinator pattern to help developers quickly build flexible recursive descent parsers.

**⚠️ Note:** The library is currently in the testing phase and may be unstable; please exercise caution when using it in production environments.

## Features

- **Parser Composition**: Easily combine parsers using operators like `>>` (then), `|` (or), and `>>=` (map).
- **Lazy Parsing**: Supports lazy initialization for recursive parser definitions.
- **Compile-Time Construction**: Syntax parsers can be constructed at compile-time, enabling efficient and optimized parsing logic without runtime overhead.
- **Customizable Parsers**: Create parsers that return custom types, including smart pointers and user-defined types.
- **Header-Only Library with Zero Dependencies**: The library is entirely header-based, ensuring portability across platforms and build systems.
- **Custom Exception Handling**: Utilize TryCatch, Sync, or custom Parser to handle parsing errors, offering flexibility for various parsing scenarios and enhancing fault tolerance.


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
    string_stream input("ab");
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

#### Semantic Actions
```cpp
using namespace pkuyo::parsers;

// Value transformation 
constexpr auto IntParser = SingleValue<char>([](char c){ return isdigit(c); })
    >>= [](auto val /*,GlobalState& global_state*/ /*,LocalState& global_state*/) 
            { return (val -'0') * 2; };

// Side-effect handling
constexpr auto LogParser = Check<char>('[')
    <<= [](auto /*,GlobalState& global_state*/ /*,LocalState& global_state*/) 
            { std::cout << "Start array" << std::endl; };
```

#### Parsing Context

```cpp
using namespace pkuyo::parsers;

constexpr auto IntParser = SingleValue<char>([](char c){ return isdigit(c); })
    >>= [](auto val ,auto& global_state, auto& state) 
            {
                global_state.sum += (val -'0');
                state.last_value = (val -'0');
                return (val -'0') * 2; 
            };

// You can pass a variable as the global context in the second parameter
YourGlobalState global_stack;
auto result = parser.Parse(input,global_stack);


//The local context will have only one instance per Parse.
auto with_local_state = WithState<YourLocalState>(/*other parsers*/);
```
For context usage scenarios, refer to the examples in the [examples/sax_xml_parser](examples/sax_xml_parser) directory.


#### Lazy Parsing

```cpp
using namespace pkuyo::parsers;

struct lazy_parser;

constexpr auto parser = Lazy<char,lazy_parser>();

constexpr auto real_parser = SingleValue<char>('a') >> 'b';

struct lazy_parser : public base_parser<char,lazy_parser> {
    
    //The return type MUST be the exact type name; auto cannot be used.
    std::optional<char> parse_impl(auto& stream) const {
        return real_parser.Parse(stream);
    }
    bool peek_impl(auto& stream) const {
        return real_parser.Peek(stream);
    }
};

string_stream input("aab");
auto result = parser.Parse(input);
```

#### Token Stream Implementations

```cpp
using namespace pkuyo::parsers;

//token stream implementation for parsing strings.
// (`wstring_stream` for `wchar_t`).
string_stream input("Hello, World!");

// A generic token stream implementation for parsing sequences stored in containers 
// (e.g., `std::vector`).
std::vector<int> tokens = {1, 2, 3, 4};
container_stream<std::vector<int>> input(tokens);

// A token stream implementation for parsing files. It supports buffering and position tracking.
file_stream input("example.txt");
```

#### Error Handling

**Custom error handling**
```cpp
parser_error_handler<YourToken>::DefaultOnError([](auto & parser, 
        auto && token, auto & token_value, auto & token_pos, auto & stream_name) {
    if (token) {
        std::cerr << std::format("Unexpected {} in parser: {}, at: {}",
                                 token_value, parser.Name(), token_pos) << std::endl;
    } else {
        std::cerr << "Unexpected EOF in parser: " << parser.Name() << std::endl;
    }
});

//Set error handling for a specific parser.
auto parser = (....).OnError(....);
```
**Try-Catch parser**
```cpp
auto parser = TryCatch(Str("com-Try"),Str("com-Recovery"),[] (auto&& exception, auto && gloabl_state) {
    std::err << exception.what() << "\n";
});
string_stream tokens("com-Recovery");

//If matching fails, it attempts to re-match using recovery.
auto result = parser.Parse(tokens);
// *result == "com-Recovery"
```

**Sync-Point parser**
```cpp
auto parser = TryCatch(Str("head"),Sync('<'),[] (auto&& exception, auto && gloabl_state) {
    std::err << exception.what() << "\n";
}) >> Str("<com>");

string_stream tokens("no<com>");
//If matching fails, it attempts to re-match at first token that matched '<'.
auto result = parser.Parse(tokens);
// *result == "<com>"
```

### Examples
For more complex usage scenarios, refer to the examples in the [examples](examples) directory.

## API Reference

### Global Method

| Method             | Description                                                                                               |
|--------------------|-----------------------------------------------------------------------------------------------------------|
| `Check()`          | Create a parser only check single token                                                                   |
| `SeqCheck()`       | Create a parser only check multi tokens                                                                   |
| `Str()`            | Create a string-matching parser                                                                           |
| `Until()`          | Create a parser stop at certain token                                                                     |
| `TryCatch()`       | Create a parser that Use a recovery parser instead of a child parser when an error occurs during parsing. |
| `Sync()`           | Create a parser that read token until sync_func or cmp matched.                                           |
| `SingleValue()`    | Create a value parser                                                                                     |
| `SinglePtr()`      | Create a value parser (return unique_ptr<>)                                                               |
| `SeqValue()`       | Create a multi-value parser                                                                               |
| `SeqPtr()`         | Create a multi-value parser (return unique_ptr<>)                                                         |
| `WithState()`      | Create a  parser with a local state                                                                       |
| `DefaultOnError()` | Sets a default error handler for all parsers                                                              |
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
| `OnError()`       | Sets a error handler for parsers                                                            |               
| `Name()`          | Sets an alias for parser.                                                                   |

## Contributing

Contributions are welcome! Please open an issue or submit a pull request if you have any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Author

- **pkuyo** - [GitHub](https://github.com/pkuyo)
