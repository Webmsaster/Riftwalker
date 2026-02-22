#pragma once
#include <cstdint>
#include <cstddef>

class Entity;

class Component {
public:
    virtual ~Component() = default;
    virtual void init() {}
    virtual void update(float dt) {}

    Entity* entity = nullptr;
    bool active = true;
};

// Component type ID system
using ComponentTypeID = std::size_t;

inline ComponentTypeID getNextComponentTypeID() {
    static ComponentTypeID lastID = 0;
    return lastID++;
}

template <typename T>
inline ComponentTypeID getComponentTypeID() {
    static ComponentTypeID typeID = getNextComponentTypeID();
    return typeID;
}

constexpr std::size_t MAX_COMPONENTS = 32;
