// SPDX-FileCopyrightText: 2017-2023 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

template <typename ValueType>
__device__ __forceinline__ void simple_apply(
    const gko::batch::matrix::dense::batch_item<const ValueType>& mat,
    const ValueType* const __restrict__ b, ValueType* const __restrict__ x)
{
    constexpr auto tile_size = config::warp_size;

    auto thread_block = group::this_thread_block();
    auto subgroup = group::tiled_partition<tile_size>(thread_block);
    const auto subgroup_id = static_cast<int>(threadIdx.x / tile_size);
    const int num_subgroups_per_block = ceildiv(blockDim.x, tile_size);

    for (int row = subgroup_id; row < mat.num_rows;
         row += num_subgroups_per_block) {
        ValueType temp = zero<ValueType>();
        for (int j = subgroup.thread_rank(); j < mat.num_cols;
             j += subgroup.size()) {
            const ValueType val = mat.values[row * mat.stride + j];
            temp += val * b[j];
        }

        // subgroup level reduction
        temp = reduce(subgroup, temp, thrust::plus<ValueType>{});

        if (subgroup.thread_rank() == 0) {
            x[row] = temp;
        }
    }
}

template <typename ValueType>
__global__ __launch_bounds__(
    default_block_size,
    sm_oversubscription) void simple_apply_kernel(const gko::batch::matrix::
                                                      dense::uniform_batch<
                                                          const ValueType>
                                                          mat,
                                                  const gko::batch::
                                                      multi_vector::
                                                          uniform_batch<
                                                              const ValueType>
                                                              b,
                                                  const gko::batch::
                                                      multi_vector::
                                                          uniform_batch<
                                                              ValueType>
                                                              x)
{
    for (size_type batch_id = blockIdx.x; batch_id < mat.num_batch_items;
         batch_id += gridDim.x) {
        const auto mat_b =
            gko::batch::matrix::extract_batch_item(mat, batch_id);
        const auto b_b = gko::batch::extract_batch_item(b, batch_id);
        const auto x_b = gko::batch::extract_batch_item(x, batch_id);
        simple_apply(mat_b, b_b.values, x_b.values);
    }
}


template <typename ValueType>
__device__ __forceinline__ void advanced_apply(
    const ValueType alpha,
    const gko::batch::matrix::dense::batch_item<const ValueType>& mat,
    const ValueType* const __restrict__ b, const ValueType beta,
    ValueType* const __restrict__ x)
{
    constexpr auto tile_size = config::warp_size;

    auto thread_block = group::this_thread_block();
    auto subgroup = group::tiled_partition<tile_size>(thread_block);
    const auto subgroup_id = static_cast<int>(threadIdx.x / tile_size);
    const int num_subgroups_per_block = ceildiv(blockDim.x, tile_size);

    for (int row = subgroup_id; row < mat.num_rows;
         row += num_subgroups_per_block) {
        ValueType temp = zero<ValueType>();
        for (int j = subgroup.thread_rank(); j < mat.num_cols;
             j += subgroup.size()) {
            const ValueType val = mat.values[row * mat.stride + j];
            temp += alpha * val * b[j];
        }

        // subgroup level reduction
        temp = reduce(subgroup, temp, thrust::plus<ValueType>{});

        if (subgroup.thread_rank() == 0) {
            x[row] = temp + beta * x[row];
        }
    }
}

template <typename ValueType>
__global__ __launch_bounds__(
    default_block_size,
    sm_oversubscription) void advanced_apply_kernel(const gko::batch::
                                                        multi_vector::
                                                            uniform_batch<
                                                                const ValueType>
                                                                alpha,
                                                    const gko::batch::matrix::
                                                        dense::uniform_batch<
                                                            const ValueType>
                                                            mat,
                                                    const gko::batch::
                                                        multi_vector::
                                                            uniform_batch<
                                                                const ValueType>
                                                                b,
                                                    const gko::batch::
                                                        multi_vector::
                                                            uniform_batch<
                                                                const ValueType>
                                                                beta,
                                                    const gko::batch::
                                                        multi_vector::
                                                            uniform_batch<
                                                                ValueType>
                                                                x)
{
    for (size_type batch_id = blockIdx.x; batch_id < mat.num_batch_items;
         batch_id += gridDim.x) {
        const auto mat_b =
            gko::batch::matrix::extract_batch_item(mat, batch_id);
        const auto b_b = gko::batch::extract_batch_item(b, batch_id);
        const auto x_b = gko::batch::extract_batch_item(x, batch_id);
        const auto alpha_b = gko::batch::extract_batch_item(alpha, batch_id);
        const auto beta_b = gko::batch::extract_batch_item(beta, batch_id);
        advanced_apply(alpha_b.values[0], mat_b, b_b.values, beta_b.values[0],
                       x_b.values);
    }
}
