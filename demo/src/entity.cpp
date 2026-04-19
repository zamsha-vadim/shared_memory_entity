#include "entity.h"

Entity::Entity(uint64_t id, const char* name) : id_{id}, name_{name} {}

auto Entity::GetId() const noexcept -> uint64_t
{
    return id_;
}

auto Entity::GetName() const noexcept -> const sme::mdm::string&
{
    return name_;
}

void Entity::SetName(const char* name)
{
    name_ = name;
}

auto Entity::GetProperties() const noexcept -> const sme::mdm::deque<Property>&
{
    return props_;
}

auto Entity::GetProperties() noexcept -> sme::mdm::deque<Property>&
{
    return props_;
}
