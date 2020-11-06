// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-cxx-build.cpp
 *
 * @brief Verify mptcpd API can be used in C++ code.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#undef NDEBUG

#include <memory>
#include <cassert>

#include <ell/main.h>
#include <ell/idle.h>
#include <ell/log.h>

#include <mptcpd/path_manager.h>   // Include to test build under C++.
#include <mptcpd/network_monitor.h>
#include <mptcpd/plugin.h>

#include <mptcpd/plugin_private.h>

#include "test-plugin.h"

/**
 * @class test_nm
 *
 * @brief Encapsulate a @c mptcpd_nm object.
 *
 * @note This class is only intended to exercise @c mptcpd network
 *       monitor operations when using a C++ compiler.  It is not
 *       intended to be used as a model for how the operations should
 *       be encapsulated in production code.
 */
class test_nm
{
public:

        test_nm() : nm_(mptcpd_nm_create()) { assert(this->nm_); }
        ~test_nm() { mptcpd_nm_destroy (this->nm_); }

private:
        test_nm(test_nm const &);
        void operator=(test_nm const &);

private:
        mptcpd_nm *const nm_;
};

/**
 * @class test_plugin
 *
 * @brief Encapsulate @c mptcpd plugin operations.
 *
 * @note This class is only intended to exercise @c mptcpd plugin
 *       operations when using a C++ compiler.  It is not intended to
 *       be used as a model for how the operations should be
 *       encapsulated in production code.
 */
class test_plugin
{
public:
        test_plugin()
        {
                static char const dir[]            = TEST_PLUGIN_DIR;
                static char const default_plugin[] = TEST_PLUGIN_FOUR;

                bool const loaded =
                        mptcpd_plugin_load(dir, default_plugin);
                assert(loaded);

                call_plugin_ops(&test_count_4,
                                NULL,
                                test_token_4,
                                test_raddr_id_4,
                                (struct sockaddr const *) &test_laddr_4,
                                (struct sockaddr const *) &test_raddr_4,
                                test_backup_4);
        }

        ~test_plugin() { mptcpd_plugin_unload(); }

private:
        test_plugin(test_plugin const &);
        void operator=(test_plugin const &);
};


int main()
{
        if (!l_main_init())
                return -1;

        l_log_set_stderr();

        /**
         * @note As of ELL 0.17 uncommenting the below call to
         *       l_debug_enable() causes link-time unresolved symbol
         *       errors caused by the @c __start___ell_debug and
         *       @c __stop___ell_debug symbols not being exported from
         *       the ELL shared library.
         */
        // l_debug_enable("*");

        test_nm nm;
        test_plugin p;

        (void) nm;
        (void) p;

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
