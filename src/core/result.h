#pragma once

#include <QString>
#include <type_traits>
#include <utility>

namespace Remus {

/**
 * @brief Lightweight result type carrying a value or an error message.
 *
 * Similar to std::expected (C++23) but available in C++17.
 * Use where functions currently return empty structs + qWarning() side effects.
 *
 * Usage:
 *   Result<GameMetadata> r = provider.getById(id);
 *   if (r) { use(*r); }
 *   else   { log(r.error()); }
 */
template <typename T>
class Result {
    struct ErrorTag {};

public:
    /// Construct a success result
    static Result ok(T value) { return Result(std::move(value)); }

    /// Construct an error result
    static Result fail(QString error) { return Result(ErrorTag{}, std::move(error)); }

    /// True when the result holds a value
    explicit operator bool() const { return m_hasValue; }
    bool hasValue() const { return m_hasValue; }

    /// Access the value (undefined if !hasValue())
    const T &value() const & { return m_value; }
    T &&value() && { return std::move(m_value); }
    const T &operator*() const & { return m_value; }
    T &&operator*() && { return std::move(m_value); }
    const T *operator->() const { return &m_value; }

    /// Access the error message (empty if hasValue())
    const QString &error() const { return m_error; }

    /// Return the value or a fallback
    T valueOr(T fallback) const { return m_hasValue ? m_value : std::move(fallback); }

private:
    explicit Result(T value)
        : m_value(std::move(value)), m_hasValue(true) {}
    explicit Result(ErrorTag, QString error)
        : m_error(std::move(error)), m_hasValue(false) {}

    T m_value{};
    QString m_error;
    bool m_hasValue = false;
};

} // namespace Remus
