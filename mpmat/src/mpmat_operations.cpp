//
// Created by Petr Kravchuk on 8/14/17.
//

#include "mpmat.h"
#include <gmpxx.h>
#include <math.h>
#include <cassert>
#include <iostream>
#include <mkl.h>
#include "Timers.h"

template <typename T>
inline T ceil_div(T a, T b) {
    return a / b + ( (a % b != 0) & (a > 0) );
}

void mpmatMultiplyGMPBaseCase(mpf_class & dest,
                              const mpf_class  a,
                              const mpf_class  b) {

    int mpmat_limb = MPMAT_DOUBLE_MANT_IMPLICIT / 2;
    int mpmat_size_a = ceil_div( abs(a.get_mpf_t()->_mp_size) * mp_bits_per_limb, mpmat_limb );
    int mpmat_size_b = ceil_div( abs(b.get_mpf_t()->_mp_size) * mp_bits_per_limb, mpmat_limb );

    while ( 2 * mpmat_limb + ceil(log2(fmin(mpmat_size_a, mpmat_size_b))) > MPMAT_DOUBLE_MANT_IMPLICIT ) {
        mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(fmin(mpmat_size_a, mpmat_size_b))) ) / 2;
        mpmat_size_a = ceil_div( abs(a.get_mpf_t()->_mp_size) * mp_bits_per_limb, mpmat_limb );
        mpmat_size_b = ceil_div( abs(b.get_mpf_t()->_mp_size) * mp_bits_per_limb, mpmat_limb );
    }

    int mpmat_size_c = fmin(mpmat_size_a, mpmat_size_b);

    assert(mpmat_limb > 0);

    mpmat_double * a_double = new mpmat_double[mpmat_size_a];
    mpmat_double * b_double = new mpmat_double[mpmat_size_b];
    mpmat_double * c_double = new mpmat_double[mpmat_size_c];

    int a_mp_exp = a.get_mpf_t() -> _mp_exp * mp_bits_per_limb;
    int b_mp_exp = b.get_mpf_t() -> _mp_exp * mp_bits_per_limb;

    mpmatConvertGMPToDouble(a,a_double,mpmat_size_a, mpmat_limb, a_mp_exp);
    mpmatConvertGMPToDouble(b,b_double,mpmat_size_b, mpmat_limb, b_mp_exp);

    for(int i = 0; i< mpmat_size_c; i++) {
        c_double[i]=0;
        for(int k = 0; k<=i; k++) {
            c_double[i] += a_double[i-k] * b_double[k];
        }
    }

    mpmatConvertDoubleToGMP(dest, c_double, mpmat_size_c,mpmat_limb, a_mp_exp+b_mp_exp-mpmat_limb);

    delete [] a_double;
    delete [] b_double;
    delete [] c_double;
}

template <typename T>
inline T max (T a,T b) { return a>b ? a : b; }

// Test for row-major matrices, not optimzied
void cblas_dgemm_emulator(const int m,
                          const int n,
                          const int k,
                          const mpmat_double * a,
                          const mpmat_double * b,
                          mpmat_double * c){
    for (int ic = 0; ic < m; ic++) {
        for(int jc = 0; jc < n; jc++) {
            for(int l = 0; l < k; l++) {
                c[ n*ic + jc ] += a[ k*ic + l ] * b[ n*l + jc ];
            }
        }
    }
}


void mpmat_gemm_reduced(
        const CBLAS_LAYOUT Layout,
        //const CBLAS_TRANSPOSE transa,
        //const CBLAS_TRANSPOSE transb,
        const int m,
        const int n,
        const int k,
        //const mpf_class alpha,
        const mpf_class * a,
        //const int lda,
        const mpf_class * b,
        //const int ldb,
        //const mpf_class beta,
        mpf_class * c
        //const int ldc
        ) {

    timers["mpmat_gemm_reduced.complete"].start();

    int mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k)) )/ 2;
    int mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
    int mpmat_size_b = ceil_div( abs(b[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );

    while ( 2 * mpmat_limb + ceil(log2(k+fmin(mpmat_size_a, mpmat_size_b))) > MPMAT_DOUBLE_MANT_IMPLICIT ) {
        mpmat_limb = ( MPMAT_DOUBLE_MANT_IMPLICIT - ceil(log2(k+fmin(mpmat_size_a, mpmat_size_b))) ) / 2;
        mpmat_size_a = ceil_div( abs(a[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
        mpmat_size_b = ceil_div( abs(b[0].get_mpf_t()->_mp_prec+1) * mp_bits_per_limb, mpmat_limb );
    }

    int mpmat_size_c = fmin(mpmat_size_a, mpmat_size_b);

    std::cout << "Allocating double sizes " << mpmat_size_a << " " << mpmat_size_b << " " << mpmat_size_c << std::endl;
    std::cout << mpmat_size_a * m * k << std::endl;
    std::cout << mpmat_size_b * n * k << std::endl;
    std::cout << mpmat_size_c * m * n << std::endl;
    std::flush(std::cout);

    int mem_a = mpmat_size_a * m * k;
    int mem_b = mpmat_size_b * n * k;
    int mem_c = mpmat_size_c * m * n;

    auto a_double_array = new mpmat_double [mem_a];
    auto b_double_array = new mpmat_double [mem_b];

    auto c_double_array = new mpmat_double [mem_c];

    auto tmp            = new mpmat_double [ max( max(mem_a,mem_b), mem_c) ];

    memset(c_double_array, 0, mpmat_size_c * m *n * sizeof(mpmat_double));

    int expa, expb;

    std::cout << "Converting a to double" << std::endl;
    std::flush(std::cout);
    mpmatConvertGMPToDoubleVector(
            a,
            m * k,
            a_double_array,
            mpmat_size_a,
            mpmat_limb,
            expa,
            tmp
    );

    std::cout << "Converting b to double" << std::endl;
    std::flush(std::cout);
    mpmatConvertGMPToDoubleVector(
            b,
            n * k,
            b_double_array,
            mpmat_size_b,
            mpmat_limb,
            expb,
            tmp
    );

    timers["mpmat_gemm_reduced.multiplication"].start();

    std::cout << "Computing the product" << std::endl;
    std::flush(std::cout);
    for (int i = 0; i < mpmat_size_c; i++) {
        for (int j = 0; j <= i; j++) {
            cblas_dgemm(
                    Layout,
                    CblasNoTrans,
                    CblasNoTrans,
                    m,
                    n,
                    k,
                    1,
                    a_double_array+k*m*j,
                    Layout == CblasRowMajor ? k : m,
                    b_double_array+(i-j)*k*n,
                    Layout == CblasRowMajor ? n : k,
                    1,
                    c_double_array+i*m*n,
                    Layout == CblasRowMajor ? n : m
            );
        }
    }

    timers["mpmat_gemm_reduced.multiplication"].stop();

    std::cout << "Converting back" << std::endl;
    mpmatConvertDoubleToGMPVector(
            c,
            m*n,
            c_double_array,
            mpmat_size_c,
            mpmat_limb,
            expa+expb-mpmat_limb,
            tmp
    );

    delete [] a_double_array;
    delete [] b_double_array;
    delete [] c_double_array;
    delete [] tmp;

    timers["mpmat_gemm_reduced.complete"].stop();
}