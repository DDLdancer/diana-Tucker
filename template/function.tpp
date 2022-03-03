#include "tensor.hpp"
#include "logger.hpp"

#include <cmath>

namespace Function {
/**
 * @brief  Calculate \f$ \bm{\mathcal{A}} \times_n \bm{M} \f$, where
 * \f$ \bm{\mathcal{A}} \f$ is a tensor and \f$ \bm{M} \f$ is a matrix.
 *
 * @tparam Ty
 * @param A A matrix of shape \f$ I_1 \times \cdots \times I_N \f$
 * @param M A matrix of shape \f$ J_n \times I_n \f$
 * @param n Index for TTM routine.
 * @return Tensor<Ty>
 */
template <typename Ty>
Tensor<Ty> ttm(const Tensor<Ty> &A, const Tensor<Ty> &M, size_t n) {
    if (A.distribution()->type() == Distribution::Type::kCartesianBlock &&
        M.distribution()->type() == Distribution::Type::kGlobal) {
        assert(M.is_matrix());
        assert(A.shape_global()[n] == M.shape()[1]);
        const size_t kNdim = A.ndim();
        shape_t coord =
            ((DistributionCartesianBlock *)A.distribution())->coordinate();
        shape_t par =
            ((DistributionCartesianBlock *)A.distribution())->partition();
        size_t row_length = M.shape()[0];
        size_t col_length = M.shape()[1];
        size_t col_local = A.shape()[n];
        size_t col_begin = DIANA_CEILDIV(col_length * coord[n], par[n]);
        size_t col_end = DIANA_CEILDIV(col_length * (coord[n] + 1), par[n]);
        assert(col_end - col_begin == col_local);
        size_t remain_size = A.size() / col_local;
        Ty *data_A = A.data();
        Ty *data_B = A.op()->alloc(A.size());
        Ty *data_M = M.data();
        Ty *data_Anew = A.op()->alloc(row_length * remain_size);
        // Matricization
        A.op()->tenmatt(data_B, data_A, A.shape(), n);
        // Do TTM
        A.op()->matmulNT(data_Anew, data_B, data_M + col_begin * row_length,
                         remain_size, row_length, col_local);
        // Split communicator
        int new_rank = (int)coord[n];
        int new_color = 0;
        int pre = 1;
        for (size_t d = 0; d < kNdim; d++) {
            if (d != n) {
                new_color += (int)coord[d] * pre;
                pre *= (int)par[d];
            }
        }
        MPI_Comm comm_new = A.comm()->comm_split(new_color, new_rank);
        // Do reduce-scatter
        shape_t new_shape = A.shape_global();
        new_shape[n] = row_length;
        Tensor<Ty> ret(A.distribution(), new_shape, false);
        Ty *data_ret = ret.data();
        Ty *data_ret_buf = ret.op()->alloc(ret.size());
        int *recvcounts = new int[par[n]];
        for (size_t i = 0; i < par[n]; i++) {
            recvcounts[i] =
                (int)DIANA_CEILDIV(row_length * (coord[i] + 1), par[n]) -
                (int)DIANA_CEILDIV(row_length * coord[i], par[n]);
            recvcounts[i] *= (int)remain_size;
        }
        ret.comm()->reduce_scatter(data_Anew, data_ret_buf, recvcounts, MPI_SUM,
                                   comm_new);
        // Tensorization
        ret.op()->mattten(data_ret, data_ret_buf, ret.shape(), n);
        // Free spaces
        A.op()->free(data_B);
        A.op()->free(data_Anew);
        A.op()->free(data_ret_buf);
        return ret;
    }
    error("Invalid input or not implemented yet.");
}

template <typename Ty> Tensor<Ty> gather(const Tensor<Ty> &A) {
    if (A.distribution() == nullptr) {
        error("This tensor is not distributed, cannot be gathered.");
    }
    if (A.distribution()->type() == Distribution::Type::kLocal) {
        error("Distribution of this tensor is Distribution::Type::kLocal, "
              "cannot be gathered.");
    }
    if (A.distribution()->type() == Distribution::Type::kGlobal) {
        error("Distribution of this tensor is Distribution::Type::kGlobal, "
              "there is no need to be gathered.");
    }
    if (A.distribution()->type() == Distribution::Type::kCartesianBlock) {
        const int kZERO = 0;
        Tensor<Ty> ret(A.shape_global(), false);
        if (A.comm()->rank() == kZERO) {
            const int kMPISize = mpi_size();
            int *recvcounts = new int[kMPISize];
            int *displs = new int[kMPISize];
            // Calculate recvcounts
            for (int i = 0; i < kMPISize; i++) {
                recvcounts[i] =
                    (int)A.distribution()->local_size(i, A.shape_global());
            }
            // Calculate displs
            displs[0] = 0;
            for (int i = 1; i < kMPISize; i++) {
                displs[i] = displs[i - 1] + recvcounts[i - 1];
            }
            // Receive data
            A.comm()->gatherv(A.data(), (int)A.size(), ret.data(), recvcounts,
                              displs, kZERO);
            // Reorder data
            ret.op()->reorder_from_gather_cartesian_block(
                ret.data(), ret.shape(),
                ((DistributionCartesianBlock *)A.distribution())->partition(),
                displs);
            // Bcast data
            A.comm()->bcast(ret.data(), (int)ret.size(), kZERO);
        } else {
            // Send data
            A.comm()->gatherv(A.data(), (int)A.size(), nullptr, nullptr,
                              nullptr, kZERO);
            // Receive data from Bcast
            A.comm()->bcast(ret.data(), (int)ret.size(), kZERO);
        }
        return ret;
    }
    error("Invalid input or not implemented yet.");
}

template <typename Ty>
Tensor<Ty> scatter(const Tensor<Ty> &A, Distribution *distribution, int proc) {
    if (A.distribution() != nullptr &&
        A.distribution()->type() == Distribution::Type::kCartesianBlock) {
        error("Distribution of this tensor is "
              "Distribution::Type::kCartesianBlock,  there is no need to be "
              "scatterd.");
    }
    if (A.distribution() != nullptr &&
        A.distribution()->type() == Distribution::Type::kGlobal) {
        error("Distribution of this tensor is Distribution::Type::kGlobal, "
              "there is no need to be scatterd.");
    }
    if ((A.distribution() == nullptr ||
         A.distribution()->type() == Distribution::Type::kLocal) &&
        distribution->type() == Distribution::Type::kCartesianBlock) {
        Tensor<Ty> ret(distribution, A.shape(), false);
        if (mpi_rank() == proc) {
            const int kMPISize = mpi_size();
            int *sendcounts = new int[kMPISize];
            int *displs = new int[kMPISize];
            // Calculate sendcounts
            for (int i = 0; i < kMPISize; i++) {
                sendcounts[i] = (int)distribution->local_size(i, A.shape());
            }
            // Calculate displs
            displs[0] = 0;
            for (int i = 1; i < kMPISize; i++) {
                displs[i] = displs[i - 1] + sendcounts[i - 1];
            }
            // Reorder data
            A.op()->reorder_for_scatter_cartesian_block(
                A.data(), A.shape(),
                ((DistributionCartesianBlock *)distribution)->partition(),
                displs);
            // Scatter data
            ret.comm()->scatterv(A.data(), sendcounts, displs, ret.data(),
                                 (int)ret.size(), proc);
            // Reorder back data
            tick;
            A.op()->reorder_from_gather_cartesian_block(
                A.data(), A.shape(),
                ((DistributionCartesianBlock *)distribution)->partition(),
                displs);
        } else {
            // Receive data
            ret.comm()->scatterv(nullptr, nullptr, nullptr, ret.data(),
                                 (int)ret.size(), proc);
        }
        return ret;
    }
    error("Invalid input or not implemented yet.");
}

template <typename Ty> double fnorm(const Tensor<Ty> &A) {
    if (A.distribution()->type() == Distribution::Type::kCartesianBlock) {
        double ret = A.op()->fnorm(A.data(), A.size());
        ret = ret * ret;
        A.comm()->allreduce_inplace(&ret, 1, MPI_SUM);
        return sqrt(ret);
    } else {
        return A.op()->fnorm(A.data(), A.size());
    }
}

template <typename Ty> Ty sum(const Tensor<Ty> &A) {
    if (A.distribution()->type() == Distribution::Type::kCartesianBlock) {
        Ty ret = A.op()->sum(A.data(), A.size());
        A.comm()->allreduce_inplace(&ret, 1, MPI_SUM);
        return ret;
    } else {
        return A.op()->sum(A.data(), A.size());
    }
}

} // namespace Function