// SPDX-FileCopyrightText: 2017-2023 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

#include <ginkgo/core/stop/combined.hpp>


#include <thread>
#include <vector>


#include <gtest/gtest.h>


#include <ginkgo/core/stop/iteration.hpp>
#include <ginkgo/core/stop/time.hpp>


namespace {


constexpr gko::size_type test_iterations = 10;
constexpr int test_seconds = 999;  // we will never converge through seconds
constexpr double eps = 1.0e-4;
using double_seconds = std::chrono::duration<double>;


class Combined : public ::testing::Test {
protected:
    Combined()
    {
        exec_ = gko::ReferenceExecutor::create();
        factory_ =
            gko::stop::Combined::build()
                .with_criteria(
                    gko::stop::Iteration::build()
                        .with_max_iters(test_iterations)
                        .on(exec_),
                    gko::stop::Time::build()
                        .with_time_limit(std::chrono::seconds(test_seconds))
                        .on(exec_))
                .on(exec_);
    }

    std::unique_ptr<gko::stop::Combined::Factory> factory_;
    std::shared_ptr<const gko::Executor> exec_;
};


TEST_F(Combined, CanCreateFactory)
{
    ASSERT_NE(factory_, nullptr);
    ASSERT_EQ(factory_->get_parameters().criteria.size(), 2);
}


TEST_F(Combined, CanCreateCriterion)
{
    auto criterion = factory_->generate(nullptr, nullptr, nullptr);
    ASSERT_NE(criterion, nullptr);
}


TEST_F(Combined, CanIgnoreNullptr)
{
    auto combined = gko::stop::Combined::build()
                        .with_criteria(gko::stop::Iteration::build()
                                           .with_max_iters(test_iterations)
                                           .on(exec_),
                                       nullptr)
                        .on(exec_);

    ASSERT_NO_THROW(combined->generate(nullptr, nullptr, nullptr));
}


TEST_F(Combined, CanThrowAllNullptr)
{
    auto combined =
        gko::stop::Combined::build().with_criteria(nullptr, nullptr).on(exec_);

    ASSERT_THROW(combined->generate(nullptr, nullptr, nullptr),
                 gko::NotSupported);
}


TEST_F(Combined, CanThrowWithoutInput)
{
    auto combined = gko::stop::Combined::build().on(exec_);

    ASSERT_THROW(combined->generate(nullptr, nullptr, nullptr),
                 gko::NotSupported);
}


TEST_F(Combined, FunctionCanThrowWithoutInput)
{
    std::vector<std::shared_ptr<const gko::stop::CriterionFactory>>
        criterion_vec{};

    ASSERT_THROW(gko::stop::combine(criterion_vec), gko::NotSupported);
}


TEST_F(Combined, FunctionCanThrowOnlyOneNullptr)
{
    std::vector<std::shared_ptr<const gko::stop::CriterionFactory>>
        criterion_vec{nullptr};

    ASSERT_THROW(gko::stop::combine(criterion_vec), gko::NotSupported);
}


TEST_F(Combined, FunctionCanThrowAllNullptr)
{
    std::vector<std::shared_ptr<const gko::stop::CriterionFactory>>
        criterion_vec{nullptr, nullptr};

    ASSERT_THROW(gko::stop::combine(criterion_vec), gko::NotSupported);
}


TEST_F(Combined, FunctionCanThrowFirstIsInvalid)
{
    auto stop = gko::share(gko::stop::Iteration::build()
                               .with_max_iters(test_iterations)
                               .on(exec_));
    std::vector<std::shared_ptr<const gko::stop::CriterionFactory>>
        criterion_vec{nullptr, stop};

    ASSERT_THROW(gko::stop::combine(criterion_vec), gko::NotSupported);
}


TEST_F(Combined, FunctionCanIgnoreNullptr)
{
    auto stop = gko::share(gko::stop::Iteration::build()
                               .with_max_iters(test_iterations)
                               .on(exec_));
    std::vector<std::shared_ptr<const gko::stop::CriterionFactory>>
        criterion_vec{stop, nullptr};
    auto combined = gko::stop::combine(criterion_vec);

    ASSERT_NO_THROW(combined->generate(nullptr, nullptr, nullptr));
}


}  // namespace
