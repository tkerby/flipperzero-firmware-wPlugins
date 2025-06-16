#pragma once
#include "dynamic_map.hpp"
#include <memory>

inline std::unique_ptr<DynamicMap> mapsFirst()
{
    auto map = std::make_unique<DynamicMap>("First", 64, 57);

    // OUTER BORDER - Full perimeter walls
    map->addHorizontalWall(0, 63, 0, TILE_WALL);  // Top border
    map->addHorizontalWall(0, 63, 56, TILE_WALL); // Bottom border
    map->addVerticalWall(0, 0, 56, TILE_WALL);    // Left border
    map->addVerticalWall(63, 0, 56, TILE_WALL);   // Right border

    // TOP SECTION
    map->addHorizontalWall(1, 28, 1, TILE_WALL);  // Top section wall
    map->addHorizontalWall(40, 62, 1, TILE_WALL); // Right side of top

    // UPPER LEFT ROOMS AND CORRIDORS
    // Main upper left room structure
    map->addRoom(6, 2, 25, 6, true); // Upper left room
    map->addDoor(26, 4);             // Door to connect areas

    // Upper middle section
    map->addRoom(30, 2, 45, 8, true); // Upper middle room
    map->addCorridor(25, 4, 30, 4);   // Connect upper rooms

    // MIDDLE SECTION
    // Large central area
    map->addRoom(1, 10, 30, 25, true);

    // Add internal walls and structures in middle section
    map->addVerticalWall(15, 12, 18, TILE_WALL);  // Internal divider
    map->addHorizontalWall(8, 22, 15, TILE_WALL); // Horizontal divider

    // RIGHT SIDE - Major wall structure
    map->addVerticalWall(45, 8, 30, TILE_WALL);  // Major right wall
    map->addVerticalWall(50, 10, 25, TILE_WALL); // Secondary right wall

    // LOWER LEFT - Complex room system
    map->addRoom(1, 25, 15, 35, true);  // Lower left room
    map->addRoom(18, 25, 30, 35, true); // Lower middle room
    map->addCorridor(15, 30, 18, 30);   // Connect lower rooms

    // LOWER SECTION - Final room structures
    map->addRoom(5, 40, 25, 50, true);  // Lower main room
    map->addRoom(30, 40, 55, 50, true); // Lower right room
    map->addCorridor(25, 45, 30, 45);   // Connect lower sections

    // Add some doors in strategic locations
    map->addDoor(13, 20); // Door in middle section
    map->addDoor(22, 30); // Door in lower connection
    map->addDoor(40, 25); // Door in right section

    // BOTTOM SECTION - Final area
    map->addHorizontalWall(10, 50, 52, TILE_WALL); // Bottom section wall

    return map;
}

inline std::unique_ptr<DynamicMap> mapsSecond()
{
    auto map = std::make_unique<DynamicMap>("Second", 64, 57);

    // Outer walls
    map->addHorizontalWall(0, 63, 0, TILE_WALL);
    map->addHorizontalWall(0, 63, 56, TILE_WALL);
    map->addVerticalWall(0, 0, 56, TILE_WALL);
    map->addVerticalWall(63, 0, 56, TILE_WALL);

    // Main starting area (upper left)
    map->addRoom(5, 5, 20, 15, true);
    map->addDoor(20, 10); // Exit door

    // Central corridor system
    map->addCorridor(20, 10, 35, 10); // Horizontal main corridor
    map->addCorridor(35, 10, 35, 25); // Vertical connector

    // Upper right room
    map->addRoom(40, 5, 55, 18, true);
    map->addDoor(40, 12); // Entry door

    // Central hub area
    map->addRoom(30, 20, 45, 35, true);
    map->addDoor(35, 20); // North door
    map->addDoor(30, 27); // West door
    map->addDoor(45, 27); // East door

    // Lower left area
    map->addRoom(5, 25, 25, 40, true);
    map->addCorridor(25, 30, 30, 30); // Connect to central hub

    // Lower right area
    map->addRoom(50, 25, 60, 40, true);
    map->addCorridor(45, 30, 50, 30); // Connect to central hub

    // Final bottom area
    map->addRoom(20, 45, 40, 52, true);
    map->addCorridor(32, 40, 32, 45); // Connect to final area

    // Add some internal structures for complexity
    map->addVerticalWall(12, 8, 12, TILE_WALL);    // Internal wall in starting room
    map->addHorizontalWall(42, 48, 30, TILE_WALL); // Internal wall in central area

    return map;
}

inline std::unique_ptr<DynamicMap> mapsTutorial(uint8_t width = 10, uint8_t height = 10)
{
    return std::make_unique<DynamicMap>("Tutorial", width, height);
}
