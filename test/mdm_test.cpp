#include <sys/wait.h>

#include <algorithm>
#include <array>
#include <deque>
#include <forward_list>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "sme/mapped_obj.h"
#include "sme/mdm/deque.h"
#include "sme/mdm/forward_list.h"
#include "sme/mdm/list.h"
#include "sme/mdm/map.h"
#include "sme/mdm/set.h"
#include "sme/mdm/string.h"
#include "sme/mdm/unordered_map.h"
#include "sme/mdm/unordered_set.h"
#include "sme/mdm/vector.h"
#include "sme/mem_domain.h"
#include "sme/msp/root_obj.h"
#include "test/mem_fault.h"
#include "test/shm_test_util.h"

namespace {

constexpr auto kSharedMemorySize = 1024 * 64U;

// Checker class

template <typename ControlDataT, typename SharedDataT>
class Checker {
   public:
    using ControlDataType = ControlDataT;
    using SharedDataType = SharedDataT;

    using ControlDataCreator = std::function<std::unique_ptr<ControlDataType>()>;
    using SharedDataCreator =
        std::function<sme::Pointer<SharedDataType>(const ControlDataType&,
                                                   sme::MemoryDomain&)>;
    using DataComparator =
        std::function<bool(const ControlDataType&, const SharedDataType&)>;

   public:
    Checker(ControlDataCreator ctrl_data_creator,
            SharedDataCreator shm_data_creator,
            DataComparator data_comparator);

    Checker(const Checker&) = delete;
    Checker(Checker&&) = delete;
    auto operator=(Checker&&) -> Checker& = delete;
    auto operator=(const Checker&) -> Checker& = delete;
    ~Checker() = default;

    void BuildSharedData(sme::MemoryMap& mem_map);
    auto CompareData(sme::MemoryMap& mem_map) const -> bool;

   private:
    using MemoryHeader =
        std::pair<sme::Pointer<sme::MemoryDomain>, sme::Pointer<SharedDataType>>;

    std::unique_ptr<ControlDataType> ctrl_data_;
    SharedDataCreator shm_data_creator_;
    DataComparator data_comparator_;
};

template <typename ControlDataType, typename SharedDataType>
Checker<ControlDataType, SharedDataType>::Checker(ControlDataCreator ctrl_data_creator,
                                                  SharedDataCreator shm_data_creator,
                                                  DataComparator data_comparator)
    : ctrl_data_(ctrl_data_creator()), shm_data_creator_(std::move(shm_data_creator)),
      data_comparator_(std::move(data_comparator))
{
}

template <typename ControlDataType, typename SharedDataType>
void Checker<ControlDataType, SharedDataType>::BuildSharedData(sme::MemoryMap& mem_map)
{
    auto* mem_space = sme::ConstructMemorySpace(mem_map, 0);
    auto* hdr_ptr = sme::msp::CreateRoot<MemoryHeader>(*mem_space);
    auto mem_domain =
        sme::CreateMemoryDomain(*mem_space, sme::Synchronizer::Type::kShared);

    sme::AllocationContext<sme::MemoryDomain> mdm_ctx{*mem_domain};

    auto shm_data = shm_data_creator_(*ctrl_data_, *mem_domain);

    hdr_ptr->first = mem_domain;
    hdr_ptr->second = shm_data;
}

template <typename ControlDataType, typename SharedDataType>
auto Checker<ControlDataType, SharedDataType>::CompareData(sme::MemoryMap& mem_map) const
    -> bool
{
    const auto& mem_space = sme::GetObject<sme::MemorySpace>(mem_map, 0);
    auto& hdr = sme::msp::GetRoot<MemoryHeader>(mem_space);

    sme::AllocationContext<sme::MemoryDomain> mdm_ctx{*(hdr.first)};

    const auto& shm_data = hdr.second;

    return data_comparator_(*ctrl_data_, *shm_data);
}

//-----------------------

enum class CheckResult : int { kNotEqual, kEqual, kNotSupported, kUndefined };

template <typename ControlDataT, typename SharedDataT>
auto Check(
    typename Checker<ControlDataT, SharedDataT>::ControlDataCreator ctrl_data_creator,
    typename Checker<ControlDataT, SharedDataT>::SharedDataCreator shm_data_creator,
    typename Checker<ControlDataT, SharedDataT>::DataComparator data_comparator)
    -> CheckResult
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSharedMemorySize);
    SharedMemoryFileDeleter smfd{smf};

    auto shm1 = smf.MapMemory(sme::kAllMemoryMapRequestForShared);
    auto shm2 = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    /*
    std::cout << "Memory1: " << shm1.GetAddress() << "-"
              << (void*)(static_cast<const char*>(shm1.GetAddress()) + shm1.GetSize())
              << std::endl;
    std::cout << "Memory2: " << shm2.GetAddress() << "-"
              << (void*)(static_cast<const char*>(shm2.GetAddress()) + shm2.GetSize())
              << std::endl;
    */

    Checker<ControlDataT, SharedDataT> checker{ctrl_data_creator, shm_data_creator,
                                               data_comparator};

    checker.BuildSharedData(shm2);

    const char* shm_addr_begin = static_cast<const char*>(shm2.GetAddress());
    const char* shm_addr_end = shm_addr_begin + shm2.GetSize();

    shm2.Unmap();

    auto pid = fork();
    if (pid == -1)
        throw std::system_error(errno, std::generic_category(),
                                "Can't fork test proccess");
    if (pid == 0) {
        sme::MemoryFaultCatcher mf_catcher{shm_addr_begin, shm_addr_end};

        bool res = checker.CompareData(shm1);
        exit(res == true ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    int wait_stat{};
    if (waitpid(pid, &wait_stat, 0) == -1)
        throw std::system_error(errno, std::generic_category(),
                                "Can't get test proccess exit status");
    if (WIFEXITED(wait_stat)) {
        switch (WEXITSTATUS(wait_stat)) {
            case EXIT_SUCCESS:
                return CheckResult::kEqual;
            case EXIT_FAILURE:
                return CheckResult::kNotEqual;
            case (128 + SIGSEGV):
            case (128 + SIGBUS):
                return CheckResult::kNotSupported;
            default:
                return CheckResult::kUndefined;
        }
    } else if (WIFSIGNALED(wait_stat)) {
        return CheckResult::kUndefined;
    } else {
        return CheckResult::kUndefined;
    }
}

//-----------------------

auto Compare(const std::string& lhs, const sme::mdm::string& rhs) -> bool
{
    if (lhs.size() != rhs.size())
        return false;
    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin());
}

#define APPLY_RESULT(res)                                              \
    switch (res) {                                                     \
        case CheckResult::kEqual:                                      \
            SUCCEED();                                                 \
            break;                                                     \
        case CheckResult::kNotEqual:                                   \
            FAIL();                                                    \
            break;                                                     \
        case CheckResult::kNotSupported:                               \
            GTEST_SKIP() << "NOTE: STL has incomplete implementation"; \
            break;                                                     \
        case CheckResult::kUndefined:                                  \
        default:                                                       \
            FAIL() << "Unexpected error";                              \
            break;                                                     \
    }

//-----------------------

const char* kSomeChars =
    "_00000000001111111111222222222233333333334444444444"
    "55555555556666666666777777777788888888889999999999_";

}  // namespace

TEST(MdmTest, TestString)
{
    auto ctrl_data_creator = []() -> std::unique_ptr<std::string> {
        return std::make_unique<std::string>(kSomeChars);
    };

    auto shm_data_creator =
        [](const std::string& ctrl_data,
           sme::MemoryDomain& mem_domain) -> sme::Pointer<sme::mdm::string> {
        auto* obj = sme::Create<sme::mdm::string>(mem_domain, ctrl_data.c_str());
        return {obj};
    };

    auto data_comparator = [](const std::string& s1, const sme::mdm::string& s2) -> bool {
        return Compare(s1, s2);
    };

    auto res = Check<std::string, sme::mdm::string>(ctrl_data_creator, shm_data_creator,
                                                    data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestVector)
{
    using ControlVector = std::vector<std::string>;
    using SharedVector = sme::mdm::vector<sme::mdm::string>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlVector> {
        auto vector = std::make_unique<ControlVector>(20);

        for (auto i = 0u; i < vector->size(); i++) {
            auto s = std::to_string(i) + kSomeChars + std::to_string(i);
            (*vector)[i] = s.c_str();
        }

        return vector;
    };

    auto shm_data_creator =
        [](const ControlVector& ctrl_vector,
           sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedVector> {
        auto* shm_vector = sme::Create<SharedVector>(mem_domain);

        shm_vector->reserve(ctrl_vector.size());

        for (const auto& s : ctrl_vector)
            shm_vector->push_back(s.c_str());

        return {shm_vector};
    };

    auto data_comparator = [](const ControlVector& vector1,
                              const SharedVector& vector2) -> bool {
        return std::equal(vector1.cbegin(), vector1.cend(), vector2.cbegin(),
                          [](const auto& s1, const auto& s2) { return Compare(s1, s2); });
    };

    auto res = Check<ControlVector, SharedVector>(ctrl_data_creator, shm_data_creator,
                                                  data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestDeque)
{
    using ControlDeque = std::deque<std::string>;
    using SharedDeque = sme::mdm::deque<sme::mdm::string>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlDeque> {
        auto vector = std::make_unique<ControlDeque>(20);

        for (auto i = 0u; i < vector->size(); i++) {
            auto s = std::to_string(i) + kSomeChars + std::to_string(i);
            (*vector)[i] = s.c_str();
        }

        return vector;
    };

    auto shm_data_creator =
        [](const ControlDeque& ctrl_vector,
           sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedDeque> {
        auto* shm_deque = sme::Create<SharedDeque>(mem_domain);

        for (const auto& s : ctrl_vector)
            shm_deque->push_back(s.c_str());

        return {shm_deque};
    };

    auto data_comparator = [](const ControlDeque& vector1,
                              const SharedDeque& vector2) -> bool {
        return std::equal(vector1.cbegin(), vector1.cend(), vector2.cbegin(),
                          [](const auto& s1, const auto& s2) { return Compare(s1, s2); });
    };

    auto res = Check<ControlDeque, SharedDeque>(ctrl_data_creator, shm_data_creator,
                                                data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestList)
{
    using ControlList = std::list<std::string>;
    using SharedList = sme::mdm::list<sme::mdm::string>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlList> {
        auto list = std::make_unique<ControlList>();

        for (int i = 0; i < 20; i++) {
            auto s = std::to_string(i) + kSomeChars + std::to_string(i);
            list->push_back(s);
        }

        return list;
    };

    auto shm_data_creator =
        [](const ControlList& ctrl_list,
           sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedList> {
        auto* shm_list = sme::Create<SharedList>(mem_domain);

        for (const auto& s : ctrl_list) {
            shm_list->push_back({s.c_str()});
        }

        return {shm_list};
    };

    auto data_comparator = [](const ControlList& list1, const SharedList& list2) -> bool {
        return std::equal(list1.cbegin(), list1.cend(), list2.cbegin(),
                          [](const auto& s1, const auto& s2) { return Compare(s1, s2); });
    };

    auto res = Check<ControlList, SharedList>(ctrl_data_creator, shm_data_creator,
                                              data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestForwardList)
{
    using ControlList = std::forward_list<std::string>;
    using SharedList = sme::mdm::forward_list<sme::mdm::string>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlList> {
        auto list = std::make_unique<ControlList>();

        for (int i = 0; i < 20; i++) {
            auto s = std::to_string(i) + kSomeChars + std::to_string(i);
            list->push_front(s);
        }

        return list;
    };

    auto shm_data_creator =
        [](const ControlList& ctrl_list,
           sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedList> {
        auto* shm_list = sme::Create<SharedList>(mem_domain);

        for (const auto& s : ctrl_list)
            shm_list->push_front({s.c_str()});
        shm_list->reverse();

        return {shm_list};
    };

    auto data_comparator = [](const ControlList& list1, const SharedList& list2) -> bool {
        return std::equal(list1.cbegin(), list1.cend(), list2.cbegin(),
                          [](const auto& s1, const auto& s2) { return Compare(s1, s2); });
    };

    auto res = Check<ControlList, SharedList>(ctrl_data_creator, shm_data_creator,
                                              data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestIntMap)
{
    using ControlMap = std::map<int, int>;
    using SharedMap = sme::mdm::map<int, int>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlMap> {
        auto map = std::make_unique<ControlMap>();

        for (int i = 0; i < 300; i++) {
            auto key = i;
            auto value = i + 1000;

            map->insert({key, value});
        }

        return map;
    };

    auto shm_data_creator = [](const ControlMap& ctrl_map,
                               sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedMap> {
        auto* shm_map = sme::Create<SharedMap>(mem_domain);

        for (const auto& item : ctrl_map) {
            auto key{item.first};
            auto value{item.second};

            shm_map->insert({key, value});
        }

        return {shm_map};
    };

    auto data_comparator = [](const ControlMap& map1, const SharedMap& map2) -> bool {
        return std::equal(map1.cbegin(), map1.cend(), map2.cbegin(),
                          [](const auto& item1, const auto& item2) {
                              return (item1.first == item2.first) &&
                                     (item1.second == item2.second);
                          });
    };

    auto res = Check<ControlMap, SharedMap>(ctrl_data_creator, shm_data_creator,
                                            data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestStringMap)
{
    using ControlMap = std::map<std::string, std::string>;
    using SharedMap = sme::mdm::map<sme::mdm::string, sme::mdm::string>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlMap> {
        auto map = std::make_unique<ControlMap>();

        for (int i = 0; i < 200; i++) {
            auto key = std::to_string(i);
            auto value = std::to_string(i) + kSomeChars + std::to_string(i + 1);

            map->insert({key, value});
        }

        return map;
    };

    auto shm_data_creator = [](const ControlMap& ctrl_map,
                               sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedMap> {
        auto* shm_map = sme::Create<SharedMap>(mem_domain);

        for (const auto& item : ctrl_map) {
            sme::mdm::string key{item.first.c_str()};
            sme::mdm::string value{item.second.c_str()};

            shm_map->insert({key, value});
        }

        return {shm_map};
    };

    auto data_comparator = [](const ControlMap& map1, const SharedMap& map2) -> bool {
        return std::equal(map1.cbegin(), map1.cend(), map2.cbegin(),
                          [](const auto& item1, const auto& item2) {
                              return Compare(item1.first, item2.first) &&
                                     Compare(item1.second, item2.second);
                          });
    };

    auto res = Check<ControlMap, SharedMap>(ctrl_data_creator, shm_data_creator,
                                            data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestIntUnorderedMap)
{
    using ControlMap = std::unordered_map<int, int>;
    using SharedMap = sme::mdm::unordered_map<int, int>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlMap> {
        auto map = std::make_unique<ControlMap>();

        for (int i = 0; i < 3; i++) {
            auto key = i;
            auto value = i + 1000;

            map->insert({key, value});
        }

        return map;
    };

    auto shm_data_creator = [](const ControlMap& ctrl_map,
                               sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedMap> {
        auto* shm_map = sme::Create<SharedMap>(mem_domain);

        for (const auto& item : ctrl_map) {
            auto key{item.first};
            auto value{item.second};

            shm_map->insert({key, value});
        }

        return {shm_map};
    };

    auto data_comparator = [](const ControlMap& map1, const SharedMap& map2) -> bool {
        if (map1.size() != map2.size())
            return false;

        for (const auto& item : map1) {
            auto key{item.first};

            auto found_iter = map2.find(key);
            if (found_iter == map2.cend())
                return false;

            auto value{item.second};
            if (found_iter->second != value)
                return false;
        }

        return true;
    };

    auto res = Check<ControlMap, SharedMap>(ctrl_data_creator, shm_data_creator,
                                            data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestStringUnorderedMap)
{
    using ControlMap = std::unordered_map<std::string, std::string>;
    using SharedMap = sme::mdm::unordered_map<sme::mdm::string, sme::mdm::string>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlMap> {
        auto map = std::make_unique<ControlMap>();

        for (int i = 0; i < 200; i++) {
            auto key = std::to_string(i);
            auto value = std::to_string(i) + kSomeChars + std::to_string(i + 1);

            map->insert({key, value});
        }

        return map;
    };

    auto shm_data_creator = [](const ControlMap& ctrl_map,
                               sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedMap> {
        auto* shm_map = sme::Create<SharedMap>(mem_domain);

        for (const auto& item : ctrl_map) {
            sme::mdm::string key{item.first.c_str()};
            sme::mdm::string value{item.second.c_str()};

            shm_map->insert({key, value});
        }

        return {shm_map};
    };

    auto data_comparator = [](const ControlMap& map1, const SharedMap& map2) -> bool {
        if (map1.size() != map2.size())
            return false;

        for (const auto& item : map1) {
            sme::mdm::string key{item.first.c_str()};

            auto found_iter = map2.find(key);
            if (found_iter == map2.cend())
                return false;

            sme::mdm::string value{item.second.c_str()};
            if (found_iter->second != value)
                return false;
        }

        return true;
    };

    auto res = Check<ControlMap, SharedMap>(ctrl_data_creator, shm_data_creator,
                                            data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestIntSet)
{
    using ControlSet = std::set<int>;
    using SharedSet = sme::mdm::set<int>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlSet> {
        auto set = std::make_unique<ControlSet>();

        for (int i = 0; i < 300; i++) {
            auto key = i;

            set->insert(key);
        }

        return set;
    };

    auto shm_data_creator = [](const ControlSet& ctrl_set,
                               sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedSet> {
        auto* shm_set = sme::Create<SharedSet>(mem_domain);

        for (const auto& item : ctrl_set) {
            int key{item};
            shm_set->insert(key);
        }

        return {shm_set};
    };

    auto data_comparator = [](const ControlSet& set1, const SharedSet& set2) -> bool {
        return std::equal(
            set1.cbegin(), set1.cend(), set2.cbegin(),
            [](const auto& item1, const auto& item2) { return item1 == item2; });
    };

    auto res = Check<ControlSet, SharedSet>(ctrl_data_creator, shm_data_creator,
                                            data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestStringSet)
{
    using ControlSet = std::set<std::string>;
    using SharedSet = sme::mdm::set<sme::mdm::string>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlSet> {
        auto set = std::make_unique<ControlSet>();

        for (int i = 0; i < 300; i++) {
            auto key = std::to_string(i);
            set->insert(key);
        }

        return set;
    };

    auto shm_data_creator = [](const ControlSet& ctrl_set,
                               sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedSet> {
        auto* shm_set = sme::Create<SharedSet>(mem_domain);

        for (const auto& item : ctrl_set) {
            sme::mdm::string key{item.c_str()};

            shm_set->insert(key);
        }

        return {shm_set};
    };

    auto data_comparator = [](const ControlSet& set1, const SharedSet& set2) -> bool {
        return std::equal(
            set1.cbegin(), set1.cend(), set2.cbegin(),
            [](const auto& item1, const auto& item2) { return Compare(item1, item2); });
    };

    auto res = Check<ControlSet, SharedSet>(ctrl_data_creator, shm_data_creator,
                                            data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestIntUnorderedSet)
{
    using ControlSet = std::unordered_set<int>;
    using SharedSet = sme::mdm::unordered_set<int>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlSet> {
        auto set = std::make_unique<ControlSet>();

        for (int i = 0; i < 10; i++) {
            auto key = i;

            set->insert(key);
        }

        return set;
    };

    auto shm_data_creator = [](const ControlSet& ctrl_set,
                               sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedSet> {
        auto* shm_set = sme::Create<SharedSet>(mem_domain);

        for (const auto& item : ctrl_set) {
            int key{item};

            shm_set->insert(key);
        }

        return {shm_set};
    };

    auto data_comparator = [](const ControlSet& set1, const SharedSet& set2) -> bool {
        if (set1.size() != set2.size())
            return false;

        for (const auto& item : set1) {
            if (set2.find(item) == set2.cend())
                return false;
        }

        return true;
    };

    auto res = Check<ControlSet, SharedSet>(ctrl_data_creator, shm_data_creator,
                                            data_comparator);
    APPLY_RESULT(res);
}

TEST(MdmTest, TestStringUnorderedSet)
{
    using ControlSet = std::unordered_set<std::string>;
    using SharedSet = sme::mdm::unordered_set<sme::mdm::string>;

    auto ctrl_data_creator = []() -> std::unique_ptr<ControlSet> {
        auto set = std::make_unique<ControlSet>();

        for (int i = 0; i < 10; i++) {
            auto value = std::to_string(i) + kSomeChars + std::to_string(i + 1);

            set->insert(value);
        }

        return set;
    };

    auto shm_data_creator = [](const ControlSet& ctrl_set,
                               sme::MemoryDomain& mem_domain) -> sme::Pointer<SharedSet> {
        auto* shm_set = sme::Create<SharedSet>(mem_domain);

        for (const auto& item : ctrl_set) {
            sme::mdm::string value{item.c_str()};

            shm_set->insert(value);
        }

        return {shm_set};
    };

    auto data_comparator = [](const ControlSet& set1, const SharedSet& set2) -> bool {
        if (set1.size() != set2.size())
            return false;

        for (const auto& item : set1) {
            sme::mdm::string value{item.c_str()};

            if (set2.find(value) == set2.cend())
                return false;
        }

        return true;
    };

    auto res = Check<ControlSet, SharedSet>(ctrl_data_creator, shm_data_creator,
                                            data_comparator);
    APPLY_RESULT(res);
}
