// SPDX-FileCopyrightText: 2017-2023 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

#include <ginkgo/core/base/executor.hpp>


#include <memory>
#include <thread>
#include <type_traits>


#if defined(__unix__) || defined(__APPLE__)
#include <numa.h>
#include <utmpx.h>
#endif


#include <gtest/gtest.h>


#include <ginkgo/core/base/exception.hpp>
#include <ginkgo/core/base/exception_helpers.hpp>


#include "cuda/test/utils.hpp"


namespace {


class CudaExecutor : public ::testing::Test {
protected:
    CudaExecutor()
        : ref(gko::ReferenceExecutor::create()), cuda(nullptr), cuda2(nullptr)
    {}

    void SetUp()
    {
        ASSERT_GT(gko::CudaExecutor::get_num_devices(), 0);
        cuda = gko::CudaExecutor::create(0, ref);
        cuda2 = gko::CudaExecutor::create(
            gko::CudaExecutor::get_num_devices() - 1, ref);
    }

    void TearDown()
    {
        if (cuda != nullptr) {
            // ensure that previous calls finished and didn't throw an error
            ASSERT_NO_THROW(cuda->synchronize());
        }
    }

    std::shared_ptr<gko::ReferenceExecutor> ref;
    std::shared_ptr<const gko::CudaExecutor> cuda;
    std::shared_ptr<const gko::CudaExecutor> cuda2;
};


#if GKO_HAVE_HWLOC


inline int get_cpu_os_id(int log_id)
{
    return gko::machine_topology::get_instance()->get_pu(log_id)->os_id;
}


inline int get_core_os_id(int log_id)
{
    return gko::machine_topology::get_instance()->get_core(log_id)->os_id;
}


TEST_F(CudaExecutor, CanBindToSinglePu)
{
    cuda = gko::CudaExecutor::create(0, gko::ReferenceExecutor::create());

    const int bind_pu = 1;
    gko::machine_topology::get_instance()->bind_to_pu(bind_pu);

    auto cpu_sys = sched_getcpu();
    ASSERT_TRUE(cpu_sys == get_cpu_os_id(1));
}


TEST_F(CudaExecutor, CanBindToPus)
{
    cuda = gko::CudaExecutor::create(0, gko::ReferenceExecutor::create());

    std::vector<int> bind_pus = {1, 3};
    gko::machine_topology::get_instance()->bind_to_pus(bind_pus);

    auto cpu_sys = sched_getcpu();
    ASSERT_TRUE(cpu_sys == get_cpu_os_id(3) || cpu_sys == get_cpu_os_id(1));
}


TEST_F(CudaExecutor, CanBindToCores)
{
    cuda = gko::CudaExecutor::create(0, gko::ReferenceExecutor::create());

    std::vector<int> bind_cores = {1, 3};
    gko::machine_topology::get_instance()->bind_to_cores(bind_cores);

    auto cpu_sys = sched_getcpu();
    ASSERT_TRUE(cpu_sys == get_core_os_id(3) || cpu_sys == get_core_os_id(1));
}


TEST_F(CudaExecutor, ClosestCpusIsPopulated)
{
    cuda = gko::CudaExecutor::create(0, gko::ReferenceExecutor::create());
    auto close_cpus = cuda->get_closest_pus();
    if (close_cpus.size() == 0) {
        GTEST_SKIP();
    }

    ASSERT_NE(close_cpus[0], -1);
}


TEST_F(CudaExecutor, KnowsItsNuma)
{
    cuda = gko::CudaExecutor::create(0, gko::ReferenceExecutor::create());
    auto numa0 = cuda->get_closest_numa();
    auto close_cpus = cuda->get_closest_pus();
    if (close_cpus.size() == 0) {
        GTEST_SKIP();
    }

    auto numa_sys0 = numa_node_of_cpu(get_cpu_os_id(close_cpus[0]));

    ASSERT_TRUE(numa0 == numa_sys0);
}


#endif


}  // namespace
