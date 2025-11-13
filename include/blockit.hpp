#pragma once

// High-level Blockit facade
// Composes ledger and (future) storage modules

#include "blockit/ledger/ledger.hpp"
#include "blockit/storage/sqlite_store.hpp"

namespace blockit {

    class Blockit {
      public:
        Blockit() = default;
        ~Blockit() = default;
    };

} // namespace blockit

// Temporary alias to ease migration of call sites
// Existing code can continue using `chain::` until fully migrated
namespace chain = blockit::ledger;
