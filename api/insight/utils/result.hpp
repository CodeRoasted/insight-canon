#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace insight
{

/**
 * Result<T> — A type-safe error handling wrapper
 * Inspired by Rust's Result<T, E> and C++23's std::expected
 *
 * Usage:
 *   Result<int> compute() {
 *     if (error) return Err("reason");
 *     return Ok(42);
 *   }
 *
 *   auto r{compute()};
 *   if (r) { int val = r.value(); }
 *   else { std::string err = r.error(); }
 */
template <typename T> class Result
{
  public:
    // ── Constructors ──

    /// Success case: wrap value
    explicit Result(T value) : data_(std::move(value)) {}

    /// Error case: wrap error string
    explicit Result(std::string error) : data_(std::move(error)) {}

    /// Move constructor
    Result(Result&& other) noexcept = default;

    /// Copy constructor
    Result(const Result& other) = default;

    /// Move assignment
    Result& operator=(Result&& other) noexcept = default;

    /// Copy assignment
    Result& operator=(const Result& other) = default;

    /// Destructor
    ~Result() = default;

    // ── Queries ──

    /// Check if result is success
    [[nodiscard]] bool is_ok() const
    {
        return std::holds_alternative<T>(data_);
    }

    /// Check if result is error
    [[nodiscard]] bool is_err() const
    {
        return std::holds_alternative<std::string>(data_);
    }

    /// Implicit bool conversion (true if success)
    explicit operator bool() const
    {
        return is_ok();
    }

    // ── Value Access ──

    /// Get value (throws if error)
    [[nodiscard]] T& value()
    {
        if (is_err())
        {
            throw std::runtime_error("Result::value() called on error: " + error());
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] const T& value() const
    {
        if (is_err())
        {
            throw std::runtime_error("Result::value() called on error: " + error());
        }
        return std::get<T>(data_);
    }

    /// Get value or default
    [[nodiscard]] T value_or(T default_value) const
    {
        if (is_ok())
        {
            return std::get<T>(data_);
        }
        return default_value;
    }

    /// Get error message (throws if success)
    [[nodiscard]] std::string& error()
    {
        if (is_ok())
        {
            throw std::runtime_error("Result::error() called on success");
        }
        return std::get<std::string>(data_);
    }

    [[nodiscard]] const std::string& error() const
    {
        if (is_ok())
        {
            throw std::runtime_error("Result::error() called on success");
        }
        return std::get<std::string>(data_);
    }

    // ── Monadic Operations ──

    /// Transform value if success, otherwise pass error through
    template <typename F>
    [[nodiscard]] auto map(F&& func) const -> Result<std::invoke_result_t<F, const T&>>
    {
        if (is_ok())
        {
            return Result(std::forward<F>(func)(std::get<T>(data_)));
        }
        return Result(std::get<std::string>(data_));
    }

    /// Chain operations that return Result
    template <typename F>
    [[nodiscard]] auto and_then(F&& func) const -> std::invoke_result_t<F, const T&>
    {
        if (is_ok())
        {
            return std::forward<F>(func)(std::get<T>(data_));
        }
        using RetType = std::invoke_result_t<F, const T&>;
        return RetType(std::get<std::string>(data_));
    }

    /// Apply function on error
    template <typename F> [[nodiscard]] auto map_err(F&& func) const -> Result<T>
    {
        if (is_err())
        {
            return Result(std::forward<F>(func)(std::get<std::string>(data_)));
        }
        return Result(std::get<T>(data_));
    }

    /// Handle error case with function
    template <typename F>
    [[nodiscard]] auto or_else(F&& func) const -> std::invoke_result_t<F, const std::string&>
    {
        if (is_err())
        {
            return std::forward<F>(func)(std::get<std::string>(data_));
        }
        using RetType = std::invoke_result_t<F, const std::string&>;
        return RetType(std::get<T>(data_));
    }

  private:
    std::variant<T, std::string> data_;
};

// ── Specialization for void (error-only results) ──

template <> class Result<void>
{
  public:
    explicit Result() = default;
    explicit Result(std::string error) : error_(std::move(error)) {}

    Result(Result&&) noexcept = default;
    Result(const Result&) = default;
    Result& operator=(Result&&) noexcept = default;
    Result& operator=(const Result&) = default;
    ~Result() = default;

    [[nodiscard]] bool is_ok() const
    {
        return error_.empty();
    }

    [[nodiscard]] bool is_err() const
    {
        return !error_.empty();
    }

    explicit operator bool() const
    {
        return is_ok();
    }

    [[nodiscard]] const std::string& error() const
    {
        return error_;
    }

    template <typename F> [[nodiscard]] auto map(F&& func) const -> Result<std::invoke_result_t<F>>
    {
        if (is_ok())
        {
            return Result(std::forward<F>(func)());
        }
        return Result(error_);
    }

    template <typename F> [[nodiscard]] auto and_then(F&& func) const -> std::invoke_result_t<F>
    {
        if (is_ok())
        {
            return std::forward<F>(func)();
        }
        using RetType = std::invoke_result_t<F>;
        return RetType(error_);
    }

  private:
    std::string error_;
};

} // namespace insight
