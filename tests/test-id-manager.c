// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-id-manager.c
 *
 * @brief mptcpd ID manager test.
 *
 * Copyright (c) 2020-2021, Intel Corporation
 */

#include <stddef.h>

#include <ell/log.h>
#include <ell/test.h>

#include <mptcpd/private/id_manager.h>
#include <mptcpd/id_manager.h>

#include "test-plugin.h"  // For test sockaddrs

#undef NDEBUG
#include <assert.h>


/// mptcpd ID manager.
static struct mptcpd_idm *_idm;

/// Obtained MPTCP address IDs
static mptcpd_aid_t _id[4];

/// Expected MPTCP address ID after ID update.
static mptcpd_aid_t const _updated_id = 222;

static void test_create(void const *test_data)
{
        (void) test_data;

        _idm = mptcpd_idm_create();
}

static void test_map_id(void const *test_data)
{
        (void) test_data;

        mptcpd_aid_t const id = 245;

        assert(id != _updated_id);  // sanity check

        assert(mptcpd_idm_map_id(_idm,
                                 (struct sockaddr *) &test_laddr_4,
                                 id));

        // Update previously set ID for the same IP address.
        assert(mptcpd_idm_map_id(_idm,
                                 (struct sockaddr *) &test_laddr_4,
                                 _updated_id));
}

static void test_get_id(void const *test_data)
{
        (void) test_data;

        _id[0] = mptcpd_idm_get_id(_idm,
                                   (struct sockaddr *) &test_laddr_1);
        assert(_id[0] != 0);

        _id[1] = mptcpd_idm_get_id(_idm,
                                   (struct sockaddr *) &test_laddr_2);
        assert(_id[1] != 0 && _id[1] != _id[0]);

        _id[2] = mptcpd_idm_get_id(_idm,
                                   (struct sockaddr *) &test_laddr_1);
        assert(_id[2] != 0 && _id[2] == _id[0]);

        _id[3] = mptcpd_idm_get_id(_idm,
                                   (struct sockaddr *) &test_raddr_1);
        assert(_id[3] != 0 && _id[3] != _id[0] && _id[3] != _id[1]);

        // ID that was updated in test_map_id().
        mptcpd_aid_t const id =
                mptcpd_idm_get_id(_idm,
                                  (struct sockaddr *) &test_laddr_4);
        assert(id == _updated_id);
}

static void test_remove_id(void const *test_data)
{
        (void) test_data;

        mptcpd_aid_t i = 0;

        i = mptcpd_idm_remove_id(_idm,
                                 (struct sockaddr *) &test_laddr_2);
        assert(i == _id[1]);

        i = mptcpd_idm_remove_id(_idm,
                                 (struct sockaddr *) &test_laddr_2);
        assert(i == 0);
}


static void test_destroy(void const *test_data)
{
        (void) test_data;

        mptcpd_idm_destroy(_idm);
}

int main(int argc, char *argv[])
{
        l_log_set_stderr();

        l_test_init(&argc, &argv);

        l_test_add("create ID manager",  test_create,    NULL);
        l_test_add("map ID",             test_map_id,    NULL);
        l_test_add("get ID",             test_get_id,    NULL);
        l_test_add("remove ID",          test_remove_id, NULL);
        l_test_add("destroy ID manager", test_destroy,   NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
