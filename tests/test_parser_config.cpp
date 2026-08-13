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
#include "gsml3parser/parser_config.h"

using namespace gsml3parser;

TEST(ParserConfigTest, DefaultConstruction) {
    ParserConfig cfg;
    EXPECT_EQ(cfg.getLogLevel(), LogLevel::WARNING);
    EXPECT_FALSE(cfg.getPDHandler(L3PD::RadioResource));
    EXPECT_FALSE(cfg.getPDHandler(L3PD::CallControl));
}

TEST(ParserConfigTest, WithLogLevel) {
    ParserConfig cfg;
    auto cfg2 = cfg.withLogLevel(LogLevel::DEBUG);
    EXPECT_EQ(cfg.getLogLevel(), LogLevel::WARNING);
    EXPECT_EQ(cfg2.getLogLevel(), LogLevel::DEBUG);
    auto cfg3 = cfg2.withLogLevel(LogLevel::INFO);
    EXPECT_EQ(cfg3.getLogLevel(), LogLevel::INFO);
}

TEST(ParserConfigTest, WithPDHandler) {
    ParserConfig cfg;
    auto handler = [](const L3Frame&) -> std::unique_ptr<L3Message> {
        return nullptr;
    };
    auto cfg2 = cfg.withPDHandler(L3PD::RadioResource, handler);
    EXPECT_TRUE(cfg2.getPDHandler(L3PD::RadioResource));
    EXPECT_FALSE(cfg.getPDHandler(L3PD::RadioResource));
    EXPECT_FALSE(cfg2.getPDHandler(L3PD::MobilityManagement));
}

TEST(ParserConfigTest, Chain) {
    ParserConfig cfg;
    auto h1 = [](const L3Frame&) -> std::unique_ptr<L3Message> { return nullptr; };
    auto h2 = [](const L3Frame&) -> std::unique_ptr<L3Message> { return nullptr; };

    auto finalCfg = cfg
        .withLogLevel(LogLevel::DEBUG)
        .withPDHandler(L3PD::RadioResource, h1)
        .withPDHandler(L3PD::CallControl, h2);

    EXPECT_EQ(finalCfg.getLogLevel(), LogLevel::DEBUG);
    EXPECT_TRUE(finalCfg.getPDHandler(L3PD::RadioResource));
    EXPECT_TRUE(finalCfg.getPDHandler(L3PD::CallControl));
    EXPECT_FALSE(finalCfg.getPDHandler(L3PD::MobilityManagement));
}
