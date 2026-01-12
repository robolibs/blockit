#pragma once

// High-level Blockit facade
// Composes ledger and storage modules

#include "blockit/ledger/ledger.hpp"
#include "blockit/storage/blockit_store.hpp"
#include "blockit/storage/file_store.hpp"

// Temporary alias to ease migration of call sites
// Existing code can continue using `chain::` until fully migrated
namespace chain = blockit::ledger;
