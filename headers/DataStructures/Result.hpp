#ifndef RESULT_H_
#define RESULT_H_

#include <variant>

template <typename T, typename Error = int>
struct Result
{
    std::variant<T, Error> variant;

    Result() : variant() {}
    Result(T value) : variant(value) {}
    Result(Error error) : variant(error) {}
    Result(std::variant<T, Error> variant) : variant(variant) {}

    bool is_value() const { return variant.index() == 0; }
    bool is_error() const { return variant.index() == 1; }

    T &value() { return std::get<T>(variant); }
    const T &value() const { return std::get<T>(variant); }

    Error &error() { return std::get<Error>(variant); }
    const Error &error() const { return std::get<Error>(variant); }

    void emplace(const T &value) { variant.template emplace<T>(value); }
    void emplace(const Error &error) { variant.template emplace<Error>(error); }

    void emplace(T &&value) { variant.template emplace<T>(std::forward<T>(value)); }
    void emplace(Error &&error) { variant.template emplace<Error>(std::forward<Error>(error)); }

    operator std::variant<T, Error>() { return variant; }
};

template <typename T, typename Error = int>
Result<T, Error> make_result(T value)
{
    return Result<T, Error>(std::variant<T, Error>(value));
}

template <typename T, typename Error = int>
Result<T, Error> make_result(Error error)
{
    return Result<T, Error>(std::variant<T, Error>(error));
}

#endif // RESULT_H_