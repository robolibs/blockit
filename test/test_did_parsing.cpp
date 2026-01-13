#include "blockit/identity/did.hpp"
#include "blockit/ledger/key.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("DID Parsing Tests") {

    TEST_CASE("Parse valid DID string") {
        auto result = DID::parse("did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        CHECK(result.is_ok());
        CHECK(result.value().getMethod() == "blockit");
        CHECK(result.value().getMethodSpecificId() == "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
    }

    TEST_CASE("Parse DID with fragment") {
        auto result = DID::parse("did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069#key-1");
        CHECK(result.is_ok());
        // Fragment should be stripped from base DID
        CHECK(result.value().getMethodSpecificId() == "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
    }

    TEST_CASE("Parse DID with path") {
        auto result = DID::parse("did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069/some/path");
        CHECK(result.is_ok());
        // Path should be stripped from base DID
        CHECK(result.value().getMethodSpecificId() == "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
    }

    TEST_CASE("Reject invalid DID - wrong scheme") {
        auto result = DID::parse("urn:blockit:abc123");
        CHECK(result.is_err());
    }

    TEST_CASE("Reject invalid DID - wrong method") {
        auto result = DID::parse("did:other:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        CHECK(result.is_err());
    }

    TEST_CASE("Reject invalid DID - missing method-specific-id") {
        auto result = DID::parse("did:blockit");
        CHECK(result.is_err());
    }

    TEST_CASE("Reject invalid DID - wrong length method-specific-id") {
        auto result = DID::parse("did:blockit:abc123");
        CHECK(result.is_err());
    }

    TEST_CASE("Reject invalid DID - non-hex method-specific-id") {
        auto result = DID::parse("did:blockit:zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
        CHECK(result.is_err());
    }

    TEST_CASE("Create DID from Key") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        CHECK_FALSE(did.isEmpty());
        CHECK(did.getMethod() == "blockit");
        CHECK(did.getMethodSpecificId() == key_result.value().getId());
    }

    TEST_CASE("Create DID from method-specific-id") {
        auto did = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        CHECK(did.getMethodSpecificId() == "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
    }

    TEST_CASE("DID toString produces valid format") {
        auto did = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        CHECK(did.toString() == "did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
    }

    TEST_CASE("DID withFragment creates valid DID URL") {
        auto did = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        auto did_url = did.withFragment("key-1");
        CHECK(did_url == "did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069#key-1");
    }

    TEST_CASE("DID withPath creates valid DID URL") {
        auto did = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        auto did_url = did.withPath("documents/1");
        CHECK(did_url == "did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069/documents/1");
    }

    TEST_CASE("DID withQuery creates valid DID URL") {
        auto did = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        auto did_url = did.withQuery("version=2");
        CHECK(did_url == "did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069?version=2");
    }

    TEST_CASE("DID equality comparison") {
        auto did1 = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        auto did2 = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        auto did3 = DID::fromMethodSpecificId("0000000000000000000000000000000000000000000000000000000000000000");

        CHECK(did1 == did2);
        CHECK(did1 != did3);
    }

    TEST_CASE("DID less-than comparison for ordering") {
        auto did1 = DID::fromMethodSpecificId("0000000000000000000000000000000000000000000000000000000000000001");
        auto did2 = DID::fromMethodSpecificId("0000000000000000000000000000000000000000000000000000000000000002");

        CHECK(did1 < did2);
        CHECK_FALSE(did2 < did1);
    }

    TEST_CASE("DID hash function works") {
        auto did1 = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        auto did2 = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        auto did3 = DID::fromMethodSpecificId("0000000000000000000000000000000000000000000000000000000000000000");

        CHECK(did1.hash() == did2.hash());
        CHECK(did1.hash() != did3.hash());
    }

    TEST_CASE("DID can be used in unordered_map") {
        std::unordered_map<DID, std::string> did_map;

        auto did1 = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        auto did2 = DID::fromMethodSpecificId("0000000000000000000000000000000000000000000000000000000000000000");

        did_map[did1] = "Alice";
        did_map[did2] = "Bob";

        CHECK(did_map[did1] == "Alice");
        CHECK(did_map[did2] == "Bob");
        CHECK(did_map.size() == 2);
    }

    TEST_CASE("Empty DID is detected") {
        DID empty_did;
        CHECK(empty_did.isEmpty());

        auto did = DID::fromMethodSpecificId("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069");
        CHECK_FALSE(did.isEmpty());
    }

    TEST_CASE("Round-trip: create from Key, parse back") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto did_string = did.toString();

        auto parsed = DID::parse(did_string);
        REQUIRE(parsed.is_ok());

        CHECK(parsed.value() == did);
        CHECK(parsed.value().getMethodSpecificId() == key_result.value().getId());
    }
}
