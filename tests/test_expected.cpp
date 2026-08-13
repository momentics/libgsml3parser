// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <gtest/gtest.h>
#include "gsml3parser/expected.h"

using namespace gsml3parser;

TEST(ExpectedTest, HoldValue) {
    auto exp = Expected<int>::hold(42);
    EXPECT_TRUE(exp.has_value());
    EXPECT_EQ(exp.value(), 42);
}

TEST(ExpectedTest, HoldError) {
    auto exp = Expected<int>::error(ParseError{ParseError::Code::InvalidValue, "bad"});
    EXPECT_FALSE(exp.has_value());
    EXPECT_EQ(exp.error().code, ParseError::Code::InvalidValue);
    EXPECT_EQ(exp.error().message, "bad");
}

TEST(ExpectedTest, HasValue) {
    auto good = Expected<int>::hold(10);
    auto bad = Expected<int>::error(ParseError{ParseError::Code::TruncatedInput, "short"});
    EXPECT_TRUE(good.has_value());
    EXPECT_FALSE(bad.has_value());
}

TEST(ExpectedTest, ValueAccess) {
    auto exp = Expected<int>::hold(99);
    EXPECT_EQ(exp.value(), 99);
    const auto& cexp = exp;
    EXPECT_EQ(cexp.value(), 99);
}

TEST(ExpectedTest, OperatorStar) {
    auto exp = Expected<int>::hold(7);
    EXPECT_EQ(*exp, 7);
}

TEST(ExpectedTest, BoolConversion) {
    auto good = Expected<int>::hold(1);
    auto bad = Expected<int>::error(ParseError{ParseError::Code::InvalidPD, "err"});
    EXPECT_TRUE(static_cast<bool>(good));
    EXPECT_FALSE(static_cast<bool>(bad));
}

TEST(ExpectedTest, MapTransformsValue) {
    auto exp = Expected<int>::hold(5);
    auto result = exp.map([](int v) { return v * 2; });
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 10);
}

TEST(ExpectedTest, MapPropagatesError) {
    auto err = ParseError{ParseError::Code::InvalidMTI, "mti bad"};
    auto exp = Expected<int>::error(err);
    auto result = exp.map([](int v) { return v + 1; });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ParseError::Code::InvalidMTI);
}

TEST(ExpectedTest, AndThenChains) {
    auto exp = Expected<int>::hold(3);
    auto result = exp.and_then([](int v) {
        return Expected<double>::hold(v * 2.5);
    });
    EXPECT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result.value(), 7.5);
}

TEST(ExpectedTest, AndThenPropagatesError) {
    auto err = ParseError{ParseError::Code::LengthMismatch, "len"};
    auto exp = Expected<int>::error(err);
    auto result = exp.and_then([](int v) {
        return Expected<double>::hold(v * 1.0);
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ParseError::Code::LengthMismatch);
}

TEST(ExpectedTest, ExpectedVoidSuccess) {
    auto exp = Expected<void>::hold();
    EXPECT_TRUE(exp.has_value());
    EXPECT_TRUE(static_cast<bool>(exp));
}

TEST(ExpectedTest, ExpectedVoidError) {
    auto err = ParseError{ParseError::Code::InvalidIE, "ie bad"};
    auto exp = Expected<void>::error(err);
    EXPECT_FALSE(exp.has_value());
    EXPECT_EQ(exp.error().code, ParseError::Code::InvalidIE);
}
