#ifndef SIMPLE_OBJ_H
#define SIMPLE_OBJ_H

#include <algorithm>

#include <sme/mdm/string.h>
// #include <sme/mdm/vector.h>
#include <sme/mdm/deque.h>
#include <sme/mem_ptr.h>

template <typename T>
class SimpleObject {
   public:
    using ValueType = T;

    using MeasureContainerType = sme::mdm::deque<sme::mdm::string>;

   public:
    template <typename MeasureContainer>
    SimpleObject(const char* name,
                 const MeasureContainer& measures,
                 sme::MemoryDomain& mem_domain);

    auto GetName() const noexcept -> const sme::mdm::string&;
    void SetName(const char* name);

    auto GetValue() const noexcept -> const ValueType&;
    void SetValue(const ValueType& value);

    auto GetMeasure() const -> const sme::mdm::string&;
    void SetMeasure(const char* measure);

    auto GetAcceptableMeasures() const noexcept -> const MeasureContainerType&;

   private:
    sme::MemoryDomain& mem_domain_;

    sme::mdm::string name_;
    ValueType value_{};

    MeasureContainerType accept_measures_;
    sme::Pointer<const sme::mdm::string> measure_;
};

using IntObject = SimpleObject<int>;
using DoubleObject = SimpleObject<double>;
using StringObject = SimpleObject<sme::mdm::string>;

template <typename T>
template <typename MeasureContainerT>
SimpleObject<T>::SimpleObject(const char* name,
                              const MeasureContainerT& measures,
                              sme::MemoryDomain& mem_domain)
    : mem_domain_{mem_domain},
      name_{name, sme::mdm::ItemAllocator<sme::mdm::string>{mem_domain_}},
      accept_measures_{sme::mdm::ItemAllocator<MeasureContainerType>(mem_domain_)}
{
    for (const char* measure : measures) {
        sme::mdm::string value{measure,
                               sme::mdm::ItemAllocator<sme::mdm::string>{mem_domain_}};

        accept_measures_.emplace_back(value);
    }
}

template <typename T>
auto SimpleObject<T>::GetName() const noexcept -> const sme::mdm::string&
{
    return name_;
}

template <typename T>
void SimpleObject<T>::SetName(const char* name)
{
    name_ = name;
}

template <typename T>
auto SimpleObject<T>::GetValue() const noexcept -> const ValueType&
{
    return value_;
}

template <typename T>
void SimpleObject<T>::SetValue(const ValueType& value)
{
    value_ = value;
}

template <typename T>
auto SimpleObject<T>::GetMeasure() const -> const sme::mdm::string&
{
    if (measure_ == nullptr)
        throw std::logic_error("Measure not defined");
    return *measure_;
}

template <typename T>
void SimpleObject<T>::SetMeasure(const char* value)
{
    sme::mdm::string measure{value,
                             sme::mdm::ItemAllocator<sme::mdm::string>{mem_domain_}};

    auto found_item =
        std::find(accept_measures_.cbegin(), accept_measures_.cend(), measure);
    if (found_item == accept_measures_.cend())
        throw std::logic_error("Invalid measure");

    measure_ = &(*found_item);
}

template <typename T>
auto SimpleObject<T>::GetAcceptableMeasures() const noexcept
    -> const MeasureContainerType&
{
    return accept_measures_;
}

#endif  // SIMPLE_OBJ_H
