#pragma once

#include <string>

struct ap_item_t
{
    std::string classname;
    int spawnflags;
    int ep; // If classname is a key
    int map; // If classname is a key
};
