#pragma once

#include <functional>
#include <optional>

// Pointers
#define uptr std::unique_ptr
#define sptr std::shared_ptr

#define try_exp(expr)                                                          \
  ({                                                                           \
    auto result_macro_ = (expr); /* Evaluate the expression once */            \
    if (!result_macro_) {                                                      \
      /* Return the error from the *enclosing function* */                     \
      return std::unexpected(result_macro_.error());                           \
    }                                                                          \
    /* If not an error, unwrap and return the value from the macro */          \
    *result_macro_; /* Dereference to get the contained value */               \
  })

#define get_opt(expr)                                                          \
  ({                                                                           \
    auto result_macro_ = (expr); /* Evaluate the expression once */            \
    if (!result_macro_) {                                                      \
      return {};                                                               \
    }                                                                          \
    /* If not an error, unwrap and return the value from the macro */          \
    std::move(*result_macro_); /* Dereference to get the contained value */    \
  })

#define try_opt(expr, error)                                                   \
  ({                                                                           \
    auto result_macro_ = (expr); /* Evaluate the expression once */            \
    if (!result_macro_) {                                                      \
      /* Return the error from the *enclosing function* */                     \
      return std::unexpected(error);                                           \
    }                                                                          \
    /* If not an error, unwrap and return the value from the macro */          \
    *result_macro_; /* Dereference to get the contained value */               \
  })
