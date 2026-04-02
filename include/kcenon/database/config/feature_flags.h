// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file feature_flags.h
 * @brief Unified feature flags header for database_system
 *
 * This header provides normalized KCENON_* macros for integration detection
 * in the database_system library. It includes common_system's feature_flags.h
 * when available and provides compatibility aliases.
 *
 * Usage:
 * @code
 * #include <kcenon/database/config/feature_flags.h>
 *
 * #if KCENON_HAS_COMMON_SYSTEM
 *     #include <kcenon/common/patterns/result.h>
 *     // Use common_system Result<T>
 * #else
 *     // Use local fallback
 * #endif
 * @endcode
 *
 * @see kcenon/common_system#223 for unified feature-flag consolidation
 */

#pragma once

//==============================================================================
// Include common_system feature flags if available
//==============================================================================

#if __has_include(<kcenon/common/config/feature_flags.h>)
    #include <kcenon/common/config/feature_flags.h>
#endif

//==============================================================================
// common_system Integration Flag
//==============================================================================

/**
 * @brief Enable integration with common_system module
 *
 * When enabled, Result<T>, IDatabase interface, and ILogger interface
 * from common_system are available.
 *
 * This flag is typically set via CMake:
 * - BUILD_WITH_COMMON_SYSTEM (legacy, CMake option)
 * - USE_COMMON_SYSTEM (legacy, alternative name)
 *
 * The normalized name is KCENON_HAS_COMMON_SYSTEM.
 */
#ifndef KCENON_HAS_COMMON_SYSTEM
    #if defined(BUILD_WITH_COMMON_SYSTEM) || defined(USE_COMMON_SYSTEM)
        #define KCENON_HAS_COMMON_SYSTEM 1
    #else
        #define KCENON_HAS_COMMON_SYSTEM 0
    #endif
#endif

//==============================================================================
// Legacy Aliases (for backward compatibility)
//==============================================================================

/**
 * @brief Legacy alias definitions for backward compatibility
 *
 * These ensure that code using the old macro names continues to work.
 * New code should use KCENON_HAS_COMMON_SYSTEM.
 *
 * @note Legacy aliases are planned for deprecation. Migrate to KCENON_* macros.
 */
#if KCENON_HAS_COMMON_SYSTEM
    // Ensure legacy macros are defined for backward compatibility
    #ifndef BUILD_WITH_COMMON_SYSTEM
        #define BUILD_WITH_COMMON_SYSTEM 1
    #endif
    #ifndef USE_COMMON_SYSTEM
        #define USE_COMMON_SYSTEM 1
    #endif
#endif
