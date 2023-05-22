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
#include "contraction_selection.hpp"
#include "contraction_solution.hpp"
#include "contraction_solution_registry.hpp"
#include "handle.hpp"
#include "hip_device.hpp"
#include "hiptensor.hpp"

hiptensorStatus_t hiptensorInitContractionDescriptor(const hiptensorHandle_t*           handle,
                                                     hiptensorContractionDescriptor_t*  desc,
                                                     const hiptensorTensorDescriptor_t* descA,
                                                     const int32_t                      modeA[],
                                                     const uint32_t alignmentRequirementA,
                                                     const hiptensorTensorDescriptor_t* descB,
                                                     const int32_t                      modeB[],
                                                     const uint32_t alignmentRequirementB,
                                                     const hiptensorTensorDescriptor_t* descC,
                                                     const int32_t                      modeC[],
                                                     const uint32_t alignmentRequirementC,
                                                     const hiptensorTensorDescriptor_t* descD,
                                                     const int32_t                      modeD[],
                                                     const uint32_t         alignmentRequirementD,
                                                     hiptensorComputeType_t typeCompute)

{
    if(!handle || !desc || !descA || !descB || !descD)
    {
        return HIPTENSOR_STATUS_NOT_INITIALIZED;
    }

    int32_t contractionOp;
    if(descC == nullptr || modeC == nullptr)
    {
        // C-descriptor is empty
        *desc = {(int32_t)hiptensor::ContractionOpId_t::SCALE,
                 typeCompute,
                 {*descA,
                  *descB,
                  {(hipDataType)-1, {descD->mLengths.size(), 0}, {descD->mStrides.size(), 0}},
                  *descD},
                 {alignmentRequirementA, alignmentRequirementB, 0, alignmentRequirementD}};
    }
    else
    {
        // C-descriptor is not empty
        *desc = {(int32_t)hiptensor::ContractionOpId_t::BILINEAR,
                 typeCompute,
                 {*descA, *descB, *descC, *descD},
                 {alignmentRequirementA,
                  alignmentRequirementB,
                  alignmentRequirementC,
                  alignmentRequirementD}};
    }

    return HIPTENSOR_STATUS_SUCCESS;
}

hiptensorStatus_t hiptensorInitContractionFind(const hiptensorHandle_t*    handle,
                                               hiptensorContractionFind_t* find,
                                               const hiptensorAlgo_t       algo)
{
    if(handle == nullptr || find == nullptr)
    {
        return HIPTENSOR_STATUS_NOT_INITIALIZED;
    }

    auto realHandle = hiptensor::Handle::toHandle((int64_t*)handle->fields);

    // Ensure current HIP device is same as the handle.
    hiptensor::HipDevice currentDevice;
    if((int)currentDevice.getDeviceId() != realHandle->getDevice().getDeviceId())
    {
        return HIPTENSOR_STATUS_ARCH_MISMATCH;
    }

    if(algo == HIPTENSOR_ALGO_DEFAULT || algo == HIPTENSOR_ALGO_DEFAULT_PATIENT
       || algo == HIPTENSOR_ALGO_ACTOR_CRITIC)
    {
        // Update the stored selection algorithm
        find->mSelectionAlgorithm = algo;

        // For now, enumerate all known contraction kernels.
        // Using the hipDevice, determine if the device supports F64
        auto& registry = hiptensor::ContractionSolutionRegistry::instance();
        auto  query    = registry->allSolutions();

        // Check if the current device supports F64
        if(!currentDevice.supportsF64())
        {
            // Allow only supported f32 combos
            query = query.query(HIP_R_32F, HIP_R_32F, HIP_R_32F, HIP_R_32F) // Bilinear F32
                    || query.query(
                        HIP_R_32F, HIP_R_32F, hipDataType(-1), HIP_R_32F); // Scale F32 (no C)
        }

        // Can do more checking for scale / bilinear, etc. if we need to.

        if(query.solutionCount() == 0)
        {
            // No kernels found!
            return HIPTENSOR_STATUS_INTERNAL_ERROR;
        }

        // Retrieve the solution map
        auto& solutions = query.solutions();

        // Extract the solutions to the candidates vector.
        find->mCandidates.resize(solutions.size());
        transform(solutions.begin(), solutions.end(), find->mCandidates.begin(), [](auto p) {
            return (void*)p.second;
        });

        return HIPTENSOR_STATUS_SUCCESS;
    }
    else
    {
        return HIPTENSOR_STATUS_INVALID_VALUE;
    }
}

hiptensorStatus_t hiptensorContractionGetWorkspaceSize(const hiptensorHandle_t* handle,
                                                       const hiptensorContractionDescriptor_t* desc,
                                                       const hiptensorContractionFind_t*       find,
                                                       const hiptensorWorksizePreference_t     pref,
                                                       uint64_t* workspaceSize)
{
    if(handle == nullptr || desc == nullptr || find == nullptr || workspaceSize == nullptr)
    {
        return HIPTENSOR_STATUS_NOT_INITIALIZED;
    }

    // NOTE: Here, ck::index_t is int, NOT same as std::index_t = long uint
    // Therefore the conversion to ck::index_t is required.
    auto toCKVec
        = [](auto& inputVec) { return std::vector<ck::index_t>(inputVec.begin(), inputVec.end()); };

    auto a_ms_ks_lengths = toCKVec(desc->mTensorDesc[0].mLengths);
    auto a_ms_ks_strides = toCKVec(desc->mTensorDesc[0].mStrides);
    auto b_ns_ks_lengths = toCKVec(desc->mTensorDesc[1].mLengths);
    auto b_ns_ks_strides = toCKVec(desc->mTensorDesc[1].mStrides);
    auto d_ms_ns_lengths = toCKVec(desc->mTensorDesc[2].mLengths);
    auto d_ms_ns_strides = toCKVec(desc->mTensorDesc[2].mStrides);
    auto e_ms_ns_lengths = toCKVec(desc->mTensorDesc[3].mLengths);
    auto e_ms_ns_strides = toCKVec(desc->mTensorDesc[3].mStrides);

    *workspaceSize = 0u;

    // No mem alloc, just need to init sizes to determine workspace req.
    float alpha, beta;
    void *A_d, *B_d, *C_d;
    for(auto* candidate : find->mCandidates)
    {
        auto* solution = (hiptensor::ContractionSolution*)candidate;
        if(solution->initArgs(&alpha,
                              A_d,
                              B_d,
                              &beta,
                              C_d,
                              C_d,
                              a_ms_ks_lengths,
                              a_ms_ks_strides,
                              b_ns_ks_lengths,
                              b_ns_ks_strides,
                              std::vector<std::vector<ck::index_t>>{d_ms_ns_lengths},
                              std::vector<std::vector<ck::index_t>>{d_ms_ns_strides},
                              e_ms_ns_lengths,
                              e_ms_ns_strides,
                              nullptr))
        {
            if(*workspaceSize == 0)
            {
                *workspaceSize = solution->workspaceSize();
            }
            else
            {
                if(pref == HIPTENSOR_WORKSPACE_MIN)
                {
                    *workspaceSize = std::min(*workspaceSize, solution->workspaceSize());
                }
                else
                {
                    *workspaceSize = std::max(*workspaceSize, solution->workspaceSize());
                }
            }
        }
    }

    return HIPTENSOR_STATUS_SUCCESS;
}

hiptensorStatus_t hiptensorInitContractionPlan(const hiptensorHandle_t*                handle,
                                               hiptensorContractionPlan_t*             plan,
                                               const hiptensorContractionDescriptor_t* desc,
                                               const hiptensorContractionFind_t*       find,
                                               const uint64_t workspaceSize)
{
    if(handle == nullptr || plan == nullptr || desc == nullptr || find == nullptr)
    {
        return HIPTENSOR_STATUS_NOT_INITIALIZED;
    }

    auto realHandle = hiptensor::Handle::toHandle((int64_t*)handle->fields);

    // Ensure current HIP device is same as the handle.
    hiptensor::HipDevice currentDevice;
    if((int)currentDevice.getDeviceId() != realHandle->getDevice().getDeviceId())
    {
        return HIPTENSOR_STATUS_ARCH_MISMATCH;
    }

    // At this point, we need to format inputs for kernels as they will be tested via selection model.
    // Brute force method currently uses CK kernel format, so we will adjust inputs to that style.

    // Convert to concrete contraction solutions
    auto candidates = std::vector<hiptensor::ContractionSolution*>(find->mCandidates.size());
    transform(find->mCandidates.begin(), find->mCandidates.end(), candidates.begin(), [](auto* p) {
        return (hiptensor::ContractionSolution*)p;
    });

    // Query contraction solutions for the correct contraction operation
    auto solutionMap = hiptensor::ContractionSolutionRegistry::Query{candidates}
                           .query((hiptensor::ContractionOpId_t)desc->mContractionOpId)
                           .solutions();
    candidates.resize(solutionMap.size());
    transform(solutionMap.begin(), solutionMap.end(), candidates.begin(), [](auto p) {
        return (hiptensor::ContractionSolution*)p.second;
    });

    // NOTE: Here, ck::index_t is int, NOT same as std::index_t = long uint
    // Therefore the conversion to ck::index_t is required.
    auto toCKVec
        = [](auto& inputVec) { return std::vector<ck::index_t>(inputVec.begin(), inputVec.end()); };

    auto ADataType = desc->mTensorDesc[0].mType;
    auto BDataType = desc->mTensorDesc[1].mType;
    auto DDataType = desc->mTensorDesc[2].mType;
    auto EDataType = desc->mTensorDesc[3].mType;

    auto a_ms_ks_lengths = toCKVec(desc->mTensorDesc[0].mLengths);
    auto a_ms_ks_strides = toCKVec(desc->mTensorDesc[0].mStrides);
    auto b_ns_ks_lengths = toCKVec(desc->mTensorDesc[1].mLengths);
    auto b_ns_ks_strides = toCKVec(desc->mTensorDesc[1].mStrides);
    auto d_ms_ns_lengths = toCKVec(desc->mTensorDesc[2].mLengths);
    auto d_ms_ns_strides = toCKVec(desc->mTensorDesc[2].mStrides);
    auto e_ms_ns_lengths = toCKVec(desc->mTensorDesc[3].mLengths);
    auto e_ms_ns_strides = toCKVec(desc->mTensorDesc[3].mStrides);

    // Launch selection algorithm
    hiptensor::ContractionSolution* winner = nullptr;
    auto                            result = HIPTENSOR_STATUS_INTERNAL_ERROR;
    if(find->mSelectionAlgorithm == HIPTENSOR_ALGO_DEFAULT
       || find->mSelectionAlgorithm == HIPTENSOR_ALGO_DEFAULT_PATIENT)
    {
        result = hiptensor::bruteForceModel(&winner,
                                            candidates,
                                            ADataType,
                                            a_ms_ks_lengths,
                                            a_ms_ks_strides,
                                            BDataType,
                                            b_ns_ks_lengths,
                                            b_ns_ks_strides,
                                            DDataType,
                                            d_ms_ns_lengths,
                                            d_ms_ns_strides,
                                            EDataType,
                                            e_ms_ns_lengths,
                                            e_ms_ns_strides,
                                            workspaceSize);
    }
    else if(find->mSelectionAlgorithm == HIPTENSOR_ALGO_ACTOR_CRITIC)
    {
        result = hiptensor::actorCriticModel(&winner,
                                             solutionMap,
                                             ADataType,
                                             a_ms_ks_lengths,
                                             a_ms_ks_strides,
                                             BDataType,
                                             b_ns_ks_lengths,
                                             b_ns_ks_strides,
                                             DDataType,
                                             d_ms_ns_lengths,
                                             d_ms_ns_strides,
                                             EDataType,
                                             e_ms_ns_lengths,
                                             e_ms_ns_strides,
                                             workspaceSize);
    }

    if(result != HIPTENSOR_STATUS_SUCCESS)
    {
        return result;
    }

    // Assign the contraction descriptor
    plan->mContractionDesc = *desc;
    plan->mSolution        = winner;

    return HIPTENSOR_STATUS_SUCCESS;
}

hiptensorStatus_t hiptensorContraction(const hiptensorHandle_t*          handle,
                                       const hiptensorContractionPlan_t* plan,
                                       const void*                       alpha,
                                       const void*                       A,
                                       const void*                       B,
                                       const void*                       beta,
                                       const void*                       C,
                                       void*                             D,
                                       void*                             workspace,
                                       uint64_t                          workspaceSize,
                                       hipStream_t                       stream)
{
    if(handle == nullptr || plan == nullptr)
    {
        return HIPTENSOR_STATUS_NOT_INITIALIZED;
    }

    if(alpha == nullptr || A == nullptr || B == nullptr || D == nullptr)
    {
        return HIPTENSOR_STATUS_INVALID_VALUE;
    }

    if(plan->mSolution == nullptr)
    {
        return HIPTENSOR_STATUS_INTERNAL_ERROR;
    }

    auto realHandle = hiptensor::Handle::toHandle((int64_t*)handle->fields);

    // Ensure current HIP device is same as the handle.
    hiptensor::HipDevice currentDevice;
    if((int)currentDevice.getDeviceId() != realHandle->getDevice().getDeviceId())
    {
        return HIPTENSOR_STATUS_ARCH_MISMATCH;
    }

    auto* cSolution = (hiptensor::ContractionSolution*)(plan->mSolution);

    // NOTE: Here, ck::index_t is int, NOT same as std::index_t = long uint
    // Therefore the conversion to ck::index_t is required.
    auto toCKVec
        = [](auto& inputVec) { return std::vector<ck::index_t>(inputVec.begin(), inputVec.end()); };

    auto a_ms_ks_lengths = toCKVec(plan->mContractionDesc.mTensorDesc[0].mLengths);
    auto a_ms_ks_strides = toCKVec(plan->mContractionDesc.mTensorDesc[0].mStrides);

    auto b_ns_ks_lengths = toCKVec(plan->mContractionDesc.mTensorDesc[1].mLengths);
    auto b_ns_ks_strides = toCKVec(plan->mContractionDesc.mTensorDesc[1].mStrides);

    auto d_ms_ns_lengths = toCKVec(plan->mContractionDesc.mTensorDesc[2].mLengths);
    auto d_ms_ns_strides = toCKVec(plan->mContractionDesc.mTensorDesc[2].mStrides);

    auto e_ms_ns_lengths = toCKVec(plan->mContractionDesc.mTensorDesc[3].mLengths);
    auto e_ms_ns_strides = toCKVec(plan->mContractionDesc.mTensorDesc[3].mStrides);

    auto canRun = cSolution->initArgs(alpha,
                                      A,
                                      B,
                                      beta,
                                      C,
                                      D,
                                      a_ms_ks_lengths,
                                      a_ms_ks_strides,
                                      b_ns_ks_lengths,
                                      b_ns_ks_strides,
                                      std::vector<std::vector<ck::index_t>>{d_ms_ns_lengths},
                                      std::vector<std::vector<ck::index_t>>{d_ms_ns_strides},
                                      e_ms_ns_lengths,
                                      e_ms_ns_strides,
                                      workspace);

    if(canRun)
    {
        if(cSolution->workspaceSize() > workspaceSize)
        {
            return HIPTENSOR_STATUS_INSUFFICIENT_WORKSPACE;
        }

        (*cSolution)(StreamConfig{stream, false});
        return HIPTENSOR_STATUS_SUCCESS;
    }
    else
    {
        return HIPTENSOR_STATUS_INTERNAL_ERROR;
    }
}