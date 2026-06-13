#pragma once
#include <string>
#include <vector>

struct CardMetadata {
    std::string id;
    std::string name;
    std::string number;
    std::string description;
    std::string creator;
};

extern std::vector<CardMetadata> g_card_registry;
void register_all_cards();
