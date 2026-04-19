#ifndef SME_DEMO_ENTITY_H
#define SME_DEMO_ENTITY_H

#include <sme/mdm/deque.h>
#include <sme/mdm/string.h>

class Entity {
    public:
     struct Property {
         sme::mdm::string name;
         uint64_t value;
     };

   public:
    Entity(uint64_t id, const char* name);

    auto GetId() const noexcept -> uint64_t;

    auto GetName() const noexcept -> const sme::mdm::string&;
    void SetName(const char* name);

    auto GetProperties() const noexcept -> const sme::mdm::deque<Property>&;
    auto GetProperties() noexcept -> sme::mdm::deque<Property>&;

   private:
    uint64_t id_{};
    sme::mdm::string name_;
    sme::mdm::deque<Property> props_;
};

#endif // SME_DEMO_ENTITY_H
