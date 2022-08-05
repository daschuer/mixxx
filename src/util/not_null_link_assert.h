#pragma once

#ifdef GSL_POINTERS_H
#error "util/not_null_link_assert.h" before <gsl/pointers>
#endif
#define GSL_CONTRACTS_H // avoid using conflicting definitions of Expects and Ensures 

#include "util/assert.h"

/// Include this instead of <gsl/pointers> to assert at link time that the
/// gsl::not_mull pointer is not null

/// This function is intentionally not defined to produce a linker error
/// when the compiler keeps code branch for null pointers.
/// NOTE: Fix that by an explicit null check before reating the
/// gsl::not_null pointer
extern void link_assert_not_null_failed(void);
// Note void link_assert_not_null_failed(void) = delete; does not work,
// because it is evaluated before optimizing

// Precondition check, fails if required at runtime
#define Expects(a)                     \
    if (!(a)) {                        \
        link_assert_not_null_failed(); \
    }

// Remove Postcondition check, because Expects(a) above already asserts not null
#define Ensures(a)

#include <gsl/pointers>

#undef GSL_CONTRACTS_H
