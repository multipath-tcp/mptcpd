// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file export.h
 *
 * @brief mptcpd shared library symbol export/import macros.
 *
 * @internal The macros in this header are not part of the public
 *           mptcpd library API.
 *
 * Copyright (c) 2018, 2019, Intel Corporation
 */

#ifndef MPTCPD_EXPORT_H
#define MPTCPD_EXPORT_H

// Internal macros
#if __GNUC__ >= 4
#  define MPTCPD_DLL_IMPORT __attribute__((visibility ("default")))
#  define MPTCPD_DLL_EXPORT __attribute__((visibility ("default")))
#  define MPTCPD_DLL_LOCAL  __attribute__((visibility ("hidden")))
#else
#  define MPTCPD_DLL_IMPORT
#  define MPTCPD_DLL_EXPORT
#  define MPTCPD_DLL_LOCAL
#endif

#ifdef MPTCPD_DLL
#  ifdef MPTCPD_DLL_EXPORTS
#    define MPTCPD_API MPTCPD_DLL_EXPORT
#  else
#    define MPTCPD_API MPTCPD_DLL_IMPORT
#  endif
#  define MPTCPD_LOCAL MPTCPD_DLL_LOCAL
#else
#  define MPTCPD_API
#  define MPTCPD_LOCAL
#endif

#endif  // MPTCPD_EXPORT_H
