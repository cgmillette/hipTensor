/*******************************************************************************
 *
 * MIT License
 *
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

// This (ifndef) is a hack to use customized behavior for buffer load rather
// than using default setting Don't use this hack unless absolutely necessary!
// FIXME: make the behavior of buffer load a configurable (template) parameter
// of each device op
#define CK_EXPERIMENTAL_USE_BUFFER_LOAD_OOB_CHECK_OFFSET_TRICK 1

#include "common.hpp"

namespace ck
{
    namespace tensor_operation
    {
        namespace device
        {
            namespace instance
            {

                using F32       = float;
                using F32_Tuple = ck::Tuple<F32>;

                template <ck::index_t... Is>
                using S = ck::Sequence<Is...>;

                using PassThrough = ck::tensor_operation::element_wise::PassThrough;
                using Bilinear    = ck::tensor_operation::element_wise::Bilinear;

                static constexpr auto GemmMNKPadding
                    = ck::tensor_operation::device::GemmSpecialization::MNKPadding;

                // A[m0, m1, k0, k1] * B[n0, n1, k0, k1] + D[m0, m1, n0, n1] = E[m0, m1, n0, n1]
                // k/n/n/n are the fast changing dimension for A/B/D/E
                using device_contraction_bilinear_m2_n2_k2_xdl_c_shuffle_f32_f32_f32_f32_knnn_instance
                    = std::tuple<
                        // clang-format off
        //#####################################| NumDimM| NumDimN| NumDimK| AData| BData| AccData| CShuffle|    DsData| EData|            A|           B|         CDE|           GEMM| NumGemmK| Block|  MPer|  NPer|  KPer| AK1| BK1| MPer| NPer| MXdl| NXdl|  ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|  BBlockTransfer| BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|    CShuffle|    CShuffle| CBlockTransferClusterLengths|  CBlockTransfer|
        //#####################################|        |        |        |  Type|  Type|    Type| DataType|      Type|  Type|  Elementwise| Elementwise| Elementwise| Specialization| Prefetch|  Size| Block| Block| Block|    |    |  XDL|  XDL|  Per|  Per|   ThreadCluster|  ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|   ThreadCluster|  ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN| MXdlPerWave| NXdlPerWave|         _MBlock_MWaveMPerXdl| ScalarPerVector|
        //#####################################|        |        |        |      |      |        |         |          |      |    Operation|   Operation|   Operation|               |    Stage|      |      |      |      |    |    |     |     | Wave| Wave| Lengths_K0_M_K1|   ArrangeOrder|               |               |      PerVector|   PerVector_K1|          | Lengths_K0_N_K1|   ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |  PerShuffle|  PerShuffle|         _NBlock_NWaveNPerXdl|   _NWaveNPerXdl|
        //#####################################|        |        |        |      |      |        |         |          |      |             |            |            |               |         |      |      |      |      |    |    |     |     |     |     |                |               |               |               |               |               |          |                |               |               |              |               |               |          |            |            |                             |                |
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,   256,   128,    16,   4,   1,   32,   32,    4,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              1,         0,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,   256,   128,    16,   4,   4,   32,   32,    4,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              2,              4,         1,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,   128,   256,    16,   4,   1,   32,   32,    2,    4,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              1,         0,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,   128,   256,    16,   4,   4,   32,   32,    2,    4,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              4,         1,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   128,   128,   128,    16,   4,   1,   32,   32,    4,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              1,         0,           1,           1,              S<1,  8, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   128,   128,   128,    16,   4,   4,   32,   32,    4,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              4,         1,           1,           1,              S<1,  8, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,   128,   128,    16,   4,   1,   32,   32,    2,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              1,         0,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,   128,   128,    16,   4,   4,   32,   32,    2,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              2,              4,         1,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   128,   128,    64,    16,   4,   1,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<8, 16, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              1,         0,           1,           1,              S<1, 16, 1,  8>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   128,   128,    64,    16,   4,   4,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              2,              4,         1,           1,           1,              S<1, 16, 1,  8>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   128,    64,   128,    16,   4,   1,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              1,         0,           1,           1,              S<1,  8, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   128,    64,   128,    16,   4,   4,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              4,         1,           1,           1,              S<1,  8, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,   128,    64,    16,   4,   1,   32,   32,    2,    1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<16,16, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              1,         0,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,   128,    64,    16,   4,   4,   32,   32,    2,    1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              1,              4,         1,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,    64,   128,    16,   4,   1,   32,   32,    1,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              1,         0,           1,           1,              S<1, 16, 1, 16>,               4>,
        DeviceContractionMultipleD_Xdl_CShuffle<       2,       2,       2,   F32,   F32,     F32,      F32, F32_Tuple,   F32,  PassThrough, PassThrough,    Bilinear, GemmMNKPadding,        1,   256,    64,   128,    16,   4,   4,   32,   32,    1,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              4,              4,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              2,              4,         1,           1,           1,              S<1, 16, 1, 16>,               4>
                        // clang-format on
                        >;

                void
                    add_device_contraction_bilinear_m2_n2_k2_xdl_c_shuffle_f32_f32_f32_f32_knnn_instance(
                        std::vector<std::unique_ptr<DeviceContractionMultipleD<2,
                                                                               2,
                                                                               2,
                                                                               F32,
                                                                               F32,
                                                                               F32_Tuple,
                                                                               F32,
                                                                               PassThrough,
                                                                               PassThrough,
                                                                               Bilinear>>>&
                            instances)
                {
                    add_device_operation_instances(
                        instances,
                        device_contraction_bilinear_m2_n2_k2_xdl_c_shuffle_f32_f32_f32_f32_knnn_instance{});
                }

            } // namespace instance
        } // namespace device
    } // namespace tensor_operation
} // namespace ck