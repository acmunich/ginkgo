// SPDX-FileCopyrightText: 2017-2023 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

#include <ginkgo/core/log/record.hpp>


#include <gtest/gtest.h>


#include <ginkgo/core/base/executor.hpp>
#include <ginkgo/core/base/utils.hpp>
#include <ginkgo/core/solver/bicgstab.hpp>
#include <ginkgo/core/stop/iteration.hpp>


#include "core/test/utils/assertions.hpp"


namespace {


constexpr int num_iters = 10;
const std::string apply_str = "Dummy::apply";


TEST(Record, CanGetData)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::iteration_complete_mask);

    ASSERT_EQ(logger->get().allocation_started.size(), 0);
}


TEST(Record, CatchesAllocationStarted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::allocation_started_mask);

    logger->on<gko::log::Logger::allocation_started>(exec.get(), 42);

    auto& data = logger->get().allocation_started.back();
    ASSERT_EQ(data->exec, exec.get());
    ASSERT_EQ(data->num_bytes, 42);
    ASSERT_EQ(data->location, 0);
}


TEST(Record, CatchesAllocationCompleted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::allocation_completed_mask);
    int dummy = 1;
    auto ptr = reinterpret_cast<gko::uintptr>(&dummy);

    logger->on<gko::log::Logger::allocation_completed>(exec.get(), 42, ptr);

    auto& data = logger->get().allocation_completed.back();
    ASSERT_EQ(data->exec, exec.get());
    ASSERT_EQ(data->num_bytes, 42);
    ASSERT_EQ(data->location, ptr);
}


TEST(Record, CatchesFreeStarted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(gko::log::Logger::free_started_mask);
    int dummy = 1;
    auto ptr = reinterpret_cast<gko::uintptr>(&dummy);

    logger->on<gko::log::Logger::free_started>(exec.get(), ptr);

    auto& data = logger->get().free_started.back();
    ASSERT_EQ(data->exec, exec.get());
    ASSERT_EQ(data->num_bytes, 0);
    ASSERT_EQ(data->location, ptr);
}


TEST(Record, CatchesFreeCompleted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::free_completed_mask);
    int dummy = 1;
    auto ptr = reinterpret_cast<gko::uintptr>(&dummy);

    logger->on<gko::log::Logger::free_completed>(exec.get(), ptr);

    auto& data = logger->get().free_completed.back();
    ASSERT_EQ(data->exec, exec.get());
    ASSERT_EQ(data->num_bytes, 0);
    ASSERT_EQ(data->location, ptr);
}


TEST(Record, CatchesCopyStarted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(gko::log::Logger::copy_started_mask);
    int dummy_from = 1;
    int dummy_to = 1;
    auto ptr_from = reinterpret_cast<gko::uintptr>(&dummy_from);
    auto ptr_to = reinterpret_cast<gko::uintptr>(&dummy_to);

    logger->on<gko::log::Logger::copy_started>(exec.get(), exec.get(), ptr_from,
                                               ptr_to, 42);

    auto& data = logger->get().copy_started.back();
    auto data_from = std::get<0>(*data);
    auto data_to = std::get<1>(*data);
    ASSERT_EQ(data_from.exec, exec.get());
    ASSERT_EQ(data_from.num_bytes, 42);
    ASSERT_EQ(data_from.location, ptr_from);
    ASSERT_EQ(data_to.exec, exec.get());
    ASSERT_EQ(data_to.num_bytes, 42);
    ASSERT_EQ(data_to.location, ptr_to);
}


TEST(Record, CatchesCopyCompleted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::copy_completed_mask);
    int dummy_from = 1;
    int dummy_to = 1;
    auto ptr_from = reinterpret_cast<gko::uintptr>(&dummy_from);
    auto ptr_to = reinterpret_cast<gko::uintptr>(&dummy_to);

    logger->on<gko::log::Logger::copy_completed>(exec.get(), exec.get(),
                                                 ptr_from, ptr_to, 42);

    auto& data = logger->get().copy_completed.back();
    auto data_from = std::get<0>(*data);
    auto data_to = std::get<1>(*data);
    ASSERT_EQ(data_from.exec, exec.get());
    ASSERT_EQ(data_from.num_bytes, 42);
    ASSERT_EQ(data_from.location, ptr_from);
    ASSERT_EQ(data_to.exec, exec.get());
    ASSERT_EQ(data_to.num_bytes, 42);
    ASSERT_EQ(data_to.location, ptr_to);
}


TEST(Record, CatchesOperationLaunched)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::operation_launched_mask);
    gko::Operation op;

    logger->on<gko::log::Logger::operation_launched>(exec.get(), &op);

    auto& data = logger->get().operation_launched.back();
    ASSERT_EQ(data->exec, exec.get());
    ASSERT_EQ(data->operation, &op);
}


TEST(Record, CatchesOperationCompleted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::operation_completed_mask);
    gko::Operation op;

    logger->on<gko::log::Logger::operation_completed>(exec.get(), &op);

    auto& data = logger->get().operation_completed.back();
    ASSERT_EQ(data->exec, exec.get());
    ASSERT_EQ(data->operation, &op);
}


TEST(Record, CatchesPolymorphicObjectCreateStarted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::polymorphic_object_create_started_mask);
    auto po = gko::matrix::Dense<>::create(exec);

    logger->on<gko::log::Logger::polymorphic_object_create_started>(exec.get(),
                                                                    po.get());


    auto& data = logger->get().polymorphic_object_create_started.back();
    ASSERT_EQ(data->exec, exec.get());
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->input.get()), po, 0);
    ASSERT_EQ(data->output.get(), nullptr);
}


TEST(Record, CatchesPolymorphicObjectCreateCompleted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::polymorphic_object_create_completed_mask);
    auto po = gko::matrix::Dense<>::create(exec);
    auto output = gko::matrix::Dense<>::create(exec);

    logger->on<gko::log::Logger::polymorphic_object_create_completed>(
        exec.get(), po.get(), output.get());

    auto& data = logger->get().polymorphic_object_create_completed.back();
    ASSERT_EQ(data->exec, exec.get());
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->input.get()), po, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->output.get()), output, 0);
}


TEST(Record, CatchesPolymorphicObjectCopyStarted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::polymorphic_object_copy_started_mask);
    auto from = gko::matrix::Dense<>::create(exec);
    auto to = gko::matrix::Dense<>::create(exec);

    logger->on<gko::log::Logger::polymorphic_object_copy_started>(
        exec.get(), from.get(), to.get());

    auto& data = logger->get().polymorphic_object_copy_started.back();
    ASSERT_EQ(data->exec, exec.get());
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->input.get()), from, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->output.get()), to, 0);
}


TEST(Record, CatchesPolymorphicObjectCopyCompleted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::polymorphic_object_copy_completed_mask);
    auto from = gko::matrix::Dense<>::create(exec);
    auto to = gko::matrix::Dense<>::create(exec);

    logger->on<gko::log::Logger::polymorphic_object_copy_completed>(
        exec.get(), from.get(), to.get());


    auto& data = logger->get().polymorphic_object_copy_completed.back();
    ASSERT_EQ(data->exec, exec.get());
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->input.get()), from, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->output.get()), to, 0);
}


TEST(Record, CatchesPolymorphicObjectMoveStarted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::polymorphic_object_move_started_mask);
    auto from = gko::matrix::Dense<>::create(exec);
    auto to = gko::matrix::Dense<>::create(exec);

    logger->on<gko::log::Logger::polymorphic_object_move_started>(
        exec.get(), from.get(), to.get());

    auto& data = logger->get().polymorphic_object_move_started.back();
    ASSERT_EQ(data->exec, exec.get());
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->input.get()), from, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->output.get()), to, 0);
}


TEST(Record, CatchesPolymorphicObjectMoveCompleted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::polymorphic_object_move_completed_mask);
    auto from = gko::matrix::Dense<>::create(exec);
    auto to = gko::matrix::Dense<>::create(exec);

    logger->on<gko::log::Logger::polymorphic_object_move_completed>(
        exec.get(), from.get(), to.get());


    auto& data = logger->get().polymorphic_object_move_completed.back();
    ASSERT_EQ(data->exec, exec.get());
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->input.get()), from, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->output.get()), to, 0);
}


TEST(Record, CatchesPolymorphicObjectDeleted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::polymorphic_object_deleted_mask);
    auto po = gko::matrix::Dense<>::create(exec);

    logger->on<gko::log::Logger::polymorphic_object_deleted>(exec.get(),
                                                             po.get());


    auto& data = logger->get().polymorphic_object_deleted.back();
    ASSERT_EQ(data->exec, exec.get());
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->input.get()), po, 0);
    ASSERT_EQ(data->output, nullptr);
}


TEST(Record, CatchesLinOpApplyStarted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::linop_apply_started_mask);
    auto A = gko::initialize<Dense>({1.1}, exec);
    auto b = gko::initialize<Dense>({-2.2}, exec);
    auto x = gko::initialize<Dense>({3.3}, exec);

    logger->on<gko::log::Logger::linop_apply_started>(A.get(), b.get(),
                                                      x.get());

    auto& data = logger->get().linop_apply_started.back();
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->A.get()), A, 0);
    ASSERT_EQ(data->alpha, nullptr);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->b.get()), b, 0);
    ASSERT_EQ(data->beta, nullptr);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->x.get()), x, 0);
}


TEST(Record, CatchesLinOpApplyCompleted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::linop_apply_completed_mask);
    auto A = gko::initialize<Dense>({1.1}, exec);
    auto b = gko::initialize<Dense>({-2.2}, exec);
    auto x = gko::initialize<Dense>({3.3}, exec);

    logger->on<gko::log::Logger::linop_apply_completed>(A.get(), b.get(),
                                                        x.get());

    auto& data = logger->get().linop_apply_completed.back();
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->A.get()), A, 0);
    ASSERT_EQ(data->alpha, nullptr);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->b.get()), b, 0);
    ASSERT_EQ(data->beta, nullptr);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->x.get()), x, 0);
}


TEST(Record, CatchesLinOpAdvancedApplyStarted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::linop_advanced_apply_started_mask);
    auto A = gko::initialize<Dense>({1.1}, exec);
    auto alpha = gko::initialize<Dense>({-4.4}, exec);
    auto b = gko::initialize<Dense>({-2.2}, exec);
    auto beta = gko::initialize<Dense>({-5.5}, exec);
    auto x = gko::initialize<Dense>({3.3}, exec);

    logger->on<gko::log::Logger::linop_advanced_apply_started>(
        A.get(), alpha.get(), b.get(), beta.get(), x.get());

    auto& data = logger->get().linop_advanced_apply_started.back();
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->A.get()), A, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->alpha.get()), alpha, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->b.get()), b, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->beta.get()), beta, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->x.get()), x, 0);
}


TEST(Record, CatchesLinOpAdvancedApplyCompleted)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::linop_advanced_apply_completed_mask);
    auto A = gko::initialize<Dense>({1.1}, exec);
    auto alpha = gko::initialize<Dense>({-4.4}, exec);
    auto b = gko::initialize<Dense>({-2.2}, exec);
    auto beta = gko::initialize<Dense>({-5.5}, exec);
    auto x = gko::initialize<Dense>({3.3}, exec);

    logger->on<gko::log::Logger::linop_advanced_apply_completed>(
        A.get(), alpha.get(), b.get(), beta.get(), x.get());

    auto& data = logger->get().linop_advanced_apply_completed.back();
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->A.get()), A, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->alpha.get()), alpha, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->b.get()), b, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->beta.get()), beta, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->x.get()), x, 0);
}


TEST(Record, CatchesLinopFactoryGenerateStarted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::linop_factory_generate_started_mask);
    auto factory =
        gko::solver::Bicgstab<>::build()
            .with_criteria(gko::stop::Iteration::build().with_max_iters(3u))
            .on(exec);
    auto input = factory->generate(gko::matrix::Dense<>::create(exec));

    logger->on<gko::log::Logger::linop_factory_generate_started>(factory.get(),
                                                                 input.get());

    auto& data = logger->get().linop_factory_generate_started.back();
    ASSERT_EQ(data->factory, factory.get());
    ASSERT_NE(data->input.get(), nullptr);
    ASSERT_EQ(data->output.get(), nullptr);
}


TEST(Record, CatchesLinopFactoryGenerateCompleted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::linop_factory_generate_completed_mask);
    auto factory =
        gko::solver::Bicgstab<>::build()
            .with_criteria(gko::stop::Iteration::build().with_max_iters(3u))
            .on(exec);
    auto input = factory->generate(gko::matrix::Dense<>::create(exec));
    auto output = factory->generate(gko::matrix::Dense<>::create(exec));

    logger->on<gko::log::Logger::linop_factory_generate_completed>(
        factory.get(), input.get(), output.get());

    auto& data = logger->get().linop_factory_generate_completed.back();
    ASSERT_EQ(data->factory, factory.get());
    ASSERT_NE(data->input.get(), nullptr);
    ASSERT_NE(data->output.get(), nullptr);
}


TEST(Record, CatchesCriterionCheckStarted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::criterion_check_started_mask);
    auto criterion =
        gko::stop::Iteration::build().with_max_iters(3u).on(exec)->generate(
            nullptr, nullptr, nullptr);
    constexpr gko::uint8 RelativeStoppingId{42};

    logger->on<gko::log::Logger::criterion_check_started>(
        criterion.get(), 1, nullptr, nullptr, nullptr, RelativeStoppingId,
        true);

    auto& data = logger->get().criterion_check_started.back();
    ASSERT_NE(data->criterion, nullptr);
    ASSERT_EQ(data->stopping_id, RelativeStoppingId);
    ASSERT_EQ(data->set_finalized, true);
    ASSERT_EQ(data->oneChanged, false);
    ASSERT_EQ(data->converged, false);
}


TEST(Record, CatchesCriterionCheckCompletedOld)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::criterion_check_completed_mask);
    auto criterion =
        gko::stop::Iteration::build().with_max_iters(3u).on(exec)->generate(
            nullptr, nullptr, nullptr);
    constexpr gko::uint8 RelativeStoppingId{42};
    gko::array<gko::stopping_status> stop_status(exec, 1);

    logger->on<gko::log::Logger::criterion_check_completed>(
        criterion.get(), 1, nullptr, nullptr, nullptr, RelativeStoppingId, true,
        &stop_status, true, true);

    stop_status.get_data()->reset();
    stop_status.get_data()->stop(RelativeStoppingId);
    auto& data = logger->get().criterion_check_completed.back();
    ASSERT_NE(data->criterion, nullptr);
    ASSERT_EQ(data->stopping_id, RelativeStoppingId);
    ASSERT_EQ(data->set_finalized, true);
    ASSERT_EQ(data->status->get_const_data()->has_stopped(), true);
    ASSERT_EQ(data->status->get_const_data()->get_id(),
              stop_status.get_const_data()->get_id());
    ASSERT_EQ(data->status->get_const_data()->is_finalized(), true);
    ASSERT_EQ(data->oneChanged, true);
    ASSERT_EQ(data->converged, true);
}


TEST(Record, CatchesCriterionCheckCompleted)
{
    auto exec = gko::ReferenceExecutor::create();
    auto logger = gko::log::Record::create(
        gko::log::Logger::criterion_check_completed_mask);
    auto criterion =
        gko::stop::Iteration::build().with_max_iters(3u).on(exec)->generate(
            nullptr, nullptr, nullptr);
    constexpr gko::uint8 RelativeStoppingId{42};
    gko::array<gko::stopping_status> stop_status(exec, 1);

    logger->on<gko::log::Logger::criterion_check_completed>(
        criterion.get(), 1, nullptr, nullptr, nullptr, nullptr,
        RelativeStoppingId, true, &stop_status, true, true);

    stop_status.get_data()->reset();
    stop_status.get_data()->stop(RelativeStoppingId);
    auto& data = logger->get().criterion_check_completed.back();
    ASSERT_NE(data->criterion, nullptr);
    ASSERT_EQ(data->stopping_id, RelativeStoppingId);
    ASSERT_EQ(data->set_finalized, true);
    ASSERT_EQ(data->status->get_const_data()->has_stopped(), true);
    ASSERT_EQ(data->status->get_const_data()->get_id(),
              stop_status.get_const_data()->get_id());
    ASSERT_EQ(data->status->get_const_data()->is_finalized(), true);
    ASSERT_EQ(data->oneChanged, true);
    ASSERT_EQ(data->converged, true);
}


TEST(Record, CatchesIterations)
{
    using Dense = gko::matrix::Dense<>;
    auto exec = gko::ReferenceExecutor::create();
    auto logger =
        gko::log::Record::create(gko::log::Logger::iteration_complete_mask);
    auto factory =
        gko::solver::Bicgstab<>::build()
            .with_criteria(gko::stop::Iteration::build().with_max_iters(3u))
            .on(exec);
    auto solver = factory->generate(gko::initialize<Dense>({1.1}, exec));
    auto right_hand_side = gko::initialize<Dense>({-5.5}, exec);
    auto residual = gko::initialize<Dense>({-4.4}, exec);
    auto solution = gko::initialize<Dense>({-2.2}, exec);
    auto residual_norm = gko::initialize<Dense>({-3.3}, exec);
    auto implicit_sq_residual_norm = gko::initialize<Dense>({-3.5}, exec);
    constexpr gko::uint8 RelativeStoppingId{42};
    gko::array<gko::stopping_status> stop_status(exec, 1);
    stop_status.get_data()->reset();
    stop_status.get_data()->converge(RelativeStoppingId);

    logger->on<gko::log::Logger::iteration_complete>(
        solver.get(), right_hand_side.get(), solution.get(), num_iters,
        residual.get(), residual_norm.get(), implicit_sq_residual_norm.get(),
        &stop_status, true);

    stop_status.get_data()->reset();
    stop_status.get_data()->stop(RelativeStoppingId);
    auto& data = logger->get().iteration_completed.back();
    ASSERT_NE(data->solver.get(), nullptr);
    ASSERT_EQ(data->num_iterations, num_iters);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->residual.get()), residual, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->right_hand_side.get()),
                        right_hand_side, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->solution.get()), solution, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->residual_norm.get()),
                        residual_norm, 0);
    GKO_ASSERT_MTX_NEAR(gko::as<Dense>(data->implicit_sq_residual_norm.get()),
                        implicit_sq_residual_norm, 0);
    ASSERT_EQ(data->status.get_const_data()->has_stopped(), true);
    ASSERT_EQ(data->status.get_const_data()->get_id(),
              stop_status.get_const_data()->get_id());
    ASSERT_EQ(data->status.get_const_data()->is_finalized(), true);
    ASSERT_TRUE(data->all_stopped);
}


}  // namespace
