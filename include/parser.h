//
// Created by pkuyo on 2025/1/31.
//

#ifndef LIGHT_PARSER_PARSER_H
#define LIGHT_PARSER_PARSER_H
//
// Created by pkuyo on 2025/1/19.

#include <map>
#include <set>
#include <format>
#include <vector>
#include <variant>
#include <string>
namespace pkuyo::parsers {

    template<size_t N>
    struct ct_string {
        constexpr ct_string(const char(&str)[N]) {
            std::copy_n(str, N, value);
        }

    public:
        static constexpr size_t size = N;
        char value[N]{};
    };


    template<ct_string cts>
    consteval auto operator ""_ct() {
        return cts;
    }


    class parser_exception : public std::runtime_error {

    public:

        parser_exception(const std::string_view error_part, const std::string_view & parser_name)
                : std::runtime_error(std::format("parser exception in {}, at {}",parser_name,error_part)), error_part(error_part),  parser_name(parser_name)
        {}

        std::string_view error_part;
        std::string_view parser_name;



    };
    template<typename token_type>
    class panic_mode_recovery {
    public:
        using sync_pred_t =  std::function<bool(const token_type&)>;

        explicit panic_mode_recovery(sync_pred_t pred) : sync_check(pred) {}

        template<typename Iter>
        Iter recover(Iter it, Iter end) const {
            while(it != end) {
                if(sync_check(*it)) return it;
                ++it;
            }
            return end;
        }

    private:
        sync_pred_t sync_check;
    };



    template<typename token_type>
    class parser_container;


    // 提取原始类型的辅助模板
    template <typename T>
    struct remove_smart_pointer {
        using type = T; // 默认情况，直接是T
    };

    template <typename T>
    struct remove_smart_pointer<std::unique_ptr<T>> {
        using type = T; // 如果是 unique_ptr，则提取其所管理的类型
    };

    template<typename T>
    using remove_smart_pointer_t = remove_smart_pointer<T>::type;



    template<typename C,typename T>
    struct is_array_or_pointer : std::bool_constant<
            std::is_same_v<std::decay_t<T>, C*> ||         // 指针类型
            std::is_same_v<std::decay_t<T>, const C*> ||   // const指针类型
            std::is_array_v<T> && (                           // 原生数组类型
                    std::is_same_v<std::remove_extent_t<T>, C> ||
                    std::is_same_v<std::remove_extent_t<T>, const C>
            )
    > {};
    template<typename C,typename T>
    inline constexpr bool is_array_or_pointer_v = is_array_or_pointer<C,T>::value;

    template<typename T>
    struct remove_extent_or_pointer{
        typedef std::remove_pointer_t<std::remove_extent_t<T>> type;
    };

    template<typename T>
    using remove_extent_or_pointer_t = remove_extent_or_pointer<T>::type;

    template<typename T, typename = void>
    struct is_formattable : std::false_type {};

    template<typename T>
    struct is_formattable<T, std::void_t<decltype(std::formatter<T>())>> : std::true_type {};



    template <typename T, typename Signature>
    struct is_convertible_to_function_impl;

    template <typename T, typename R, typename... Args>
    struct is_convertible_to_function_impl<T, std::function<R(Args...)>> {
    private:
        // 检测 T 是否可以被调用，并且返回值类型是 R，参数类型是 Args...
        template <typename U>
        static auto test(int) -> decltype(
        std::is_convertible<decltype(std::declval<U>()(std::declval<Args>()...)), R>::value,
                std::true_type{}
        );

        template <typename>
        static std::false_type test(...);

    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template <typename T, typename Signature>
    struct is_convertible_to_function
            : std::false_type {};

    template <typename T, typename R, typename... Args>
    struct is_convertible_to_function<T, std::function<R(Args...)>>
            : is_convertible_to_function_impl<T, std::function<R(Args...)>> {};

    template <typename T, typename Signature>
    constexpr bool is_convertible_to_function_v =
            is_convertible_to_function<T, Signature>::value;


}
namespace pkuyo::parsers {


    // parser接口类，不应实际使用
    template<typename token_type>
    class _abstract_parser {
    public:
        typedef token_type token_t;
        typedef std::vector<token_t>::const_iterator input_iterator_t;
        virtual ~_abstract_parser() = default;
        void set_name(const std::string_view & name) { parser_name = name; }
        void set_error_handler(std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)> && func) {error_handler = func;}
        void set_recovery(panic_mode_recovery<token_type> && _recovery) { this->recovery = std::move(_recovery);}

        template<typename T>
        friend class parser_container;

        [[nodiscard]] std::string_view Name() const {return parser_name;}


    protected:
        _abstract_parser() : container(nullptr) {}

        void error_handle_recovery(input_iterator_t &token_it,const input_iterator_t &token_end) {
            auto last_it = token_it;
            token_it = (this->recovery ? this->recovery.value() : this->container->default_recovery).recover(token_it,token_end);
            (this->error_handler ? this->error_handler.value() : this->container->default_error_handler)(*this, last_it == token_end
            ? std::nullopt : std::make_optional(*last_it));

        }

        std::string parser_name;
        parser_container<token_type>* container;

    private:
        std::optional<panic_mode_recovery<token_type>> recovery;
        std::optional<std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)>> error_handler;


    };


    // parser基类，抽象基类
    template<typename token_type, typename return_type>
    class base_parser : public _abstract_parser<token_type> {
    public:


        //便于lazy类型推导
        typedef base_parser parser_base_t;

        typedef token_type token_t;
        typedef std::vector<token_t>::const_iterator input_iterator_t;
        typedef std::optional<return_type> return_t;


    public:
        virtual bool Peek(const input_iterator_t &token_it,const input_iterator_t &token_end) = 0;

        return_t Parse(const input_iterator_t &token_it,const input_iterator_t &token_end) {
            auto it = token_it;
            return parse_impl(it,token_end);
        }

        template<typename It>
        return_t Parse(It token_it,It token_end) {
            std::vector tmp(token_it,token_end);
            return parse_impl(tmp.cbegin(),tmp.cend());
        }


        return_t Parse(input_iterator_t &token_it,const input_iterator_t &token_end) {
            return parse_impl(token_it,token_end);
        }
        virtual ~base_parser() = default;
        virtual return_t parse_impl(input_iterator_t &token_it,const input_iterator_t &token_end) = 0;

    protected:
        base_parser() = default;


    };


}

namespace pkuyo::parsers {

    template<typename TokenType>
    class parser_empty : public base_parser<TokenType, nullptr_t> {

    public:
        using parser_base_t = base_parser<TokenType, nullptr_t>;

        // parse_impl 函数实现
        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t&,
                                               const typename parser_base_t::input_iterator_t&) override {
            return nullptr;
        }

        bool Peek(const typename parser_base_t::input_iterator_t&,
                  const typename parser_base_t::input_iterator_t&) override {

            return true;
        }
    };
    // 基本的parser，仅比对第一个token，匹配成功返回nullptr_t，匹配失败尝试错误恢复策略, 返回std::nullopt
    template<typename token_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_check : public base_parser<token_type,nullptr_t>{
    public:
        typedef base_parser<token_type, nullptr_t> parser_base_t;
        friend class parser_container<token_type>;
    protected:
        explicit parser_check(const cmp_type &_cmp_value) : cmp_value(_cmp_value) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            token_it++;
            return std::make_optional(nullptr);

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == cmp_value;
        }


    private:
        cmp_type cmp_value;
    };

    // 基本的parser，仅比对第一个token，返回以token为参数构造的result_type（智能指针形式），匹配失败尝试错误恢复策略, 返回std::nullopt
    template<typename token_type, typename return_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_ptr : public base_parser<token_type, std::unique_ptr<return_type>> {

    public:
        typedef base_parser<token_type, std::unique_ptr<return_type>> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_ptr(const cmp_type &_cmp_value)
                : cmp_value(_cmp_value){}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            return std::make_optional(std::move(std::make_unique<return_type>(*(token_it++))));

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == cmp_value;
        }

    private:
        cmp_type cmp_value;
    };

    // 基本的parser，仅比对第一个token，返回以token为参数构造的result_type（值形式），匹配失败尝试错误恢复策略, 返回std::nullopt
    template<typename token_type, typename return_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_value : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_value(const cmp_type &_cmp_value)
                : cmp_value(_cmp_value) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)) {
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            return std::make_optional(*(token_it++));

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == cmp_value;
        }

    private:


        cmp_type cmp_value;
    };

    // 基本的parser，仅比对第一个token，返回以token为参数构造的result_type（智能指针形式），匹配失败尝试错误恢复策略, 返回std::nullopt
    template<typename token_type, typename return_type>
    class parser_ptr_with_func : public base_parser<token_type, std::unique_ptr<return_type>> {

    public:
        typedef base_parser<token_type, std::unique_ptr<return_type>> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_ptr_with_func(const std::function<bool(const token_type&)> &_cmp_func)
                : cmp_func(_cmp_func){}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            return std::make_optional(std::move(std::make_unique<return_type>(*(token_it++))));

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && cmp_func(*token_it);
        }

    private:
        std::function<bool(const token_type&)> cmp_func;
    };

    // 基本的parser，仅比对第一个token，返回以token为参数构造的result_type（值形式），匹配失败尝试错误恢复策略, 返回std::nullopt
    template<typename token_type, typename return_type>
    class parser_value_with_func : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_value_with_func(const std::function<bool(const token_type&)> &_cmp_func)
                : cmp_func(_cmp_func){}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            return std::make_optional<return_type>(*(token_it++));

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && cmp_func(*token_it);
        }

    private:
        std::function<bool(const token_type&)> cmp_func;
    };

    // 基本的parser，仅比对第一个token，返回以token为参数构造的result_type（智能指针形式），匹配失败尝试错误恢复策略, 返回std::nullopt
    template<typename token_type, typename return_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_ptr_with_constructor : public base_parser<token_type, std::unique_ptr<return_type>> {

    public:
        typedef base_parser<token_type, std::unique_ptr<return_type>> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_ptr_with_constructor(const cmp_type &_cmp_value,
                                             std::function<std::unique_ptr<return_type>(const token_type &)> && constructor)
        : cmp_value(_cmp_value), constructor(constructor) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)){
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }

            return std::make_optional(std::move((constructor(*(token_it++)))));


        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == cmp_value;
        }

    private:
        std::function<std::unique_ptr<return_type>(const token_type &)> constructor;
        cmp_type cmp_value;
    };

    // 基本的parser，仅比对第一个token，返回以token为参数构造的result_type（值形式），匹配失败尝试错误恢复策略, 返回std::nullopt
    template<typename token_type, typename return_type, typename cmp_type>
    requires std::__weakly_equality_comparable_with<token_type, cmp_type>
    class parser_value_with_constructor : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_value_with_constructor(const cmp_type &_cmp_value,std::function<return_type(const token_type &)> && constructor)
        : cmp_value(_cmp_value), constructor(constructor) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {

            if(!Peek(token_it,token_end)) {
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }

            return std::optional<return_type>(std::move(constructor(*(token_it++))));
        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == cmp_value;
        }

    private:

        std::function<return_type(const token_type &)> constructor;
        cmp_type cmp_value;
    };

    //字符串匹配
    template<typename token_type>
    class parser_str : public base_parser<token_type, std::basic_string_view<token_type>> {

    public:
        typedef base_parser<token_type, std::basic_string_view<token_type>> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_str(const std::basic_string_view<token_type> &_cmp_value)
                : cmp_value(_cmp_value) {}
        parser_base_t::return_t parse_impl(parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            auto last_it = token_it;

            for(auto c_it : cmp_value) {
                if(*token_it != c_it) {
                    parser_base_t::error_handle_recovery(token_it,token_end);
                    return std::nullopt;
                }
                token_it++;
            }
            return cmp_value;

        }
    public:

        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return token_end != token_it && (*token_it) == *cmp_value.begin();
        }

    private:


        std::basic_string_view<token_type> cmp_value;
    };

    // 与运算，顺序满足传入的全部parser，返回每个parser的返回值为参数构造的unique_ptr<result_type>
    template<typename token_type, typename return_type, typename input_type_left,typename input_type_right>
    class parser_then : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_then(base_parser<token_type, input_type_left>* parser_left,base_parser<token_type, input_type_right>* parser_right)
                : children_parsers(std::make_pair(parser_left,parser_right)){}

        parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it
                ,const parser_base_t::input_iterator_t &token_end) override {
            auto last_it = token_it;
            auto left_re = children_parsers.first->parse_impl(token_it,token_end);
            //匹配失败尝试恢复，并复原token索引，返回nullopt
            if(left_re == std::nullopt) {
                parser_base_t::error_handle_recovery(token_it,token_end);

                //复原token索引
                token_it = last_it;
                return std::nullopt;
            }

            auto right_re = children_parsers.second->parse_impl(token_it,token_end);
            if(right_re == std::nullopt) {
                parser_base_t::error_handle_recovery(token_it,token_end);

                //复原token索引
                token_it = last_it;
                return std::nullopt;
            }
            if constexpr (std::is_same_v<input_type_left,nullptr_t>) {
                return std::make_optional(std::move(right_re.value()));
            }
            else if constexpr (std::is_same_v<input_type_right,nullptr_t>) {
                return std::make_optional(std::move(left_re.value()));
            }
            else {
                return std::make_optional<return_type>(std::move(left_re.value()),std::move(right_re.value()));
            }
        }

    public:
        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return children_parsers.first->Peek(token_it,token_end);
        }

    private:
        std::pair<base_parser<token_type, input_type_left>*,base_parser<token_type, input_type_right>*> children_parsers;

    };


    // 或运算，查询第一个满足的子parser并返回，注意仅预测一个token，多于单token的回溯会导致parser_exception
    // 每个子parser的返回类型应为result_type的子类
    template<typename token_type, typename return_type, typename ...input_types>
    class parser_or : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        friend class parser_container<token_type>;

    protected:
        explicit parser_or(base_parser<token_type, input_types>*... parsers) : children_parsers(std::make_tuple(parsers...)){}

        parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it
                ,const parser_base_t::input_iterator_t &token_end) override {
            // 遍历 children_parsers，找到第一个 Peek 返回 true 的解析器并调用其 parse_impl
            return parse_impl_or(token_it, token_end, std::make_index_sequence<sizeof...(input_types)>());
        }
    public:
        bool Peek(const parser_base_t::input_iterator_t &token_it,const parser_base_t::input_iterator_t &token_end) override {
            return peek_impl(token_it,token_end,std::make_index_sequence<sizeof...(input_types)>());
        }

    private:
        template<std::size_t... Indices>
        parser_base_t::return_t parse_impl_or(typename parser_base_t::input_iterator_t &token_it,
                                           const parser_base_t::input_iterator_t &token_end,
                                           std::index_sequence<Indices...>) {

            // 遍历元组并尝试找到第一个 Peek 返回 true 的解析器
            return parse_first_valid(token_it, token_end, std::get<Indices>(children_parsers)...);
        }

        template <typename Firstparse_implr, typename... Remainingparse_implrs>
        parser_base_t::return_t parse_first_valid(typename parser_base_t::input_iterator_t &token_it,
                                                  const parser_base_t::input_iterator_t &token_end,
                                                  Firstparse_implr *first, Remainingparse_implrs *...remaining) {

            if (first->Peek(token_it, token_end)) {
                auto last_it = token_it;
                auto re = first->parse_impl(token_it, token_end);
                if(re == std::nullopt) {
                    parser_base_t::error_handle_recovery(token_it,token_end);
                    //复原token索引
                    token_it = last_it;
                    return std::nullopt;
                }
                return re;
            } else if constexpr (sizeof...(Remainingparse_implrs) > 0) {
                // 否则递归处理剩余的解析器
                return parse_first_valid(token_it, token_end, remaining...);
            } else {
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;

            }
        }
        template<std::size_t... Indices>
        bool peek_impl(const typename parser_base_t::input_iterator_t &token_it
                ,const parser_base_t::input_iterator_t &token_end
                ,std::index_sequence<Indices...>)  {
            return (std::get<Indices>(children_parsers)->Peek(token_it,token_end) || ...);
        }
    private:
        std::tuple<base_parser<token_type, input_types>*...> children_parsers;

    };

    // 匹配多个相同元素
    template<typename token_type, typename child_return_type>
    class parser_many : public base_parser<token_type, std::vector<child_return_type>> {
    public:
        typedef base_parser<token_type, std::vector<child_return_type>> parser_base_t;
        friend class parser_container<token_type>;

    public:


        bool Peek(const typename parser_base_t::input_iterator_t &,
                  const typename parser_base_t::input_iterator_t &) override {
            // Many 允许零次匹配
            return true;
        }

    protected:

        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it,
                                                    const typename parser_base_t::input_iterator_t &token_end) override {
            std::vector<child_return_type> results;
            while (token_it != token_end) {
                if(child_parser->Peek(token_it, token_end)) {
                    auto result = child_parser->parse_impl(token_it, token_end);
                    if (result == std::nullopt) {
                        parser_base_t::error_handle_recovery(token_it,token_end);

                        //遇到错误停止many匹配
                        break;
                    }
                    results.push_back(std::move(result.value()));
                    continue;
                }
                break;
            }
            return std::make_optional(std::vector<child_return_type>(std::move(results)));
        }

        explicit parser_many(base_parser<token_type, child_return_type>* child): child_parser(child) {}


    private:
        base_parser<token_type, child_return_type>* child_parser;
    };

    // 匹配多个相同元素
    template<typename token_type, typename child_return_type>
    class parser_more : public base_parser<token_type, std::vector<child_return_type>> {
    public:
        typedef base_parser<token_type, std::vector<child_return_type>> parser_base_t;
        friend class parser_container<token_type>;

    public:


        bool Peek(const typename parser_base_t::input_iterator_t & it,
                  const typename parser_base_t::input_iterator_t & end) override {
            return child_parser->Peek(it, end);
        }

    protected:

        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it,
                                                    const typename parser_base_t::input_iterator_t &token_end) override {

            auto first_result = child_parser->parse_impl(token_it, token_end);
            if (first_result == std::nullopt) {
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            std::vector<child_return_type> results{std::move(first_result.value())};

            while (token_it != token_end) {
                if(child_parser->Peek(token_it, token_end)) {
                    auto result = child_parser->parse_impl(token_it, token_end);
                    if (result == std::nullopt) {
                        parser_base_t::error_handle_recovery(token_it,token_end);

                        break;
                    }
                    results.push_back(std::move(result.value()));
                    continue;
                }
                break;
            }
            return std::make_optional(std::vector<child_return_type>(std::move(results)));
        }

        explicit parser_more(base_parser<token_type, child_return_type>* child): child_parser(child) {}


    private:
        base_parser<token_type, child_return_type>* child_parser;
    };
    // 匹配可选元素，无可选返回nullptr
    template<typename token_type, typename return_type>
    class parser_optional : public base_parser<token_type, std::optional<return_type>> {
    public:
        typedef base_parser<token_type, std::optional<return_type>> parser_base_t;
        friend class parser_container<token_type>;

    public:
        bool Peek(const typename parser_base_t::input_iterator_t &,
                  const typename parser_base_t::input_iterator_t &) override {
            return true;
        }

    protected:
        explicit parser_optional(base_parser<token_type, return_type>* child): child_parser(child) {}
        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it,
                                                    const typename parser_base_t::input_iterator_t &token_end) override {
            std::optional<return_type> result(std::nullopt);
            if(child_parser->Peek(token_it,token_end)) {
                auto re = child_parser->parse_impl(token_it, token_end);
                if (re == std::nullopt) {
                    parser_base_t::error_handle_recovery(token_it, token_end);
                    return std::nullopt;
                }
                 result = std::move(re.value());
            }

            return std::make_optional(std::move(result));
        }

    private:
        base_parser<token_type, return_type>* child_parser;
    };


    // 延迟初始化解析器，解决parser之间的递归依赖，无自行token判断，无错误恢复
    template<typename token_type, typename return_type>
    class parser_lazy : public base_parser<token_type, return_type> {

    public:
        typedef base_parser<token_type, return_type> parser_base_t;
        typedef std::function<base_parser<token_type, return_type>*()> parser_factory_t;
        friend class parser_container<token_type>;

    public:


        bool Peek(const typename parser_base_t::input_iterator_t &token_it,
                  const typename parser_base_t::input_iterator_t &token_end) override {
            if (!real_parser) initialize();
            return real_parser->Peek(token_it, token_end);
        }

    protected:
        explicit parser_lazy(parser_factory_t factory) : factory(factory) {}
        typename parser_base_t::return_t parse_impl(typename parser_base_t::input_iterator_t &token_it,
                                                    const typename parser_base_t::input_iterator_t &token_end) override {
            if (!real_parser) initialize();
            return real_parser->parse_impl(token_it, token_end);
        }

    private:
        void initialize() {
            real_parser = factory();
        }

        parser_factory_t factory;
        base_parser<token_type, return_type>* real_parser = nullptr;
    };

    // 将解析结果转换为目标类型，无自行token判断，无错误恢复
    template<typename token_type, typename SourceType, typename TargetType>
    class parser_map : public base_parser<token_type, TargetType> {
    public:
        typedef base_parser<token_type, TargetType> parser_base_t;
        typedef std::function<TargetType(SourceType)> mapper_t;
        friend class parser_container<token_type>;

    public:
        bool Peek(const typename parser_base_t::input_iterator_t &token_it,
                  const typename parser_base_t::input_iterator_t &token_end) override {
            return source->Peek(token_it, token_end);
        }

    protected:
        parser_map(base_parser<token_type, SourceType>* source, mapper_t mapper)
                : source(source), mapper(mapper) {}

        typename parser_base_t::return_t parse_impl(
                typename parser_base_t::input_iterator_t &token_it,
                const typename parser_base_t::input_iterator_t &token_end
        ) override {
            // 解析源结果
            auto source_result = source->parse_impl(token_it, token_end);
            if (source_result == std::nullopt) {
                parser_base_t::error_handle_recovery(token_it,token_end);
                return std::nullopt;
            }
            return std::make_optional(std::move(mapper(std::move(source_result.value()))));
        }
    private:
        base_parser<token_type, SourceType>* source;
        mapper_t mapper;
    };
}

namespace pkuyo::parsers {

    //包装类，防止裸指针，便于运算符处理
    template<typename token_type, typename return_type>
    class parser_wrapper {

    public:
        parser_wrapper(parser_container<token_type>& c, base_parser<token_type, return_type>* p)
                : container(c), parser(p) {}

        base_parser<token_type, return_type>* operator->() { return parser; }

        base_parser<token_type, return_type>* Get() {return parser;}

        template<typename other_return_t>
        auto operator[](parser_wrapper<token_type, other_return_t> other) {
            return *this >> other.Optional();
        }

        // 重载 | 运算符，创建 Or 解析器
        template<typename other_return_t>

        auto operator|(parser_wrapper<token_type, other_return_t> other) {
            if constexpr ( std::is_same_v<return_type,other_return_t>){
                return container.template Or<return_type>(parser, other.parser);
            }
            //非指针类型
            else if constexpr (std::is_same_v<remove_smart_pointer_t<return_type>, return_type> ||
                               std::is_same_v<remove_smart_pointer_t<other_return_t>, other_return_t>) {
                return container.template Or<std::variant<return_type,other_return_t>>(parser, other.parser);
            }
            else {
                if constexpr (
                        std::is_base_of_v<remove_smart_pointer_t<return_type>, remove_smart_pointer_t<other_return_t>> ||
                        std::is_same_v<other_return_t, nullptr_t> )
                    //重要处理，nullptr_t直接吃掉
                    return container.template Or<return_type>(parser, other.parser);
                else if constexpr (
                        std::is_base_of_v<remove_smart_pointer_t<other_return_t>, remove_smart_pointer_t<return_type>> ||
                        std::is_same_v<return_type, nullptr_t>)
                    return container.template Or<other_return_t>(parser, other.parser);
                else
                    return container.template Or<std::variant<return_type, other_return_t>>(parser, other.parser);
            }

        }

        // 重载 >> 运算符，创建 Then 解析器
        template<typename other_return_t>
        auto operator>>(parser_wrapper<token_type, other_return_t> other) {
            return container.template Then<return_type, other_return_t>(parser, other.parser);
        }
        template<typename other_return_t>
        auto Then(parser_wrapper<token_type, other_return_t> other) {
            return container.template Then<return_type,other_return_t>(parser,other.parser);
        }

        template<typename return_t,typename ...other_return_t>
        auto Or(const parser_wrapper<token_type, other_return_t...> & other) {
            return container.template Or<return_t,return_type,other_return_t...>(parser,other);
        }

        auto Many() {
            return container.template Many<return_type>(parser);
        }

        auto More() {
            return container.template More<return_type>(parser);
        }

        auto Optional() {
            return container.template Optional<return_type>(parser);
        }

        template<typename Mapper>
        auto Map(Mapper mapper) {
            using MappedType = decltype(mapper(std::declval<return_type>()));
            return container.template Map<return_type, MappedType>(parser, mapper);
        }


        parser_wrapper& Name(const std::string_view & new_name) {
            parser->set_name(new_name);
            return *this;
        }
        template<typename FF>
        parser_wrapper& OnError(FF && func) {
            parser->set_error_handler(func);
            return *this;
        }

        base_parser<token_type, return_type>* parser;

    private:
        parser_container<token_type>& container;


    };

}

namespace pkuyo::parsers {


    // parser的容器
    template<typename token_type>
    class parser_container {

    public:
        template<typename T>
        friend class _abstract_parser;

        parser_container() : default_error_handler([](const _abstract_parser<token_type> & self,const std::optional<token_type> & token) {

            if(token) {
                if constexpr(is_formattable<token_type>::value) {
                    throw parser_exception(std::format("'{}'",token.value()), self.Name());
                }
                else {
                    throw parser_exception("TOKEN", self.Name());
                }
            }
            else {
                throw parser_exception("EOF", self.Name());
            }
        }), default_recovery([](const auto &) {return false;}) {}

        ~parser_container() {
            parsers_set.clear();
        }

        auto Str(const std::basic_string_view<token_type> & cmp_value) {
            using result_parser_t = parser_str<token_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"String",result_parser_t>(cmp_value));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,std::basic_string_view<token_type>>(*this, ptr);

        }


        template<typename return_type = token_type, typename cmp_type>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type>
        auto SingleValue(const cmp_type & cmp_value) {
            //针对字符串的处理
            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_value<token_type,return_type,std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(
                        std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type,return_type>(*this, ptr);
            }
            else {
                using result_parser_t = parser_value<token_type,return_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(cmp_value));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, return_type>(*this, ptr);
            }

        }



        template<typename return_type, typename cmp_type>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type>
        auto SinglePtr(const cmp_type & cmp_value) {
            //针对字符串的处理
            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_ptr<token_type,return_type,std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(
                        std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);
            }
            else {
                using result_parser_t = parser_ptr<token_type,return_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(cmp_value));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);
            }

        }

        template<typename return_type = token_type, typename FF>
        requires is_convertible_to_function_v<FF,std::function<bool(const token_type &)>>
        auto SingleValue(FF && compare_func) {

            using result_parser_t = parser_value_with_func<token_type, return_type>;
            auto tmp = std::unique_ptr<result_parser_t>(
                    create_parser<"Single", result_parser_t>(std::forward<FF>(compare_func)));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type, return_type>(*this, ptr);
        }



        template<typename return_type,typename FF>
        requires is_convertible_to_function_v<FF,std::function<bool(const token_type &)>>
        auto SinglePtr(FF && compare_func) {

            using result_parser_t = parser_ptr_with_func<token_type, return_type>;
            auto tmp = std::unique_ptr<result_parser_t>(
                    create_parser<"Single", result_parser_t>(std::forward<FF>(compare_func)));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);


        }

        template<typename return_type = token_type, typename cmp_type,typename FF>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type> &&
                is_convertible_to_function_v<FF,std::function<return_type(const token_type &)>>
        auto SingleValue(const cmp_type & cmp_value,FF && constructor) {
            //针对字符串的处理
            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_value_with_constructor<token_type,return_type,std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(
                        std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value),std::forward<FF>(constructor)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type,return_type>(*this, ptr);
            }
            else {
                using result_parser_t = parser_value_with_constructor<token_type,return_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(cmp_value,std::forward<FF>(constructor)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, return_type>(*this, ptr);
            }

        }



        template<typename return_type, typename cmp_type,typename FF>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type> &&
                is_convertible_to_function_v<FF,std::function<std::unique_ptr<return_type>(const token_type &)>>
        auto SinglePtr(const cmp_type & cmp_value,
                     FF && constructor) {
            //针对字符串的处理
            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_ptr_with_constructor<token_type,return_type,
                std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(
                        std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value),std::forward<FF>(constructor)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);
            }
            else {
                using result_parser_t = parser_ptr_with_constructor<token_type,return_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Single",result_parser_t>(cmp_value,std::forward<FF>(constructor)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::unique_ptr<return_type>>(*this, ptr);
            }

        }


        template<typename return_type, typename ...cmp_type>
        parser_wrapper<token_type, nullptr_t> Check(const cmp_type &... cmp_value) {
            using result_parser_t = parser_check<token_type, return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Check",result_parser_t>(token_type(cmp_value...)));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,nullptr_t>(*this, ptr);
        }


        template<typename cmp_type>
        requires std::__weakly_equality_comparable_with<token_type, cmp_type>
        parser_wrapper<token_type,nullptr_t> Check(const cmp_type & cmp_value) {

            //针对字符串的处理
            if constexpr (is_array_or_pointer_v<char,cmp_type> ||
                          is_array_or_pointer_v<wchar_t,cmp_type>) {
                using result_parser_t = parser_check<token_type,std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Check",result_parser_t>(std::basic_string_view<remove_extent_or_pointer_t<cmp_type>>(cmp_value)));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type,nullptr_t>(*this, ptr);
            }
            else {
                using result_parser_t = parser_check<token_type,cmp_type>;
                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Check",result_parser_t>(cmp_value));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, nullptr_t>(*this, ptr);
            }
        }

        template<typename input_type_left, typename input_type_right>

        auto Then(base_parser<token_type, input_type_left>*parser_left, base_parser<token_type, input_type_right>* parser_right) {

            if constexpr (std::is_same_v<input_type_left,nullptr_t>) {
                using result_parser_t = parser_then<token_type,input_type_right,input_type_left, input_type_right>;

                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Then", result_parser_t>(parser_left,parser_right));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, input_type_right>(*this, ptr);
            }
            else if constexpr (std::is_same_v<input_type_right,nullptr_t>){
                using result_parser_t = parser_then<token_type, input_type_left,input_type_left, input_type_right>;

                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Then", result_parser_t>(parser_left,parser_right));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, input_type_left>(*this, ptr);
            }
            else{
                using result_parser_t = parser_then<token_type, std::pair<input_type_left,input_type_right>,input_type_left, input_type_right>;

                auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Then", result_parser_t>(parser_left,parser_right));
                auto ptr = tmp.get();
                parsers_set.emplace(std::move(tmp));
                return parser_wrapper<token_type, std::pair<input_type_left,input_type_right>>(*this, ptr);
            }


        }

        template<typename return_type,typename ...input_types>
        parser_wrapper<token_type,return_type> Or(base_parser<token_type, input_types>*... parsers) {
            using result_parser_t = parser_or<token_type,return_type,input_types...>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Or",result_parser_t>(parsers...));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,return_type>(*this, ptr);
        }

        template<typename function_type>
        //不用std::function以便于类型推导
        auto Lazy(function_type && factory) {
            //推导std::function类型
            using factory_type = decltype(std::function{std::forward<function_type>(factory)});

            //推导base_parser类型
            using base_parser = std::remove_pointer_t<decltype(factory())>::parser_base_t;

            //推导出parsr的result（base_parser去除指针）
            using return_type = typename base_parser::return_t::value_type;
            using result_parser_t = parser_lazy<token_type, return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Lazy",result_parser_t>(factory_type(factory)));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,return_type>(*this, ptr);
        }

        template<typename return_type>
        auto Optional(base_parser<token_type, return_type>* child) {
            using result_parser_t = parser_optional<token_type, return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Optional",result_parser_t>(child));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));

            return parser_wrapper<token_type, std::optional<return_type>>(*this, ptr);

        }



        template<typename child_return_type>
        parser_wrapper<token_type, std::vector<child_return_type>> Many(base_parser<token_type, child_return_type>* child) {
            using result_parser_t = parser_many<token_type, child_return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Many",result_parser_t>(child));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type, std::vector<child_return_type>>(*this, ptr);
        }
        template<typename child_return_type>
        parser_wrapper<token_type, std::vector<child_return_type>> More(base_parser<token_type, child_return_type>* child) {
            using result_parser_t = parser_more<token_type, child_return_type>;

            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"More",result_parser_t>(child));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type, std::vector<child_return_type>>(*this, ptr);
        }

        template<typename source_type, typename target_type>
        parser_wrapper<token_type,target_type> Map(base_parser<token_type, source_type>* source,std::function<target_type(source_type)> mapper) {
            using result_parser_t = parser_map<token_type, source_type, target_type>;
            auto tmp = std::unique_ptr<result_parser_t>(create_parser<"Map",result_parser_t>(source, mapper));
            auto ptr = tmp.get();
            parsers_set.emplace(std::move(tmp));
            return parser_wrapper<token_type,target_type>(*this, ptr);
        }

        parser_wrapper<token_type,nullptr_t> Empty(){
            if(!empty) {
                auto tmp = std::unique_ptr<parser_empty<token_type>>(create_parser<"Empty",parser_empty<token_type>>());
                empty = tmp.get();
                parsers_set.emplace(std::move(tmp));
            }
            return parser_wrapper<token_type,nullptr_t>(*this,empty);
        }

        template<typename FF>
        parser_container& DefaultRecovery(FF && sync_func)
        requires is_convertible_to_function_v<FF,std::function<bool(const token_type &)>>{
            default_recovery = panic_mode_recovery<token_type>(sync_func);
            return *this;
        }
        template<typename FF>
        parser_container&  DefaultError(FF && on_error)
        requires is_convertible_to_function_v<FF,std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)>>{
            default_error_handler =std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)> (on_error);
            return *this;
        }

    private:

        template<ct_string str, typename T, typename... Args>
        T* create_parser(Args&&... args) {
            auto p = new T((args)...);
            p->set_name(str.value);
            p->container = this;
            return p;
        }



    private:
        std::set<std::unique_ptr<_abstract_parser<token_type>>> parsers_set;
        panic_mode_recovery<token_type> default_recovery;
        std::function<void(const _abstract_parser<token_type> &,const std::optional<token_type> &)> default_error_handler;
        parser_empty<token_type>* empty;

    };


}


#endif //LIGHT_PARSER_PARSER_H
