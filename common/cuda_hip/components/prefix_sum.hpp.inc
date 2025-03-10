// SPDX-FileCopyrightText: 2017-2023 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @internal
 * Computes the prefix sum and total sum of `element` over a subwarp.
 *
 * @param element     the element over which we compute the prefix sum.
 * @param prefix_sum  will be set to the sum of all `element`s from lower
 *                    lanes, plus the local `element` if `inclusive` is `true`.
 * @param total_sum   will be set to the total sum of `element` in this subwarp.
 * @param subwarp     the cooperative group representing the subwarp.
 *
 * @tparam inclusive  if this is true, the computed prefix sum will be
 *                    inclusive, otherwise it will be exclusive.
 *
 * @note For this function to work on architectures with independent thread
 * scheduling, all threads of the subwarp have to execute it.
 */
template <bool inclusive, typename ValueType, typename Group>
__forceinline__ __device__ void subwarp_prefix_sum(ValueType element,
                                                   ValueType& prefix_sum,
                                                   ValueType& total_sum,
                                                   Group subwarp)
{
    prefix_sum = inclusive ? element : zero<ValueType>();
    total_sum = element;
#pragma unroll
    // hypercube prefix sum
    for (int step = 1; step < subwarp.size(); step *= 2) {
        auto neighbor = subwarp.shfl_xor(total_sum, step);
        total_sum += neighbor;
        prefix_sum += bool(subwarp.thread_rank() & step) ? neighbor : 0;
    }
}

/**
 * @internal
 * Computes the prefix sum of `element` over a subwarp.
 *
 * @param element     the element over which we compute the prefix sum.
 * @param prefix_sum  will be set to the sum of all `element`s from lower
 *                    lanes, plus the local `element` if `inclusive` is `true`.
 * @param subwarp     the cooperative group representing the subwarp.
 *
 * @tparam inclusive  if this is true, the computed prefix sum will be
 *                    inclusive, otherwise it will be exclusive.
 *
 * @note All threads of the subwarp have to execute this function for it to work
 *       (and not dead-lock on newer architectures).
 */
template <bool inclusive, typename ValueType, typename Group>
__forceinline__ __device__ void subwarp_prefix_sum(ValueType element,
                                                   ValueType& prefix_sum,
                                                   Group subwarp)
{
    ValueType tmp{};
    subwarp_prefix_sum<inclusive>(element, prefix_sum, tmp, subwarp);
}


/**
 * @internal
 * First step of the calculation of a prefix sum. Calculates the prefix sum
 * in-place on parts of the array `elements`.
 *
 * @param elements  array on which the prefix sum is to be calculated
 * @param block_sum  array which stores the total sum of each block, requires at
 *                   least `ceildiv(num_elements, block_size) - 1` elements
 * @param num_elements  total number of entries in `elements`
 *
 * @tparam block_size  thread block size for this kernel, also size of blocks on
 *                     which this kernel calculates the prefix sum in-place
 *
 * @note To calculate the prefix sum over an array of size bigger than
 *       `block_size`, `finalize_prefix_sum` has to be used as well.
 */
template <int block_size, typename ValueType>
__global__ __launch_bounds__(block_size) void start_prefix_sum(
    size_type num_elements, ValueType* __restrict__ elements,
    ValueType* __restrict__ block_sum)
{
    const auto tidx = thread::get_thread_id_flat();
    const auto element_id = threadIdx.x;
    __shared__ uninitialized_array<ValueType, block_size> prefix_helper;
    // do not need to access the last element when exclusive prefix sum
    prefix_helper[element_id] =
        (tidx + 1 < num_elements) ? elements[tidx] : zero<ValueType>();
    auto this_block = group::this_thread_block();
    this_block.sync();

    // Do a normal reduction
#pragma unroll
    for (int i = 1; i < block_size; i <<= 1) {
        const auto ai = i * (2 * element_id + 1) - 1;
        const auto bi = i * (2 * element_id + 2) - 1;
        if (bi < block_size) {
            prefix_helper[bi] += prefix_helper[ai];
        }
        this_block.sync();
    }

    if (element_id == 0) {
        // Store the total sum except the last block
        if (blockIdx.x + 1 < gridDim.x) {
            block_sum[blockIdx.x] = prefix_helper[block_size - 1];
        }
        prefix_helper[block_size - 1] = zero<ValueType>();
    }

    this_block.sync();

    // Perform the down-sweep phase to get the true prefix sum
#pragma unroll
    for (int i = block_size >> 1; i > 0; i >>= 1) {
        const auto ai = i * (2 * element_id + 1) - 1;
        const auto bi = i * (2 * element_id + 2) - 1;
        if (bi < block_size) {
            auto tmp = prefix_helper[ai];
            prefix_helper[ai] = prefix_helper[bi];
            prefix_helper[bi] += tmp;
        }
        this_block.sync();
    }
    if (tidx < num_elements) {
        elements[tidx] = prefix_helper[element_id];
    }
}


/**
 * @internal
 * Second step of the calculation of a prefix sum. Increases the value of each
 * entry of `elements` by the total sum of all preceding blocks.
 *
 * @param elements  array on which the prefix sum is to be calculated
 * @param block_sum  array storing the total sum of each block
 * @param num_elements  total number of entries in `elements`
 *
 * @tparam block_size  thread block size for this kernel, has to be the same as
 *                    for `start_prefix_sum`
 *
 * @note To calculate a prefix sum, first `start_prefix_sum` has to be called.
 */
template <int block_size, typename ValueType>
__global__ __launch_bounds__(block_size) void finalize_prefix_sum(
    size_type num_elements, ValueType* __restrict__ elements,
    const ValueType* __restrict__ block_sum)
{
    const auto tidx = thread::get_thread_id_flat();

    if (tidx < num_elements) {
        ValueType prefix_block_sum = zero<ValueType>();
        for (size_type i = 0; i < blockIdx.x; i++) {
            prefix_block_sum += block_sum[i];
        }
        elements[tidx] += prefix_block_sum;
    }
}
