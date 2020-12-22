/**
 * @file Result.h
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-14
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#pragma once

#include <string>

template <typename T>
struct CommonResult
{
    /**
     * @brief Did this operation result in an error?
     */
    bool IsError = false;

    /**
     * @brief A message explaining the error that occurred.
     */
    std::string ErrorMessage = "";
};

template <typename T>
struct Result : public CommonResult<T>
{
    /**
     * @brief Generate a Result object indicating success and returning a value.
     */
    static Result<T> Success(T value)
    {
        auto r = Result<T>();
        r.Value = value;
        r.IsError = false;
        r.ErrorMessage = "";
        return r;
    }

    /**
     * @brief Generate a Result object indicating failure with an optional message.
     */
    static Result<T> Error(const std::string& message = "")
    {
        auto r = Result<T>();
        r.Value = {};
        r.IsError = true;
        r.ErrorMessage = message;
        return r;
    }

    /**
     * @brief Value returned by this operation on success
     */
    T Value;
};

template <>
struct Result<void> : public CommonResult<void>
{
    /**
     * @brief Generate a Result object indicating success.
     */
    static Result<void> Success()
    {
        auto r = Result<void>();
        r.IsError = false;
        r.ErrorMessage = "";
        return r;
    }

    /**
     * @brief Generate a Result object indicating failure with an optional message.
     */
    static Result<void> Error(const std::string& message = "")
    {
        auto r = Result<void>();
        r.IsError = true;
        r.ErrorMessage = message;
        return r;
    }
};