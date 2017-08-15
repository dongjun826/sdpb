//
// Created by Petr Kravchuk on 8/10/17.
//

#include "mpmat.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <mkl.h>
#include "Timers.h"

#include <iostream>
#include <bitset>

//using namespace std;

const int mp_limb_bits = sizeof(mp_limb_t) * 8;

template <typename T>
inline T ceil_div(T a, T b){
    return a / b + ( (a % b != 0) & (a > 0) );
}

//bitset<64> print64bits(mp_limb_t a){
//    return bitset<64>(static_cast<long long>(a));
//}

// Converts a pair of mp_limb_t to mpmat_double limb
// Arguments:
// (in)  src        : pointer to the higher limb; src-1 should be accessible
// (in)  highoffset : the number of high bits of the higher limb to ignore
// (in)  mpmat_limb : the size of mpmat_double limb in bits
// Returns: mpmat_double representing mpmat_limb bits of the result
//
inline mpmat_double doubleFromLimbs(const mp_limb_t * src, const int highoffset, const int mpmat_limb) {

    //std::cout << "doubleFromLimbs( [" << print64bits(*src) << ", " << print64bits(*(src-1)) << "] , " << highoffset << ", " << mpmat_limb << ")" << std::endl;

    mp_limb_t t;
    t = ( (*src)<<highoffset ) >> (mp_limb_bits - mpmat_limb);
    //cout << "    precast value : " << print64bits(t) << endl;
    // Shifts by more than type size are undefined behavior, so need to check
    if ( (2*mp_limb_bits - highoffset - mpmat_limb) < mp_limb_bits ) {
        t|= ( *(src-1) ) >> (2*mp_limb_bits - highoffset - mpmat_limb);
    }
    //cout << "    precast value : " << print64bits(t) << endl;
    return static_cast<mpmat_double>(t);
}

// Same as above, but assumes the lower limb to be 0, only src should be reachable
inline mpmat_double doubleFromHighLimb(const mp_limb_t * src, const int highoffset, const int mpmat_limb) {
    //std::cout << "doubleFromHighLimb( [" << print64bits(*src) << ", " << print64bits(0) << "] , " << highoffset << ", " << mpmat_limb << ")" << std::endl;
    mp_limb_t t;
    t = ( (*src)<<highoffset ) >> (mp_limb_bits - mpmat_limb);
    return static_cast<mpmat_double>(t);
}
// Same as above, but assumes the higher limb to be 0, only src-1 should be reachable
inline mpmat_double doubleFromLowLimb(const mp_limb_t * src, const int highoffset, const int mpmat_limb) {
    //std::cout << "doubleFromLowLimb( [" << print64bits(0) << ", " << print64bits(*(src-1)) << "] , " << highoffset << ", " << mpmat_limb << ")" << std::endl;
    mp_limb_t t=0;
    if ( (2*mp_limb_bits - highoffset - mpmat_limb) < mp_limb_bits ) {
        t|= ( *(src-1) ) >> (2*mp_limb_bits - highoffset - mpmat_limb);
    }
    return static_cast<mpmat_double>(t);
}

// Description in .h
void mpmatConvertGMPToDouble(const mpf_class source,
                             mpmat_double * dest,
                             const int size,
                             const int mpmat_limb,
                             const int exp) {

    int mp_size    = abs(source.get_mpf_t()->_mp_size);
    int mp_bit_exp = source.get_mpf_t()->_mp_exp * mp_limb_bits;

    int mp_sign    = source.get_mpf_t()->_mp_size > 0 ? 1 : -1;

    const mp_limb_t * mp_d = source.get_mpf_t()->_mp_d;

    //Check input
    assert(mp_bit_exp <= exp);
    int pad_exp = exp - mp_bit_exp;

    //Initialize by zeroes (assuming something like IEEE754)
    std::memset(dest, 0, size*sizeof(mpmat_double));

    int mpmat_pos  = pad_exp / mpmat_limb;
    if (mpmat_pos >= size) return; //No significant bits can be extracted

    int highoffset = mp_limb_bits - (pad_exp - mpmat_pos * mpmat_limb);
    int mpf_pos    = mp_size-(highoffset / mp_limb_bits);

    highoffset %= mp_limb_bits;

    if (highoffset > 0) {
        //std::cout << "mpmat_pos == " << mpmat_pos << ", mpf_pos == " << mpf_pos << std::endl;
        dest[mpmat_pos] = mp_sign * doubleFromLowLimb(mp_d + mpf_pos, highoffset, mpmat_limb);

        mpmat_pos += 1;
        mpf_pos    = mp_size - 1 - ( mpmat_pos * mpmat_limb - pad_exp ) / mp_limb_bits;
        highoffset = mpmat_pos*mpmat_limb - pad_exp - ( (mp_size-1-mpf_pos)*mp_limb_bits );
    }

    while (mpmat_pos < size && mpf_pos > 0) {
        //std::cout << "mpmat_pos == " << mpmat_pos << ", mpf_pos == " << mpf_pos << std::endl;
        dest[mpmat_pos] = mp_sign * doubleFromLimbs(mp_d + mpf_pos, highoffset, mpmat_limb);

        mpmat_pos += 1;
        mpf_pos    = mp_size - 1 - ( mpmat_pos * mpmat_limb - pad_exp ) / mp_limb_bits;
        highoffset = mpmat_pos*mpmat_limb - pad_exp - ( (mp_size-1-mpf_pos)*mp_limb_bits );
    }

    if (mpf_pos == 0 && mpmat_pos < size)
    {
        while (mpf_pos == 0 && mpmat_pos < size) {
            //std::cout << "mpmat_pos == " << mpmat_pos << ", mpf_pos == " << mpf_pos << std::endl;
            dest[mpmat_pos] = mp_sign * doubleFromHighLimb(mp_d + mpf_pos, highoffset, mpmat_limb);

            mpmat_pos += 1;
            mpf_pos = mp_size - 1 - (mpmat_pos * mpmat_limb - pad_exp) / mp_limb_bits;
            highoffset = mpmat_pos * mpmat_limb - pad_exp - ((mp_size - 1 - mpf_pos) * mp_limb_bits);
        }
    }
}

// Copies the least significant bits of a number to a pair of limbs, with offset
//
// Arguments:
// (out) l      : the lower limb of the destination; l and l+1 should be reachable
// (in)  src    : the source number
// (in)  offset : offset from the least significant limb
// (in)  bits   : number of bits to copy
//
// * assumes that the target bits are unset (0)
//
inline void dumpBitsToLimbs(mp_limb_t * l, const mp_limb_t src, int offset, int bits) {
    mp_limb_t  mask = ( (mp_limb_t(1) << bits) - 1 );
    *l |= (src & mask) << offset;
    if (bits + offset > mp_limb_bits) {
        *(l + 1) |= (src & mask) >> (mp_limb_bits - offset);
    }
}

// Same as above, but doesn't affect the lower limb; only l+1 should be reachable
inline void dumpBitsToHighLimb(mp_limb_t * l, const mp_limb_t src, int offset, int bits) {
    mp_limb_t  mask = ( (mp_limb_t(1) << bits) - 1 );
    if (bits + offset > mp_limb_bits) {
        *(l + 1) |= (src & mask) >> (mp_limb_bits - offset);
    }
}

// Same as above, but doesn't affect the higher limb; only l should be reachable
inline void dumpBitsToLowLimb(mp_limb_t * l, const mp_limb_t src, int offset, int bits) {
    mp_limb_t  mask = ( (mp_limb_t(1) << bits) - 1 );
    *l |= (src & mask) << offset;
}

// Description in .h
void mpmatConvertDoubleToGMP(mpf_class & dest,
                             const mpmat_double * source,
                             const int size,
                             const int mpmat_limb,
                             const int exp) {
    int mp_exp = ceil_div(exp + MPMAT_DOUBLE_MANT_IMPLICIT - mpmat_limb + 1, mp_limb_bits);
    int pad    = mp_exp * mp_limb_bits - exp - MPMAT_DOUBLE_MANT_IMPLICIT + mpmat_limb - 1;

    // maximum bit size of the number, including the possible 1-bit overflow over MPMAT_DOUBLE_MANT_IMPLICIT and exp pad
    // it is equal to the position of the least significant bit of source in dest's _mp_d (starting from 1)
    int max_bit_size = (size-1) * mpmat_limb + MPMAT_DOUBLE_MANT_IMPLICIT + 1 + pad;

    // recall that gmp allocates _mp_prec + 1 bits
    int mp_size    = dest.get_mpf_t()->_mp_prec + 1;

    // assuming _mp_d is always pointing to the same place, i.e. _mp_d+_mp_prec is allocated
    mp_limb_t * mp_d = dest.get_mpf_t()->_mp_d;
    std::memset(mp_d, 0, static_cast<size_t>(mp_size)*sizeof(mp_limb_t));

    int mpf_pos, mpmat_pos;
    mpf_pos = mp_size - ceil_div(max_bit_size, mp_limb_bits);
    // Assert that the full double array fits into the mpf_class
    assert(mpf_pos>=0);
    mpmat_pos = size - 1;

    /*
    if ( mpf_pos < -1 ) {
        mpf_pos = -1;
        mpmat_pos = ceil_div(mp_size*mp_limb_bits - 1 - pad, mpmat_limb) - 1;
    } else {
        mpmat_pos = size - 1;
    }
    */

    int offset = mp_limb_bits - ( (MPMAT_DOUBLE_MANT_IMPLICIT + 1 + pad + mpmat_limb * mpmat_pos) % mp_limb_bits );
    offset = offset % mp_limb_bits;

    // Find the overall sign of the expression
    mp_limb_signed_t tmp = 0; //static_cast<mp_limb_signed_t>( source[mpmat_pos] );
    mp_limb_signed_t mp_sign = 0;
    for (int mpmat_pos_tmp = mpmat_pos; mpmat_pos_tmp >= 0; mpmat_pos_tmp --) {
        tmp >>= mpmat_limb;
        tmp += static_cast<mp_limb_signed_t>(source[mpmat_pos_tmp]);

        if(tmp != 0) mp_sign = tmp < 0 ? -1 : 1;
    }
    //mp_limb_signed_t mp_sign = tmp < 0 ? -1 : 1; //BUG! If tmp == 0 then this may give a wrong sign.

    tmp = 0;
    /* if ( mpf_pos == -1 ) {
        while(mpf_pos == -1 && mpmat_pos > 0) {
            tmp = mp_sign * static_cast<mp_limb_signed_t>( source[mpmat_pos] ); //SIGNS
            dumpBitsToHighLimb(mp_d + mpf_pos, *reinterpret_cast<mp_limb_t *>(&tmp), offset, mpmat_limb);

            mpmat_pos--;
            mpf_pos = mp_size - ceil_div(mpmat_pos * mpmat_limb + MPMAT_DOUBLE_MANT_IMPLICIT + 1 + pad, mp_limb_bits);
            offset = mp_limb_bits - ((MPMAT_DOUBLE_MANT_IMPLICIT + 1 + pad + mpmat_limb * mpmat_pos) % mp_limb_bits);
            offset = offset % mp_limb_bits;
        }
    } */

    while ( mpf_pos<mp_size-1 && mpmat_pos > 0 ) {
        tmp >>= mpmat_limb;
        tmp += mp_sign * static_cast<mp_limb_signed_t>( source[mpmat_pos] ); //SIGNS
        dumpBitsToLimbs(mp_d + mpf_pos, *reinterpret_cast<mp_limb_t*>(&tmp), offset, mpmat_limb);

        mpmat_pos--;
        mpf_pos = mp_size - ceil_div(mpmat_pos * mpmat_limb + MPMAT_DOUBLE_MANT_IMPLICIT + 1 + pad, mp_limb_bits);
        offset = mp_limb_bits - ((MPMAT_DOUBLE_MANT_IMPLICIT + 1 + pad  + mpmat_limb * mpmat_pos) % mp_limb_bits);
        offset = offset % mp_limb_bits;
    }

    if ( mpf_pos == mp_size-1 && mpmat_pos > 0 ) {
        while ( mpmat_pos >0 ) {
            tmp >>= mpmat_limb;
            tmp += mp_sign * static_cast<mp_limb_t>( source[mpmat_pos] ); //SIGNS
            dumpBitsToLowLimb(mp_d + mpf_pos, *reinterpret_cast<mp_limb_t*>(&tmp), offset, mpmat_limb);

            mpmat_pos--;
            mpf_pos = mp_size - ceil_div(mpmat_pos * mpmat_limb + MPMAT_DOUBLE_MANT_IMPLICIT + 1 + pad, mp_limb_bits);
            offset = mp_limb_bits - ((MPMAT_DOUBLE_MANT_IMPLICIT + 1 + pad + mpmat_limb * mpmat_pos) % mp_limb_bits);
            offset = offset % mp_limb_bits;
        }
    }

    // Here mpmat_pos == 0

    tmp >>= mpmat_limb;
    tmp  += mp_sign * static_cast<mp_limb_t>( source[0] );
    if ( mpf_pos == mp_size - 1 ) {
        dumpBitsToLowLimb(mp_d + mpf_pos, *reinterpret_cast<mp_limb_t*>(&tmp), offset, MPMAT_DOUBLE_MANT_IMPLICIT + 1);
    } else {
        dumpBitsToLimbs(mp_d + mpf_pos, *reinterpret_cast<mp_limb_t*>(&tmp), offset, MPMAT_DOUBLE_MANT_IMPLICIT + 1);
    }

    dest.get_mpf_t()->_mp_exp  = mp_exp;
    dest.get_mpf_t()->_mp_size = mp_sign * mp_size;

    if ( mp_d[mp_size - 1] == 0 ) {
        int i = 1;
        while (mp_d[mp_size - 1 - i] == 0 && i < mp_size) i++;
        if ( i < mp_size ) {
            // Truncate number by reducing _mp_size and shift _mp_exp accordingly
            dest.get_mpf_t()->_mp_size = mp_sign * (mp_size - i);
            dest.get_mpf_t()->_mp_exp -= i;
        } else {
            // The number is simply zero
            dest.get_mpf_t()->_mp_size = 0;
            dest.get_mpf_t()->_mp_exp  = 0;
        }
    }
}

/*
// Description in .h
void mpmatConvertDoubleToGMP(mpf_class & dest,
                             const mpmat_double * source,
                             const int size,
                             const int mpmat_limb,
                             const int exp) {
    // First pass: determine the sign by finding the sign of the most significant non-zero partial sum
    mp_limb_signed_t tmp=0;
    mp_limb_signed_t mp_sign = 0;
    for (int i = size-1; i>=0; i--){
        // Here we use that right shifts of signed 2's complement integers give floor[tmp*2^{-mpmat_limb}]
        // As well as floor[tmp*2^{-mpmat_limb}] == floor[floor[tmp]*2^{-mpmat_limb}]
        tmp >>= mpmat_limb;
        tmp += static_cast<mp_limb_signed_t>( source[i] );
        if (tmp != 0) mp_sign = tmp > 0 ? +1 : -1;
    }

    // If mp_sign is zero, the number is zero, set _mp_size = _mp_exp = 0 and return
    if (mp_sign == 0) {
        dest.get_mpf_t()->_mp_exp=0;
        dest.get_mpf_t()->_mp_size=0;
        return;
    }

    // Second pass: taking the sign into account, find the most significant mpmat_limb which is non-zero
    tmp = 0;
    mp_limb_t mask = (mp_limb_t(1) << mp_limb_bits) - 1;
    int most_significant_mpmat_limb = size;
    for (int i = size-1; i>=0; i--){
        // Here we use that right shifts of signed 2's complement integers give floor[tmp*2^{-mpmat_limb}]
        // As well as floor[tmp*2^{-mpmat_limb}] == floor[floor[tmp]*2^{-mpmat_limb}]
        tmp >>= mpmat_limb;
        tmp += mp_sign * static_cast<mp_limb_signed_t>( source[i] );
        if ( *reinterpret_cast<mp_limb_t*>(&tmp) & mask !=0 ) most_significant_mpmat_limb = i;
    }

    int mp_exp = ceil_div(exp + MPMAT_DOUBLE_MANT_IMPLICIT - mpmat_limb + 1, mp_limb_bits);
    int pad    = mp_exp * mp_limb_bits - exp - MPMAT_DOUBLE_MANT_IMPLICIT + mpmat_limb - 1;

    // If tmp != 0 then there are essentially no special cancellations and we can procceed alomst as before
    if (tmp != 0) {
        assert(tmp>0); // We know it should be positive, just in case we made a mistake
        mp_limb_t leading_sum = static_cast<mp_limb_t>(tmp);


        if (pad + 1 + MPMAT_DOUBLE_MANT_IMPLICIT > mp_bits_per_limb) {
            // We are padding with at most 63 bits to align with gmp 64x exponents. This often results in adding a zero limb
            // It this condition is satisfied, there are bits of leading_sum outside of the leading limb, and so the above
            // situation can happen. We check whether it is the case.
            if ( leading_sum >> ( pad + 1 + MPMAT_DOUBLE_MANT_IMPLICIT - mp_bits_per_limb ) == 0 ) {
                pad -= mp_bits_per_limb;
            }
        }


    }


} */


// Description in .h
void mpmatConvertGMPToDoubleVector(const mpf_class * source,
                                   const int source_len,
                                   mpmat_double * dest,
                                   const int mpmat_size,
                                   const int mpmat_limb,
                                   int & exp,
                                   mpmat_double * tmp) {
    exp = source->get_mpf_t()->_mp_exp * mp_bits_per_limb;

    //mpmat_double * tmp = static_cast<mpmat_double *>( malloc(source_len * mpmat_size * sizeof(mpmat_double)) );

    //Make first pass to determine the exponent to be used
    for (int i = 1; i < source_len; i++) {
        int current_exp = source[i].get_mpf_t()->_mp_exp * mp_bits_per_limb;
        exp = current_exp > exp ? current_exp : exp;
    }

    //Make second pass to convert the GMPs to mpmat_doubles
    for (int i = 0; i < source_len; i++) {
        mpmatConvertGMPToDouble(source[i], tmp + i*mpmat_size, mpmat_size, mpmat_limb, exp);
    }

    timers["Transposition direct"].start();
    //Now transpose the array in-place to obtain limb-vectors
    /* mkl_dimatcopy(
            'r','t',
            source_len,
            mpmat_size,
            1,
            dest,
            mpmat_size,
            source_len
    ); */
    mkl_domatcopy(
            'r','t',
            source_len,
            mpmat_size,
            1,
            tmp,
            mpmat_size,
            dest,
            source_len
    );
    timers["Transposition direct"].stop();

    //free(tmp);
}

// Description in .h
void mpmatConvertDoubleToGMPVector(mpf_class * dest,
                                   const int dest_len,
                                   mpmat_double * source,
                                   const int mpmat_size,
                                   const int mpmat_limb,
                                   int exp,
                                   mpmat_double * tmp) {
    //mpmat_double * tmp = static_cast<mpmat_double *>( malloc(dest_len * mpmat_size * sizeof(mpmat_double)) );
    // Transpose out-of-place
    timers["Transposition reverse"].start();
    mkl_domatcopy(
            'r','t',
            mpmat_size,
            dest_len,
            1,
            source,
            dest_len,
            tmp,
            mpmat_size
    );
    timers["Transposition reverse"].stop();

    for (int i = 0; i < dest_len; i++) {
        mpmatConvertDoubleToGMP(
                dest[i],
                tmp + i * mpmat_size,
                mpmat_size,
                mpmat_limb,
                exp
        );
    }

    //free(tmp);
}
