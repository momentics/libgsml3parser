#include <gtest/gtest.h>
#include "gsml3parser/parser_config.h"
#include "gsml3parser/l3message.h"

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
