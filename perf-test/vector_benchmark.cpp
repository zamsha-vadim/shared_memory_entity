#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <sstream>
#include <string>
#include <vector>

#include "sme/basic_allocator.h"
#include "sme/mdm/vector.h"
#include "sme/mem_domain.h"
#include "sme/mem_space.h"

#define CATCH_AND_EXIT \
    catch (const std::exception& e) { \
        std::cerr << "FAILED: " << __builtin_FUNCTION() << ": " << e.what() << std::endl; \
        std::exit(1); \
    }

namespace {

using Clock = std::chrono::high_resolution_clock;

constexpr int kIterations = 1'000;
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
    std::vector<int> v;
    double r = measure([&]() { op(v); }, n);
    return r;
}

template <typename F, typename G>
double measure_std(F&& op, G&& refill, int n) {
    std::vector<int> v;
    refill(v);
    return measure([&]() { op(v); }, [&]() { refill(v); }, n);
}

template <typename F>
double measure_pmr(F&& op, int n) {
    std::pmr::vector<int> v{std::pmr::new_delete_resource()};
    double r = measure([&]() { op(v); }, n);
    return r;
}

template <typename F, typename G>
double measure_pmr(F&& op, G&& refill, int n) {
    std::pmr::vector<int> v{std::pmr::new_delete_resource()};
    refill(v);
    return measure([&]() { op(v); }, [&]() { refill(v); }, n);
}

template <typename F>
double measure_sme(F&& op, int n) {
    MdmFixture fix;
    sme::mdm::vector<int> v;
    double r = measure([&]() { op(v); }, n);
    return r;
}

template <typename F, typename G>
double measure_sme(F&& op, G&& refill, int n) {
    MdmFixture fix;
    sme::mdm::vector<int> v;
    refill(v);
    return measure([&]() { op(v); }, [&]() { refill(v); }, n);
}

template <typename F>
double measure_sme_explicit(F&& op, int n) {
    MdmFixtureNoCtx fix;
    sme::mdm::MemoryDomainAllocator<int> alloc{*fix.domain};
    sme::mdm::vector<int> v{alloc};
    double r = measure([&]() { op(v); }, n);
    return r;
}

template <typename F, typename G>
double measure_sme_explicit(F&& op, G&& refill, int n) {
    MdmFixtureNoCtx fix;
    sme::mdm::MemoryDomainAllocator<int> alloc{*fix.domain};
    sme::mdm::vector<int> v{alloc};
    refill(v);
    return measure([&]() { op(v); }, [&]() { refill(v); }, n);
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

    std::cout << "\n# Vector Performance Benchmark\n\n";
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

void MeasureFillSmall(std::vector<Result>& results) try {
    auto op = [](auto& v) {
        v.assign(10, 42);
    };

    const double std_ns = measure_std(op, kIterations);
    const double pmr_ns = measure_pmr(op, kIterations);
    const double mdm_ns = measure_sme(op, kIterations);
    const double mdm_exp_ns = measure_sme_explicit(op, kIterations);

    results.push_back({"Fill small (10)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureFillLarge(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 10;

    auto op = [](auto& v) {
        v.assign(10000, 42);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"Fill large (10k)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureCopyConstruct(std::vector<Result>& results) try {
        constexpr int kIter = kIterations / 4;

    const auto src = []() {
        std::vector<int> v;
        v.reserve(1000);
        for (int i = 0; i < 1000; ++i)
            v.push_back(i);
        return v;
    }();

    const double std_ns = measure(
        [&]() { std::vector<int> v(src); }, kIter);

    std::pmr::vector<int> pmr_src{src.begin(), src.end(),
                                   std::pmr::new_delete_resource()};
    const double pmr_ns = measure(
        [&]() { std::pmr::vector<int> v(pmr_src); }, kIter);

    MdmFixture fix;
    sme::mdm::vector<int> mdm_src(src.begin(), src.end());
    const double mdm_ns = measure(
        [&]() { sme::mdm::vector<int> v(mdm_src); }, kIter);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::vector<int> mdm_src_exp(src.begin(), src.end(), alloc_exp);
    const double mdm_exp_ns = measure(
        [&]() { sme::mdm::vector<int> v(mdm_src_exp); }, kIter);

    results.push_back({"Copy construct", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureMoveConstruct(std::vector<Result>& results) try {
        constexpr int kIter = kIterations / 4;

    auto make_src = []() {
        std::vector<int> v;
        v.reserve(1000);
        for (int i = 0; i < 1000; ++i)
            v.push_back(i);
        return v;
    };

    const double std_ns = measure(
        [&]() { std::vector<int> v(make_src()); }, kIter);
    const double pmr_ns = measure(
        [&]() {
            auto v = std::pmr::vector<int>(std::pmr::new_delete_resource());
            for (int i = 0; i < 1000; ++i)
                v.push_back(i);
            std::pmr::vector<int> dst(std::move(v));
        },
        kIter);

    MdmFixture fix;
    auto make_mdm_src = []() {
        sme::mdm::vector<int> v;
        for (int i = 0; i < 1000; ++i)
            v.push_back(i);
        return v;
    };

    const double mdm_ns = measure(
        [&]() { sme::mdm::vector<int> v(make_mdm_src()); },
        kIter);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    auto make_mdm_src_exp = [&alloc_exp]() {
        sme::mdm::vector<int> v{alloc_exp};
        for (int i = 0; i < 1000; ++i)
            v.push_back(i);
        return v;
    };
    const double mdm_exp_ns = measure(
        [&]() { sme::mdm::vector<int> v(make_mdm_src_exp(), alloc_exp); },
        kIter);

    results.push_back({"Move construct", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasurePushBack(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 100;

    auto op = [](auto& v) {
        v.reserve(kN);
        for (int i = 0; i < kN; ++i)
            v.push_back(i);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"push_back x100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureEmplaceBack(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 100;

    auto op = [](auto& v) {
        v.reserve(kN);
        for (int i = 0; i < kN; ++i)
            v.emplace_back(i);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"emplace_back x100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasurePopBack(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 100;

    auto refill = [](auto& v) {
        for (int i = 0; i < kN; ++i)
            v.push_back(i);
    };
    auto op = [](auto& v) {
        for (int i = 0; i < kN; ++i)
            v.pop_back();
    };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);
    const double mdm_ns = measure_sme(op, refill, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, refill, kIter);

    results.push_back({"pop_back x100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureInsert(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 10;
    constexpr int kN = 500;

    auto refill = [](auto& v) {
        v.clear();
        v.reserve(kN + 50);
        for (int i = 0; i < kN; ++i)
            v.push_back(i);
    };
    auto op = [](auto& v) {
        v.insert(v.begin() + kN / 2, 999);
    };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);
    const double mdm_ns = measure_sme(op, refill, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, refill, kIter);

    results.push_back({"insert middle", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureErase(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 10;
    constexpr int kN = 500;

    auto refill = [](auto& v) {
        v.clear();
        for (int i = 0; i < kN; ++i)
            v.push_back(i);
    };
    auto op = [](auto& v) {
        v.erase(v.begin() + kN / 2);
    };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);
    const double mdm_ns = measure_sme(op, refill, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, refill, kIter);

    results.push_back({"erase middle", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureResize(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;

    auto op = [](auto& v) {
        v.resize(1000, 42);
        v.resize(100);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"resize 1000->100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureAssign(std::vector<Result>& results) try {
        constexpr int kIter = kIterations / 8;
    constexpr int kN = 1000;

    const auto src = []() {
        std::vector<int> v;
        for (int i = 0; i < kN; ++i)
            v.push_back(i);
        return v;
    }();

    std::vector<int> std_dst;
    const double std_ns = measure(
        [&]() { std_dst = src; }, kIter);
    std::pmr::vector<int> pmr_dst{std::pmr::new_delete_resource()};
    const double pmr_ns = measure(
        [&]() {
            std::pmr::vector<int> tmp(src.begin(), src.end(), std::pmr::new_delete_resource());
            pmr_dst = tmp;
        },
        kIter);

    MdmFixture fix;
    sme::mdm::vector<int> mdm_dst;
    const double mdm_ns = measure(
        [&]() { mdm_dst.assign(src.begin(), src.end()); },
        kIter);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::vector<int> mdm_dst_exp{alloc_exp};
    const double mdm_exp_ns = measure(
        [&]() { mdm_dst_exp.assign(src.begin(), src.end()); },
        kIter);

    results.push_back({"assign (1k)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureClear(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    constexpr int kN = 1000;

    auto op = [](auto& v) {
        for (int i = 0; i < kN; ++i)
            v.push_back(i);
        v.clear();
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
        std::vector<int> v(5000, 42);
        return v;
    }();

        std::pmr::vector<int> content_pmr{std::pmr::new_delete_resource()};
    content_pmr.resize(5000, 42);

    MdmFixture fix;
    sme::mdm::vector<int> content_mdm;
    content_mdm.resize(5000, 42);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::vector<int> content_exp{alloc_exp};
    content_exp.resize(5000, 42);

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
        std::vector<int> v(5000, 42);
        return v;
    }();

        std::pmr::vector<int> content_pmr{std::pmr::new_delete_resource()};
    content_pmr.resize(5000, 42);

    MdmFixture fix;
    sme::mdm::vector<int> content_mdm;
    content_mdm.resize(5000, 42);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::vector<int> content_exp{alloc_exp};
    content_exp.resize(5000, 42);

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
        std::vector<int> v(5000, 42);
        return v;
    }();

        std::pmr::vector<int> content_pmr{std::pmr::new_delete_resource()};
    content_pmr.resize(5000, 42);

    MdmFixture fix;
    sme::mdm::vector<int> content_mdm;
    content_mdm.resize(5000, 42);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::vector<int> content_exp{alloc_exp};
    content_exp.resize(5000, 42);

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
        std::vector<int> v(5000, 42);
        return v;
    }();

        std::pmr::vector<int> content_pmr{std::pmr::new_delete_resource()};
    content_pmr.resize(5000, 42);

    MdmFixture fix;
    sme::mdm::vector<int> content_mdm;
    content_mdm.resize(5000, 42);

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<int> alloc_exp{*fix_exp.domain};
    sme::mdm::vector<int> content_exp{alloc_exp};
    content_exp.resize(5000, 42);

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

void MeasureReserve(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;

    auto op = [](auto& v) {
        v.reserve(10000);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);
    const double mdm_ns = measure_sme(op, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"reserve (10k)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureShrinkToFit(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 10;
    constexpr int kN = 10000;

    auto refill = [](auto& v) {
        v.clear();
        v.reserve(kN * 2);
        for (int i = 0; i < kN; ++i)
            v.push_back(i);
    };
    auto op = [](auto& v) {
        v.shrink_to_fit();
    };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);
    const double mdm_ns = measure_sme(op, refill, kIter);
    const double mdm_exp_ns = measure_sme_explicit(op, refill, kIter);

    results.push_back({"shrink_to_fit", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

}  // namespace

int main() {
    std::vector<Result> results;

    MeasureDefaultConstruct(results);
    MeasureFillSmall(results);
    MeasureFillLarge(results);
    MeasureCopyConstruct(results);
    MeasureMoveConstruct(results);
    MeasurePushBack(results);
    MeasureEmplaceBack(results);
    MeasurePopBack(results);
    MeasureInsert(results);
    MeasureErase(results);
    MeasureResize(results);
    MeasureAssign(results);
    MeasureClear(results);
    MeasureOperatorBracketRead(results);
    MeasureAt(results);
    MeasureRangeFor(results);
    MeasureSize(results);
    MeasureReserve(results);
    MeasureShrinkToFit(results);

    PrintTable(results);
    return 0;
}
