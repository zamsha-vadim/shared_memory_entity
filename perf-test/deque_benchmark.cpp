#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <sstream>
#include <string>
#include <vector>

#include "sme/basic_allocator.h"
#include "sme/mdm/deque.h"
#include "sme/mem_domain.h"
#include "sme/mem_space.h"

#define CATCH_AND_EXIT \
    catch (const std::exception& e) { \
        std::cerr << "FAILED: " << __builtin_FUNCTION() << ": " << e.what() << std::endl; \
        std::exit(1); \
    }

namespace {

using Clock = std::chrono::high_resolution_clock;

constexpr int kIterations = 10000;
constexpr size_t kMdmBufSize = 256ULL * 1024 * 1024;
struct MdmFixture {
    std::unique_ptr<char[]> buffer{new char[kMdmBufSize]};
    sme::MemorySpace mem_space;
    sme::Pointer<sme::MemoryDomain> domain_ptr;
    sme::MemoryDomain* domain;
    sme::AllocationContext<sme::MemoryDomain> ctx;

    MdmFixture()
        : mem_space(sme::Pointer<void>(buffer.get()), kMdmBufSize),
          domain_ptr(sme::CreateMemoryDomain(
              mem_space, sme::SynchronizationType::kNone)),
          domain(domain_ptr.GetAddress()),
          ctx(*domain) {}
};

struct MdmFixtureNoCtx {
    std::unique_ptr<char[]> buffer{new char[kMdmBufSize]};
    sme::MemorySpace mem_space;
    sme::Pointer<sme::MemoryDomain> domain_ptr;
    sme::MemoryDomain* domain;

    MdmFixtureNoCtx()
        : mem_space(sme::Pointer<void>(buffer.get()), kMdmBufSize),
          domain_ptr(sme::CreateMemoryDomain(
              mem_space, sme::SynchronizationType::kNone)),
          domain(domain_ptr.GetAddress()) {}
};

struct Result {
    const char* name;
    double std_ns;
    double pmr_ns;
    double mdm_ns;
    double mdm_exp_ns;
};

template <typename F>
double measure(F&& f, int n) {
    auto start = Clock::now();
    for (int i = 0; i < n; ++i)
        f();
    auto end = Clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count() / n;
}

template <typename F, typename G>
double measure(F&& op, G&& refill, int n) {
    double total = 0;
    for (int i = 0; i < n; ++i) {
        auto t0 = Clock::now();
        op();
        auto t1 = Clock::now();
        total += std::chrono::duration<double, std::nano>(t1 - t0).count();
        refill();
    }
    return total / n;
}

// ---- Container helpers (creates + runs operation) ----

template <typename F>
double measure_std(F&& op, int n) {
    std::deque<int> d;
    double r = measure([&]() { op(d); }, n);
    return r;
}

template <typename F, typename G>
double measure_std(F&& op, G&& refill, int n) {
    std::deque<int> d;
    refill(d);
    return measure([&]() { op(d); }, [&]() { refill(d); }, n);
}

template <typename F>
double measure_pmr(F&& op, int n) {
    std::pmr::deque<int> d{std::pmr::new_delete_resource()};
    double r = measure([&]() { op(d); }, n);
    return r;
}

template <typename F, typename G>
double measure_pmr(F&& op, G&& refill, int n) {
    std::pmr::deque<int> d{std::pmr::new_delete_resource()};
    refill(d);
    return measure([&]() { op(d); }, [&]() { refill(d); }, n);
}

template <typename F>
double measure_sme(F&& op, int n) {
    MdmFixture fix;
    sme::mdm::deque<int> d;
    double r = measure([&]() { op(d); }, n);
    return r;
}

template <typename F, typename G>
double measure_sme(F&& op, G&& refill, int n) {
    MdmFixture fix;
    sme::mdm::deque<int> d;
    refill(d);
    return measure([&]() { op(d); }, [&]() { refill(d); }, n);
}

template <typename F>
double measure_sme_explicit(F&& op, int n) {
    MdmFixtureNoCtx fix;
    sme::mdm::MemoryDomainAllocator<int> alloc{*fix.domain};
    sme::mdm::deque<int> d{alloc};
    double r = measure([&]() { op(d); }, n);
    return r;
}

template <typename F, typename G>
double measure_sme_explicit(F&& op, G&& refill, int n) {
    MdmFixtureNoCtx fix;
    sme::mdm::MemoryDomainAllocator<int> alloc{*fix.domain};
    sme::mdm::deque<int> d{alloc};
    refill(d);
    return measure([&]() { op(d); }, [&]() { refill(d); }, n);
}

std::string FmtNs(double ns) {
    std::ostringstream os;
    if (ns < 10.0)
        os << std::fixed << std::setprecision(2) << ns;
    else if (ns < 100.0)
        os << std::fixed << std::setprecision(2) << ns;
    else if (ns < 10000.0)
        os << std::fixed << std::setprecision(1) << ns;
    else
        os << static_cast<int>(ns);
    return os.str();
}

void PrintTable(const std::vector<Result>& results) {
    size_t w_name = 8;
    size_t w_std = 8;
    size_t w_pmr = 8;
    size_t w_mdm = 8;
    size_t w_mdm_exp = 8;
    size_t w_r1 = 7;
    size_t w_r2 = 7;
    size_t w_r3 = 11;
    size_t w_r4 = 11;

    for (const auto& r : results) {
        w_name = std::max(w_name, std::char_traits<char>::length(r.name));
        w_std = std::max(w_std, FmtNs(r.std_ns).size() + 3);
        w_pmr = std::max(w_pmr, FmtNs(r.pmr_ns).size() + 3);
        w_mdm = std::max(w_mdm, FmtNs(r.mdm_ns).size() + 3);
        w_mdm_exp = std::max(w_mdm_exp, FmtNs(r.mdm_exp_ns).size() + 3);
        std::ostringstream rs;
        rs << std::fixed << std::setprecision(2) << (r.mdm_ns / r.std_ns) << 'x';
        w_r1 = std::max(w_r1, rs.str().size());
        rs.str("");
        rs << std::fixed << std::setprecision(2) << (r.mdm_ns / r.pmr_ns) << 'x';
        w_r2 = std::max(w_r2, rs.str().size());
        rs.str("");
        rs << std::fixed << std::setprecision(2) << (r.mdm_exp_ns / r.mdm_ns) << 'x';
        w_r3 = std::max(w_r3, rs.str().size());
        rs.str("");
        rs << std::fixed << std::setprecision(2) << (r.mdm_exp_ns / r.pmr_ns) << 'x';
        w_r4 = std::max(w_r4, rs.str().size());
    }

    auto sep = std::string(w_name + w_std + w_pmr + w_mdm + w_mdm_exp + w_r1 + w_r2 + w_r3 + w_r4 + 27, '-');

    std::cout << "\n# Deque Performance Benchmark\n\n";
    std::cout << "+-" << sep << "-+\n";
    std::cout << "| " << std::left << std::setw(w_name) << "Operation"
              << " | " << std::right << std::setw(w_std) << "std"
              << " | " << std::right << std::setw(w_pmr) << "pmr"
              << " | " << std::right << std::setw(w_mdm) << "sme"
               << " | " << std::right << std::setw(w_mdm_exp) << "sme_exp"
               << " || " << std::right << std::setw(w_r1) << "sme/std"
              << " | " << std::right << std::setw(w_r2) << "sme/pmr"
              << " | " << std::right << std::setw(w_r3) << "sme_exp/sme"
              << " | " << std::right << std::setw(w_r4) << "sme_exp/pmr"
              << " |\n";
    std::cout << "+-" << sep << "-+\n";

    for (const auto& r : results) {
        double ratio_std = r.mdm_ns / r.std_ns;
        double ratio_pmr = r.mdm_ns / r.pmr_ns;
        double ratio_exp = r.mdm_exp_ns / r.mdm_ns;
        double ratio_exp_pmr = r.mdm_exp_ns / r.pmr_ns;
        std::ostringstream rs1, rs2, rs3, rs4;
        rs1 << std::fixed << std::setprecision(2) << ratio_std << 'x';
        rs2 << std::fixed << std::setprecision(2) << ratio_pmr << 'x';
        rs3 << std::fixed << std::setprecision(2) << ratio_exp << 'x';
        rs4 << std::fixed << std::setprecision(2) << ratio_exp_pmr << 'x';
        std::cout << "| " << std::left << std::setw(w_name) << r.name
                  << " | " << std::right << std::setw(w_std) << (FmtNs(r.std_ns) + " ns")
                  << " | " << std::right << std::setw(w_pmr) << (FmtNs(r.pmr_ns) + " ns")
                  << " | " << std::right << std::setw(w_mdm) << (FmtNs(r.mdm_ns) + " ns")
                   << " | " << std::right << std::setw(w_mdm_exp) << (FmtNs(r.mdm_exp_ns) + " ns")
                   << " || " << std::right << std::setw(w_r1) << rs1.str()
                  << " | " << std::right << std::setw(w_r2) << rs2.str()
                  << " | " << std::right << std::setw(w_r3) << rs3.str()
                  << " | " << std::right << std::setw(w_r4) << rs4.str()
                  << " |\n";
    }

    std::cout << "+-" << sep << "-+\n";
}

// ---- Benchmark functions ----

void MeasureDefaultConstruct(std::vector<Result>& results) try {
    auto op = [](auto&) {};

    const double std_ns = measure_std(op, kIterations);
    const double pmr_ns = measure_pmr(op, kIterations);
    const double mdm_ns = measure_sme(op, kIterations);
    const double mdm_exp_ns = measure_sme_explicit(op, kIterations);

    results.push_back({"Default construct", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasurePushBackSmall(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 100;

    auto op = [](auto& d) {
        for (int i = 0; i < kN; ++i)
            d.push_back(i);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"push_back x100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasurePushBackLarge(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 40;
    constexpr int kN = 10000;

    auto op = [](auto& d) {
        for (int i = 0; i < kN; ++i)
            d.push_back(i);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"push_back x10k", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasurePushFront(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 100;

    auto op = [](auto& d) {
        for (int i = 0; i < kN; ++i)
            d.push_front(i);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"push_front x100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasurePopFront(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 100;

    auto refill = [](auto& d) {
        for (int i = 0; i < kN; ++i)
            d.push_back(i);
    };
    auto op = [](auto& d) {
        for (int i = 0; i < kN; ++i)
            d.pop_front();
    };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);
    const double mdm_ns = measure_sme(op, refill, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, refill, kIter);

    results.push_back({"pop_front x100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureEmplaceBack(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 100;

    auto op = [](auto& d) {
        for (int i = 0; i < kN; ++i)
            d.emplace_back(i);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"emplace_back x100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureInsertMiddle(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 10;
    constexpr int kN = 500;

    auto refill = [](auto& d) {
        d.clear();
        for (int i = 0; i < kN; ++i)
            d.push_back(i);
    };
    auto op = [](auto& d) {
        d.insert(d.begin() + kN / 2, 999);
    };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);
    const double mdm_ns = measure_sme(op, refill, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, refill, kIter);

    results.push_back({"insert middle", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureEraseMiddle(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 10;
    constexpr int kN = 500;

    auto refill = [](auto& d) {
        d.clear();
        for (int i = 0; i < kN; ++i)
            d.push_back(i);
    };
    auto op = [](auto& d) {
        d.erase(d.begin() + kN / 2);
    };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);
    const double mdm_ns = measure_sme(op, refill, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, refill, kIter);

    results.push_back({"erase middle", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureClear(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 1000;

    auto op = [](auto& d) {
        for (int i = 0; i < kN; ++i)
            d.push_back(i);
        d.clear();
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"fill+clear (1k)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureOperatorBracketRead(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 20;

    const auto content = []() {
        std::deque<int> d;
        for (int i = 0; i < 5000; ++i)
            d.push_back(42);
        return d;
    }();

        std::pmr::deque<int> content_pmr{std::pmr::new_delete_resource()};
    for (int i = 0; i < 5000; ++i)
        content_pmr.push_back(42);

    MdmFixture fix;
    sme::mdm::deque<int> content_mdm;
    for (int i = 0; i < 5000; ++i)
        content_mdm.push_back(42);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::deque<int> content_exp{alloc_exp};
    for (int i = 0; i < 5000; ++i)
        content_exp.push_back(42);

    const double std_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (size_t i = 0; i < content.size(); ++i)
                sum += content[i];
        },
        kIter);

    const double pmr_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (size_t i = 0; i < content_pmr.size(); ++i)
                sum += content_pmr[i];
        },
        kIter);

    const double mdm_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (size_t i = 0; i < content_mdm.size(); ++i)
                sum += content_mdm[i];
        },
        kIter);

    const double mdm_exp_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (size_t i = 0; i < content_exp.size(); ++i)
                sum += content_exp[i];
        },
        kIter);

    results.push_back({"operator[] read", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureAt(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 20;

    const auto content = []() {
        std::deque<int> d;
        for (int i = 0; i < 5000; ++i)
            d.push_back(42);
        return d;
    }();

        std::pmr::deque<int> content_pmr{std::pmr::new_delete_resource()};
    for (int i = 0; i < 5000; ++i)
        content_pmr.push_back(42);

    MdmFixture fix;
    sme::mdm::deque<int> content_mdm;
    for (int i = 0; i < 5000; ++i)
        content_mdm.push_back(42);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::deque<int> content_exp{alloc_exp};
    for (int i = 0; i < 5000; ++i)
        content_exp.push_back(42);

    const double std_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (size_t i = 0; i < content.size(); ++i)
                sum += content.at(i);
        },
        kIter);

    const double pmr_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (size_t i = 0; i < content_pmr.size(); ++i)
                sum += content_pmr.at(i);
        },
        kIter);

    const double mdm_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (size_t i = 0; i < content_mdm.size(); ++i)
                sum += content_mdm.at(i);
        },
        kIter);

    const double mdm_exp_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (size_t i = 0; i < content_exp.size(); ++i)
                sum += content_exp.at(i);
        },
        kIter);

    results.push_back({"at() read", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureRangeFor(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 20;

    const auto content = []() {
        std::deque<int> d;
        for (int i = 0; i < 5000; ++i)
            d.push_back(42);
        return d;
    }();

        std::pmr::deque<int> content_pmr{std::pmr::new_delete_resource()};
    for (int i = 0; i < 5000; ++i)
        content_pmr.push_back(42);

    MdmFixture fix;
    sme::mdm::deque<int> content_mdm;
    for (int i = 0; i < 5000; ++i)
        content_mdm.push_back(42);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::deque<int> content_exp{alloc_exp};
    for (int i = 0; i < 5000; ++i)
        content_exp.push_back(42);

    const double std_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (auto x : content)
                sum += x;
        },
        kIter);

    const double pmr_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (auto x : content_pmr)
                sum += x;
        },
        kIter);

    const double mdm_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (auto x : content_mdm)
                sum += x;
        },
        kIter);

    const double mdm_exp_ns = measure(
        [&]() {
            [[maybe_unused]] volatile int sum = 0;
            for (auto x : content_exp)
                sum += x;
        },
        kIter);

    results.push_back({"range-for", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureSize(std::vector<Result>& results) try {
    const auto content = []() {
        std::deque<int> d;
        for (int i = 0; i < 5000; ++i)
            d.push_back(42);
        return d;
    }();

        std::pmr::deque<int> content_pmr{std::pmr::new_delete_resource()};
    for (int i = 0; i < 5000; ++i)
        content_pmr.push_back(42);

    MdmFixture fix;
    sme::mdm::deque<int> content_mdm;
    for (int i = 0; i < 5000; ++i)
        content_mdm.push_back(42);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::deque<int> content_exp{alloc_exp};
    for (int i = 0; i < 5000; ++i)
        content_exp.push_back(42);

    const double std_ns = measure(
        [&]() { volatile auto sz = content.size(); (void)sz; },
        kIterations);

    const double pmr_ns = measure(
        [&]() { volatile auto sz = content_pmr.size(); (void)sz; },
        kIterations);

    const double mdm_ns = measure(
        [&]() { volatile auto sz = content_mdm.size(); (void)sz; },
        kIterations);

    const double mdm_exp_ns = measure(
        [&]() { volatile auto sz = content_exp.size(); (void)sz; },
        kIterations);

    results.push_back({"size()", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

}  // namespace

int main() {
    std::vector<Result> results;

    MeasureDefaultConstruct(results);
    MeasurePushBackSmall(results);
    MeasurePushBackLarge(results);
    MeasurePushFront(results);
    MeasurePopFront(results);
    MeasureEmplaceBack(results);
    MeasureInsertMiddle(results);
    MeasureEraseMiddle(results);
    MeasureClear(results);
    MeasureOperatorBracketRead(results);
    MeasureAt(results);
    MeasureRangeFor(results);
    MeasureSize(results);

    PrintTable(results);
    return 0;
}
