// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#include <boost/test/unit_test.hpp>

#include "key.h"
#include "primecoin_address.h"


BOOST_AUTO_TEST_SUITE(primecoin_address_tests)

BOOST_AUTO_TEST_CASE(primecoin_mainnet_address) {
	fTestNet = false;

	// We know that the first letter of a public key hash should be 'A'
	CKeyID key_id = CKeyID();
	PrimecoinAddress primecoin_address = PrimecoinAddress();
	primecoin_address.Set(key_id);
	BOOST_CHECK(primecoin_address.ToString()[0] == 'A');

	// We know that the first letter of a script hash should be 'a'
	CScriptID script_id = CScriptID();
	primecoin_address = PrimecoinAddress();
	primecoin_address.Set(script_id);
	BOOST_CHECK(primecoin_address.ToString()[0] == 'a');
}

BOOST_AUTO_TEST_CASE(primecoin_testnet_address) {
	fTestNet = true;

	// We know that the first letter of a public key hash should be 'm'
	CKeyID key_id = CKeyID();
	PrimecoinAddress primecoin_address = PrimecoinAddress();
	primecoin_address.Set(key_id);
	BOOST_CHECK(primecoin_address.ToString()[0] == 'm');

	// We know that the first letter of a script hash should be '2'
	CScriptID script_id = CScriptID();
	primecoin_address = PrimecoinAddress();
	primecoin_address.Set(script_id);
	BOOST_CHECK(primecoin_address.ToString()[0] == '2');
}

BOOST_AUTO_TEST_SUITE_END()
