#include <iostream>
#include <fstream>
#include <numeric>
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include "ht/ht_types.hpp"
#include "ht/ht_tensor.hpp"
#include "ht/ht_utility.hpp"

int main(int argc, char* argv[])
{

    //std::ofstream tensorA, tensorB, tensorC;

    typedef float ADataType;
    typedef float BDataType;
    typedef float CDataType;
    typedef float floatTypeCompute;


    hiptensorDataType_t typeA = HIPTENSOR_R_32F;
    hiptensorDataType_t typeB = HIPTENSOR_R_32F;
    hiptensorDataType_t typeC = HIPTENSOR_R_32F;
    hiptensorComputeType_t typeCompute = HIPTENSOR_COMPUTE_32F;

    floatTypeCompute alpha = (floatTypeCompute)1.0f;
    floatTypeCompute beta  = (floatTypeCompute)0.0f;

    std::cout << "RAND_MAX value is " << RAND_MAX << std::endl;

    /**********************
     * Computing: C_{m,n,u,v} = A_{m,n,h,k} B_{h,k,u,v}
     **********************/

    std::vector<int> modeC{'m','n','u','v'};
    std::vector<int> modeA{'m','n','h','k'};
    std::vector<int> modeB{'h','k','u','v'};


    int nmodeA = modeA.size();
    int nmodeB = modeB.size();
    int nmodeC = modeC.size();

    std::unordered_map<int, int64_t> extent;
    
    extent['m'] = 5;
    extent['n'] = 6;
    extent['u'] = 3;
    extent['v'] = 4;
    extent['h'] = 3;
    extent['k'] = 4;
    
    std::vector<int64_t> c_ms_ns_lengths;
    for (auto mode : modeC)
    	c_ms_ns_lengths.push_back(extent[mode]);
    std::vector<int64_t> a_ms_ks_lengths;
    for (auto mode : modeA)
       	a_ms_ks_lengths.push_back(extent[mode]);
    std::vector<int64_t> b_ks_ns_lengths;
    for (auto mode : modeB)
        b_ks_ns_lengths.push_back(extent[mode]);


    hiptensorHandle_t handle;
    hiptensorInit(&handle);
    
    /********************************************
     * Intialise Tensors with the input lengths *
     ********************************************/
    hiptensorTensorDescriptor_t a_ms_ks;
    hiptensorInitTensorDescriptor(&handle, &a_ms_ks, nmodeA, 
				a_ms_ks_lengths.data(), NULL,/*stride*/
				typeA, HIPTENSOR_OP_IDENTITY);

    std::cout << "a_ms_ks: ";
    a_ms_ks.hiptensorPrintTensorAttributes();
    std::cout << std::endl;
    
    hiptensorTensorDescriptor_t b_ks_ns;
    hiptensorInitTensorDescriptor(&handle, &b_ks_ns, nmodeB,
                       		b_ks_ns_lengths.data(), NULL,/*stride*/
				typeB, HIPTENSOR_OP_IDENTITY);
    
    std::cout << "b_ks_ns: ";
    b_ks_ns.hiptensorPrintTensorAttributes();
    std::cout << std::endl;
    
    hiptensorTensorDescriptor_t c_ms_ns;
    hiptensorInitTensorDescriptor(&handle, 
				&c_ms_ns, nmodeC,
				c_ms_ns_lengths.data(), NULL,/*stride*/
                      		typeC, HIPTENSOR_OP_IDENTITY);

    std::cout << "c_ms_ns: ";
    c_ms_ns.hiptensorPrintTensorAttributes(); 
    std::cout << std::endl;
    
    /**********************
     * Allocating data
     **********************/

    size_t elementsA = a_ms_ks.hiptensorGetElementSpace();
    size_t elementsB = b_ks_ns.hiptensorGetElementSpace();
    size_t elementsC = c_ms_ns.hiptensorGetElementSpace();

    size_t sizeA = sizeof(ADataType) * elementsA;
    size_t sizeB = sizeof(BDataType) * elementsB;
    size_t sizeC = sizeof(CDataType) * elementsC;

    ADataType *A = (ADataType*) malloc(sizeA);
    BDataType *B = (BDataType*) malloc(sizeB);
    CDataType *C = (CDataType*) malloc(sizeC);
    
    void *A_d, *B_d, *C_d;
    
    hip_check_error(hipMalloc(static_cast<void**>(&A_d), sizeA));
    hip_check_error(hipMalloc(static_cast<void**>(&B_d), sizeB));
    hip_check_error(hipMalloc(static_cast<void**>(&C_d), sizeC));

    /*******************
     * Initialize data
     *******************/
    for (int64_t i = 0; i < elementsA; i++)
        A[i] = ((float(std::rand()))/float(RAND_MAX) - 0.5)*100;
    for (int64_t i = 0; i < elementsB; i++)
        B[i] = ((float(std::rand()))/float(RAND_MAX) - 0.5)*100;	

    /********************************************
     * Transfer the Host Tensor to Device Memory *
     ********************************************/
    hip_check_error(hipMemcpy(A_d, static_cast<const void*>(A), sizeA, hipMemcpyHostToDevice));
    hip_check_error(hipMemcpy(B_d, static_cast<const void*>(B), sizeB, hipMemcpyHostToDevice));
    hip_check_error(hipMemset(C_d, 0, sizeC));
    
    /************************************************
     * Retrieve the memory alignment for each tensor
     ************************************************/ 
    uint32_t alignmentRequirementA;
    hiptensorGetAlignmentRequirement(&handle,
                          A_d, &a_ms_ks,
                          &alignmentRequirementA);
    std::cout << "Tensor A element space: " << alignmentRequirementA << std::endl;

    uint32_t alignmentRequirementB;
    hiptensorGetAlignmentRequirement(&handle,
                          B_d, &b_ks_ns,
                          &alignmentRequirementB);
    std::cout << "Tensor B element space: " << alignmentRequirementB << std::endl;

    uint32_t alignmentRequirementC;
    hiptensorGetAlignmentRequirement(&handle,
                          C_d, &c_ms_ns,
                          &alignmentRequirementC);
    std::cout << "Tensor C element space: " << alignmentRequirementC << std::endl;

    
    /*******************************
     * Create Contraction Descriptor
     *******************************/

    hiptensorContractionDescriptor_t desc;
    hiptensorInitContractionDescriptor(&handle,
                                    &desc,
                                    &a_ms_ks, modeA.data(), alignmentRequirementA,
                                    &b_ks_ns, modeB.data(), alignmentRequirementB,
                                    &c_ms_ns, modeC.data(), alignmentRequirementC,
                                    NULL, NULL, 0,
                                    typeCompute);
    /**************************
    * Set the algorithm to use
    ***************************/

    hiptensorContractionFind_t find;
    hiptensorInitContractionFind(&handle, 
                                &find,
                                HIPTENSOR_ALGO_DEFAULT);

   /**********************
    * Query workspace
    **********************/

    uint64_t worksize = 0;
    hiptensorContractionGetWorkspace(&handle,
                                    &desc,
                                    &find,
                                    HIPTENSOR_WORKSPACE_RECOMMENDED, &worksize);
    void *work = nullptr;
	
   /**************************
    * Create Contraction Plan
    **************************/

    hiptensorContractionPlan_t plan;
    hiptensorInitContractionPlan(&handle,
                                &plan,
                                &desc,
                                &find,
                                worksize);

    hiptensorContraction(&handle,
                       &plan,
                       (void*) &alpha, A_d, B_d,
                       (void*) &beta,  NULL, C_d,
                       work, worksize, 0 /* stream */);
    
	plan.hiptensorPrintContractionMetrics();
    hip_check_error(hipMemcpy(static_cast<void *>(C), C_d, sizeC, hipMemcpyDeviceToHost));
    
    std::cout<<"Tensor A elements:\n";
    std::copy(A, A + elementsA, std::ostream_iterator<ADataType>(std::cout, ","));
    std::cout<<std::endl;
    std::cout<<"Tensor B elements:\n";
    std::copy(B, B + elementsB, std::ostream_iterator<BDataType>(std::cout, ","));
    std::cout<<std::endl;
    std::cout<<"Tensor C elements:\n";
    std::copy(C, C + elementsC, std::ostream_iterator<BDataType>(std::cout, ","));
    std::cout<<std::endl;


#if 0
    tensorA.open("tensor_A.txt");
    LogRangeToFile<ADataType>(tensorA, a_ms_ks.ht_tensor.mData, ","); 
    LogRangeAsType<ADataType>(std::cout<<"Tensor A elements:\n", a_ms_ks.ht_tensor.mData,",");
    std::cout<<std::endl;
    tensorA.close();
    tensorB.open("tensor_B.txt");
    LogRangeToFile<BDataType>(tensorB, b_ks_ns.ht_tensor.mData, ","); 
    LogRangeAsType<BDataType>(std::cout<<"Tensor B elements:\n", b_ks_ns.ht_tensor.mData,",");
    std::cout<<std::endl;
    tensorB.close();
    tensorC.open("tensor_C_contraction_results.txt");
    LogRangeToFile<CDataType>(tensorC, c_ms_ns.ht_tensor.mData, ","); 
    LogRangeAsType<CDataType>(std::cout<<"Tensor C elements:\n", c_ms_ns.ht_tensor.mData, ","); 
    std::cout<<std::endl;
    tensorC.close();
#endif
	
    if (A) free(A);
    if (B) free(B);
    if (C) free(C);
    if (A_d) hip_check_error(hipFree(A_d));
    if (B_d) hip_check_error(hipFree(B_d));
    if (C_d) hip_check_error(hipFree(C_d));
    
    return 0;
}