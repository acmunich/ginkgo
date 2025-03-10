// SPDX-FileCopyrightText: 2017-2023 The Ginkgo authors
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GKO_DPCPP_COMPONENTS_WARP_BLAS_DP_HPP_
#define GKO_DPCPP_COMPONENTS_WARP_BLAS_DP_HPP_


#include <cassert>
#include <type_traits>


#include <CL/sycl.hpp>


#include <ginkgo/config.hpp>


#include "dpcpp/base/dpct.hpp"
#include "dpcpp/components/cooperative_groups.dp.hpp"
#include "dpcpp/components/reduction.dp.hpp"


namespace gko {
namespace kernels {
namespace dpcpp {


/**
 * @internal
 *
 * Defines a postprocessing transformation that should be performed on the
 * result of a function call.
 *
 * @note This functionality should become useless once accessors and ranges are
 *       in place, as they will define the storage scheme.
 */
enum postprocess_transformation { and_return, and_transpose };


/**
 * @internal
 *
 * Applies a Gauss-Jordan transformation (single step of Gauss-Jordan
 * elimination) to a `max_problem_size`-by-`max_problem_size` matrix using the
 * thread group `group. Each thread contributes one `row` of the matrix, and the
 * routine uses warp shuffles to exchange data between rows. The transform is
 * performed by using the `key_row`-th row and `key_col`-th column of the
 * matrix.
 */
template <
    int max_problem_size, typename Group, typename ValueType,
    typename = std::enable_if_t<group::is_communicator_group<Group>::value>>
__dpct_inline__ void apply_gauss_jordan_transform(
    const Group& __restrict__ group, int32 key_row, int32 key_col,
    ValueType* __restrict__ row, bool& __restrict__ status)
{
    auto key_col_elem = group.shfl(row[key_col], key_row);
    if (key_col_elem == zero<ValueType>()) {
        // TODO: implement error handling for GPUs to be able to properly
        //       report it here
        status = false;
        return;
    }
    if (group.thread_rank() == key_row) {
        key_col_elem = one<ValueType>() / key_col_elem;
    } else {
        key_col_elem = -row[key_col] / key_col_elem;
    }
#pragma unroll
    for (int32 i = 0; i < max_problem_size; ++i) {
        const auto key_row_elem = group.shfl(row[i], key_row);
        if (group.thread_rank() == key_row) {
            row[i] = zero<ValueType>();
        }
        row[i] += key_col_elem * key_row_elem;
    }
    row[key_col] = key_col_elem;
}


/**
 * @internal
 *
 * Applies a Gauss-Jordan transformation (single step of Gauss-Jordan
 * elimination) to a `max_problem_size`-by-`max_problem_size` matrix using the
 * thread group `group. Each thread contributes one `row` of the matrix, and the
 * routine uses warp shuffles to exchange data between rows. The transform is
 * performed by using the `key_row`-th row and `key_col`-th column of the
 * matrix.
 * Works with one right hand side vector `rhs` which can be directly worked on
 * when solving Ax = rhs without the need of storing the inverse of A.
 */
template <
    int max_problem_size, typename Group, typename ValueType,
    typename = std::enable_if_t<group::is_communicator_group<Group>::value>>
__dpct_inline__ void apply_gauss_jordan_transform_with_rhs(
    const Group& __restrict__ group, int32 key_row, int32 key_col,
    ValueType* __restrict__ row, ValueType* __restrict__ rhs,
    bool& __restrict__ status)
{
    auto key_col_elem = group.shfl(row[key_col], key_row);
    auto key_rhs_elem = group.shfl(rhs[0], key_row);
    if (key_col_elem == zero<ValueType>()) {
        // TODO: implement error handling for GPUs to be able to properly
        //       report it here
        status = false;
        return;
    }
    if (group.thread_rank() == key_row) {
        key_col_elem = one<ValueType>() / key_col_elem;
        rhs[0] = key_rhs_elem * key_col_elem;
    } else {
        key_col_elem = -row[key_col] / key_col_elem;
        rhs[0] += key_rhs_elem * key_col_elem;
    }
#pragma unroll
    for (int32 i = 0; i < max_problem_size; ++i) {
        const auto key_row_elem = group.shfl(row[i], key_row);
        if (group.thread_rank() == key_row) {
            row[i] = zero<ValueType>();
        }
        // rhs[0] += key_rhs_elem * key_row_elem;
        row[i] += key_col_elem * key_row_elem;
    }
    row[key_col] = key_col_elem;
}


/**
 * @internal
 *
 * Inverts a matrix using Gauss-Jordan elimination. The inversion is
 * done in-place, so the original matrix will be overridden with the inverse.
 * The inversion routine uses implicit pivoting, so the returned matrix will be
 * a permuted inverse (from both sides). To obtain the correct inverse, the
 * rows of the result should be permuted with $P$, and the columns with
 * $ P^T $ (i.e. $ A^{-1} = P X P $, where $ X $ is the returned matrix). These
 * permutation matrices are returned compressed as vectors `perm`
 * and`trans_perm`, respectively. `i`-th value of each of the vectors is
 * returned to thread of the group with rank `i`.
 *
 * @tparam max_problem_size  the maximum problem size that will be passed to the
 *                           inversion routine (a tighter bound results in
 *                           faster code
 * @tparam Group  type of the group of threads
 * @tparam ValueType  type of values stored in the matrix
 *
 * @param group  the group of threads which participate in the inversion
 * @param problem_size  the actual size of the matrix (cannot be larger than
 *                      max_problem_size)
 * @param row  a pointer to the matrix row (i-th thread in the group should
 *             pass the pointer to the i-th row), has to have at least
 *             max_problem_size elements
 * @param perm  a value to hold an element of permutation matrix $ P $
 * @param trans_perm  a value to hold an element of permutation matrix $ P^T $
 *
 * @return true if the inversion succeeded, false otherwise
 */
template <
    int max_problem_size, typename Group, typename ValueType,
    typename = std::enable_if_t<group::is_communicator_group<Group>::value>>
__dpct_inline__ bool invert_block(const Group& __restrict__ group,
                                  uint32 problem_size,
                                  ValueType* __restrict__ row,
                                  uint32& __restrict__ perm,
                                  uint32& __restrict__ trans_perm)
{
    GKO_ASSERT(problem_size <= max_problem_size);
    // prevent rows after problem_size to become pivots
    auto pivoted = group.thread_rank() >= problem_size;
    auto status = true;
#ifdef GINKGO_JACOBI_FULL_OPTIMIZATIONS
#pragma unroll
#else
#pragma unroll 1
#endif
    for (int32 i = 0; i < max_problem_size; ++i) {
        if (i < problem_size) {
            const auto piv = choose_pivot(group, row[i], pivoted);
            if (group.thread_rank() == piv) {
                perm = i;
                pivoted = true;
            }
            if (group.thread_rank() == i) {
                trans_perm = piv;
            }
            apply_gauss_jordan_transform<max_problem_size>(group, piv, i, row,
                                                           status);
        }
    }
    return status;
}


/**
 * @internal
 *
 * Performs the correct index calculation for the given postprocess operation.
 */
template <postprocess_transformation mod, typename T1, typename T2, typename T3>
__dpct_inline__ auto get_row_major_index(T1 row, T2 col, T3 stride) ->
    typename std::enable_if<
        mod != and_transpose,
        typename std::decay<decltype(row * stride + col)>::type>::type
{
    return row * stride + col;
}


template <postprocess_transformation mod, typename T1, typename T2, typename T3>
__dpct_inline__ auto get_row_major_index(T1 row, T2 col, T3 stride) ->
    typename std::enable_if<
        mod == and_transpose,
        typename std::decay<decltype(col * stride + row)>::type>::type
{
    return col * stride + row;
}


/**
 * @internal
 *
 * Copies a matrix stored as a collection of rows in different threads of the
 * warp in a block of memory accessible by all threads in row-major order.
 * Optionally permutes rows and columns of the matrix in the process.
 *
 * @tparam max_problem_size  maximum problem size passed to the routine
 * @tparam mod  the transformation to perform on the return data
 * @tparam Group  type of the group of threads
 * @tparam SourceValueType  type of values stored in the source matrix
 * @tparam ResultValueType  type of values stored in the result matrix
 *
 * @param group  group of threads participating in the copy
 * @param problem_size  actual size of the matrix
 *                      (`problem_size <= max_problem_size`)
 * @param source_row  pointer to memory used to store a row of the source matrix
 *                    `i`-th thread of the sub-warp should pass in the `i`-th
 *                    row of the matrix
 * @param increment  offset between two consecutive elements of the row
 * @param row_perm  permutation vector to apply on the rows of the matrix
 *                  (thread `i` supplies the `i`-th value of the vector)
 * @param col_perm  permutation vector to apply on the column of the matrix
 *                  (thread `i` supplies the `i`-th value of the vector)
 * @param destination  pointer to memory where the result will be stored
 *                     (all threads supply the same value)
 * @param stride  offset between two consecutive rows of the matrix
 */
template <
    int max_problem_size, postprocess_transformation mod = and_return,
    typename Group, typename SourceValueType, typename ResultValueType,
    typename = std::enable_if_t<group::is_communicator_group<Group>::value>>
__dpct_inline__ void copy_matrix(const Group& __restrict__ group,
                                 uint32 problem_size,
                                 const SourceValueType* __restrict__ source_row,
                                 uint32 increment, uint32 row_perm,
                                 uint32 col_perm,
                                 ResultValueType* __restrict__ destination,
                                 size_type stride)
{
    GKO_ASSERT(problem_size <= max_problem_size);
#pragma unroll
    for (int32 i = 0; i < max_problem_size; ++i) {
        if (i < problem_size) {
            const auto idx = group.shfl(col_perm, i);
            if (group.thread_rank() < problem_size) {
                // Need to assign a variable for the source_row, or hip
                // will use a lot of VGPRs in unroll. This might lead to
                // problems.
                const auto val = source_row[i * increment];
                destination[get_row_major_index<mod>(idx, row_perm, stride)] =
                    static_cast<ResultValueType>(val);
            }
        }
    }
}


/**
 * @internal
 *
 * Multiplies a transposed vector and a matrix stored in column-major order.
 *
 * In mathematical terms, performs the operation $ res^T = vec^T \cdot mtx$.
 *
 * @tparam max_problem_size  maximum problem size passed to the routine
 * @tparam Group  type of the group of threads
 * @tparam MatrixValueType  type of values stored in the matrix
 * @tparam VectorValueType  type of values stored in the vectors
 *
 * @param group  group of threads participating in the operation
 * @param problem_size  actual size of the matrix
 *                      (`problem_size <= max_problem_size`)
 * @param vec  input vector to multiply (thread `i` supplies the `i`-th value of
 *             the vector)
 * @param mtx_row  pointer to memory used to store a row of the input matrix,
 *                    `i`-th thread of the sub-warp should pass in the
 *                    `i`-th row of the matrix
 * @param mtx_increment  offset between two consecutive elements of the row
 * @param res  pointer to a block of memory where the result will be written
 *             (only thread 0 of the group has to supply a valid value)
 * @param mtx_increment  offset between two consecutive elements of the result
 */
template <
    int max_problem_size, typename Group, typename MatrixValueType,
    typename VectorValueType,
    typename = std::enable_if_t<group::is_communicator_group<Group>::value>>
__dpct_inline__ void multiply_transposed_vec(
    const Group& __restrict__ group, uint32 problem_size,
    const VectorValueType& __restrict__ vec,
    const MatrixValueType* __restrict__ mtx_row, uint32 mtx_increment,
    VectorValueType* __restrict__ res, uint32 res_increment)
{
    GKO_ASSERT(problem_size <= max_problem_size);
    auto mtx_elem = zero<VectorValueType>();
#pragma unroll
    for (int32 i = 0; i < max_problem_size; ++i) {
        if (i < problem_size) {
            if (group.thread_rank() < problem_size) {
                mtx_elem =
                    static_cast<VectorValueType>(mtx_row[i * mtx_increment]);
            }
            const auto out = ::gko::kernels::dpcpp::reduce(
                group, mtx_elem * vec,
                [](VectorValueType x, VectorValueType y) { return x + y; });
            if (group.thread_rank() == 0) {
                res[i * res_increment] = out;
            }
        }
    }
}


/**
 * @internal
 *
 * Multiplies a matrix and a vector stored in column-major order.
 *
 * In mathematical terms, performs the operation $res = mtx \cdot vec$.
 *
 * @tparam max_problem_size  maximum problem size passed to the routine
 * @tparam Group  type of the group of threads
 * @tparam MatrixValueType  type of values stored in the matrix
 * @tparam VectorValueType  type of values stored in the vectors
 * @tparam Closure  type of the function used to write the result
 *
 * @param group  group of threads participating in the operation
 * @param problem_size  actual size of the matrix
 *                      (`problem_size <= max_problem_size`)
 * @param vec  input vector to multiply (thread `i` supplies the `i`-th value of
 *             the vector)
 * @param mtx_row  pointer to memory used to store a row of the input matrix,
 *                    `i`-th thread of the sub-warp should pass in the
 *                    `i`-th row of the matrix
 * @param mtx_increment  offset between two consecutive elements of the row
 * @param res  pointer to a block of memory where the result will be written
 *             (only thread 0 of the group has to supply a valid value)
 * @param mtx_increment  offset between two consecutive elements of the result
 * @param closure_op  Operation that is performed when writing to
                     `res[group.thread_rank() * res_increment]` as
                     `closure_op(res[group.thread_rank() * res_increment], out)`
                      where `out` is the result of $mtx \cdot vec$.
 */
template <
    int max_problem_size, typename Group, typename MatrixValueType,
    typename VectorValueType, typename Closure,
    typename = std::enable_if_t<group::is_communicator_group<Group>::value>>
__dpct_inline__ void multiply_vec(const Group& __restrict__ group,
                                  uint32 problem_size,
                                  const VectorValueType& __restrict__ vec,
                                  const MatrixValueType* __restrict__ mtx_row,
                                  uint32 mtx_increment,
                                  VectorValueType* __restrict__ res,
                                  uint32 res_increment, Closure closure_op)
{
    GKO_ASSERT(problem_size <= max_problem_size);
    auto mtx_elem = zero<VectorValueType>();
    auto out = zero<VectorValueType>();
#pragma unroll
    for (int32 i = 0; i < max_problem_size; ++i) {
        if (i < problem_size) {
            if (group.thread_rank() < problem_size) {
                mtx_elem =
                    static_cast<VectorValueType>(mtx_row[i * mtx_increment]);
            }
            out += mtx_elem * group.shfl(vec, i);
        }
    }
    if (group.thread_rank() < problem_size) {
        closure_op(res[group.thread_rank() * res_increment], out);
    }
}


/**
 * @internal
 *
 * Computes the infinity norm of a matrix. Each thread in the group supplies
 * one row of the matrix.
 *
 * @tparam max_problem_size  maximum problem size passed to the routine
 * @tparam Group  type of the group of threads
 * @tparam ValueType  type of values stored in the matrix
 *
 * @param group  group of threads participating in the operation
 * @param num_rows  number of rows of the matrix
 *                  (`num_rows <= max_problem_size`)
 * @param num_cols  number of columns of the matrix
 * @param row  pointer to memory used to store a row of the input matrix,
 *             `i`-th thread of the group should pass in the `i`-th row of the
 *             matrix
 *
 * @return the infinity norm of the matrix
 */
template <
    int max_problem_size, typename Group, typename ValueType,
    typename = std::enable_if_t<group::is_communicator_group<Group>::value>>
__dpct_inline__ remove_complex<ValueType> compute_infinity_norm(
    const Group& group, uint32 num_rows, uint32 num_cols, const ValueType* row)
{
    using result_type = remove_complex<ValueType>;
    auto sum = zero<result_type>();
    if (group.thread_rank() < num_rows) {
#ifdef GINKGO_JACOBI_FULL_OPTIMIZATIONS
#pragma unroll
#else
#pragma unroll 1
#endif
        for (uint32 i = 0; i < max_problem_size; ++i) {
            if (i < num_cols) {
                sum += abs(row[i]);
            }
        }
    }
    return ::gko::kernels::dpcpp::reduce(
        group, sum, [](result_type x, result_type y) { return max(x, y); });
}


}  // namespace dpcpp
}  // namespace kernels
}  // namespace gko


#endif  // GKO_DPCPP_COMPONENTS_WARP_BLAS_DP_HPP_
