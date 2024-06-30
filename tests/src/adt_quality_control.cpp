#include <gtest/gtest.h>

#include "simulate_vector.h"
#include "compare_almost_equal.h"
#include "tatami/tatami.hpp"
#include "adt_quality_control.hpp"

class AdtQualityControlMetricsTest : public ::testing::Test {
protected:
    inline static std::shared_ptr<tatami::NumericMatrix> mat;
    inline static std::vector<std::vector<size_t> > subs;

    static void SetUpTestSuite() {
        size_t nr = 87, nc = 199;
        mat.reset(new tatami::DenseRowMatrix<double, int>(nr, nc, simulate_sparse_vector<double>(nr * nc, 0.05, /* lower = */ 1, /* upper = */ 10, /* seed = */ 222)));
        subs.push_back({ 0, 5, 7, 8, 9, 10, 16, 17, 51, 69, 72 });
    }
};

TEST_F(AdtQualityControlMetricsTest, NoSubset) {
    scran::adt_quality_control::MetricsOptions opts;
    auto res = scran::adt_quality_control::compute_metrics(mat.get(), {}, opts);
    EXPECT_EQ(res.sum, tatami_stats::sums::by_column(mat.get()));

    auto nonzeros = tatami_stats::counts::zero::by_column(mat.get());
    for (auto& nz : nonzeros) {
        nz = mat->nrow() - nz;
    }
    EXPECT_EQ(res.detected, nonzeros);

    EXPECT_TRUE(res.subset_sum.empty());
}

TEST_F(AdtQualityControlMetricsTest, OneSubset) {
    scran::adt_quality_control::MetricsOptions opts;
    auto res = scran::adt_quality_control::compute_metrics(mat.get(), subs, opts);

    auto submat = tatami::make_DelayedSubset(mat, subs[0], true);
    auto subsums = tatami_stats::sums::by_column(submat.get());
    EXPECT_EQ(res.subset_sum[0], subsums);
}

TEST(AdtQualityControlFilters, Basic) {
    scran::adt_quality_control::MetricsResults<double, int> results;
    results.detected = std::vector<int> { 10, 0, 15, 20, 14, 21 };
    results.subset_sum.push_back({ 50, 10, 20, 30, 40, 1000 });

    scran::adt_quality_control::FiltersOptions opt;
    auto thresholds = scran::adt_quality_control::compute_filters(results, opt);
    EXPECT_GT(thresholds.get_detected(), 0);
    EXPECT_GT(thresholds.get_subset_sum()[0], 100);

    auto keep = thresholds.filter(results);
    std::vector<uint8_t> expected { 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(expected, keep);
}

TEST(AdtQualityControlFilters, MinDrop) {
    scran::adt_quality_control::MetricsResults<double, int> results;
    results.detected = std::vector<int> { 20, 19, 20, 20, 20, 20 };
    results.subset_sum.push_back({ 50, 10, 20, 30, 40, 10 });

    // Respects the minimum difference.
    scran::adt_quality_control::FiltersOptions opt;
    auto thresholds = scran::adt_quality_control::compute_filters(results, opt);
    EXPECT_FLOAT_EQ(thresholds.get_detected(), 18);

    auto keep = thresholds.filter(results);
    std::vector<uint8_t> expected { 1, 1, 1, 1, 1, 1 };
    EXPECT_EQ(expected, keep);

    // Disabling the drop.
    opt.detected_min_drop = 0;
    thresholds = scran::adt_quality_control::compute_filters(results, opt);
    EXPECT_FLOAT_EQ(thresholds.get_detected(), 20);

    keep = thresholds.filter(results);
    expected = std::vector<uint8_t>{ 1, 0, 1, 1, 1, 1 };
    EXPECT_EQ(expected, keep);
}

TEST(AdtQualityControlFilters, Blocked) {
    {
        scran::adt_quality_control::MetricsResults<double, int> results;
        results.detected = std::vector<int> { 70, 0, 80, 90, 75, 85 };
        results.subset_sum.push_back({ 10, 20, 100, 5, 10, 15 });

        scran::adt_quality_control::FiltersOptions opt;
        auto thresholds = scran::adt_quality_control::compute_filters(results, opt);

        std::vector<int> block(6);
        auto bthresholds = scran::adt_quality_control::compute_filters_blocked(results, block.data(), opt);
        EXPECT_EQ(thresholds.get_detected(), bthresholds.get_detected()[0]);
        EXPECT_EQ(thresholds.get_subset_sum()[0], bthresholds.get_subset_sum()[0][0]);

        auto keep = bthresholds.filter(results, block.data());
        std::vector<uint8_t> expected { 1, 0, 0, 1, 1, 1 };
        EXPECT_EQ(expected, keep);
    }

    {
        scran::adt_quality_control::MetricsResults<double, int> results;
        results.detected = std::vector<int> { 100, 0, 100, 100, 400, 50, 50, 50 };
        results.subset_sum.push_back({ 10, 20, 100, 15, 90, 95, 80, 10 });

        scran::adt_quality_control::FiltersOptions opt;
        std::vector<int> block { 0, 0, 0, 0, 1, 1, 1, 1 };
        auto bthresholds = scran::adt_quality_control::compute_filters_blocked(results, block.data(), opt);

        EXPECT_GT(bthresholds.get_detected()[0], 50);
        EXPECT_LT(bthresholds.get_subset_sum()[0][0], 100);

        EXPECT_LT(bthresholds.get_detected()[1], 50);
        EXPECT_GT(bthresholds.get_subset_sum()[0][1], 100);

        auto keep = bthresholds.filter(results, block.data());
        std::vector<uint8_t> expected { 1, 0, 0, 1, 1, 1, 1, 1 };
        EXPECT_EQ(expected, keep);
    }
}
