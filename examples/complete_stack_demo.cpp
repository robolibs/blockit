/**
 * Complete Stack Demo - Real-world usage of Blockit
 *
 * This demonstrates how to use the complete Blockit stack:
 * - Custom data types
 * - Custom database schema with indexes and FTS
 * - Blockchain transactions
 * - Automatic anchoring
 * - Fast database queries
 * - Cryptographic verification
 *
 * Use case: Document management system with blockchain verification
 */

#include <blockit.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

// ===========================================
// 1. Define your domain data type
// ===========================================

struct Document {
    std::string id;
    std::string title;
    std::string author;
    std::string content;
    std::string category;
    int64_t created_at;
    int version;

    Document() : created_at(0), version(1) {}

    Document(std::string id_, std::string title_, std::string author_, std::string content_, std::string category_)
        : id(std::move(id_)), title(std::move(title_)), author(std::move(author_)), content(std::move(content_)),
          category(std::move(category_)), created_at(blockit::storage::currentTimestamp()), version(1) {}

    // Required for blockchain
    std::string to_string() const { return "Document{" + id + ", " + title + ", v" + std::to_string(version) + "}"; }

    // Serialize for database storage and hashing
    std::vector<uint8_t> toBytes() const {
        std::ostringstream oss;
        oss << id << "|" << title << "|" << author << "|" << content << "|" << category << "|" << created_at << "|"
            << version;
        std::string data = oss.str();
        return std::vector<uint8_t>(data.begin(), data.end());
    }
};

// ===========================================
// 2. Define your database schema
// ===========================================

class DocumentSchema : public blockit::storage::ISchemaExtension {
  public:
    std::vector<std::string> getCreateTableStatements() const override {
        return {// Main documents table
                R"(CREATE TABLE IF NOT EXISTS documents (
                doc_id TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                author TEXT NOT NULL,
                content TEXT NOT NULL,
                category TEXT NOT NULL,
                created_at INTEGER NOT NULL,
                version INTEGER NOT NULL DEFAULT 1,
                content_hash BLOB
            ))",

                // Full-text search on title and content
                R"(CREATE VIRTUAL TABLE IF NOT EXISTS documents_fts USING fts5(
                doc_id UNINDEXED,
                title,
                content,
                content='',
                content_rowid=''
            ))",

                // Document history/versions table
                R"(CREATE TABLE IF NOT EXISTS document_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                doc_id TEXT NOT NULL,
                version INTEGER NOT NULL,
                title TEXT NOT NULL,
                modified_by TEXT NOT NULL,
                modified_at INTEGER NOT NULL,
                changes TEXT,
                FOREIGN KEY(doc_id) REFERENCES documents(doc_id) ON DELETE CASCADE
            ))"};
    }

    std::vector<std::string> getCreateIndexStatements() const override {
        return {"CREATE INDEX IF NOT EXISTS idx_docs_author ON documents(author)",
                "CREATE INDEX IF NOT EXISTS idx_docs_category ON documents(category)",
                "CREATE INDEX IF NOT EXISTS idx_docs_created ON documents(created_at)",
                "CREATE INDEX IF NOT EXISTS idx_docs_version ON documents(version)",
                "CREATE INDEX IF NOT EXISTS idx_history_doc ON document_history(doc_id)"};
    }
};

// ===========================================
// 3. Application Layer - Document Manager
// ===========================================

class DocumentManager {
  private:
    blockit::Blockit<Document> &store_;
    std::shared_ptr<blockit::ledger::Crypto> crypto_;

  public:
    DocumentManager(blockit::Blockit<Document> &store, std::shared_ptr<blockit::ledger::Crypto> crypto)
        : store_(store), crypto_(crypto) {}

    // Create a new document
    bool createDocument(const Document &doc) {
        auto tx_guard = store_.getStorage().beginTransaction();

        // 1. Store in database
        auto hash = blockit::storage::computeSHA256(doc.toBytes());
        std::string hash_hex = blockit::storage::hashToHex(hash);

        std::ostringstream sql;
        sql << "INSERT INTO documents (doc_id, title, author, content, category, created_at, version, content_hash) "
            << "VALUES ('" << doc.id << "', '" << escapeSQL(doc.title) << "', '" << escapeSQL(doc.author) << "', '"
            << escapeSQL(doc.content) << "', '" << doc.category << "', " << doc.created_at << ", " << doc.version
            << ", x'" << hash_hex << "')";

        if (!store_.getStorage().executeSql(sql.str())) {
            return false;
        }

        // 2. Add to FTS index
        std::ostringstream fts_sql;
        fts_sql << "INSERT INTO documents_fts (doc_id, title, content) VALUES ('" << doc.id << "', '"
                << escapeSQL(doc.title) << "', '" << escapeSQL(doc.content) << "')";

        if (!store_.getStorage().executeSql(fts_sql.str())) {
            return false;
        }

        // 3. Register for blockchain anchoring
        std::string tx_id = "tx_" + doc.id;
        if (!store_.createTransaction(tx_id, doc, doc.id, doc.toBytes(), 100)) {
            return false;
        }

        tx_guard->commit();
        return true;
    }

    // Update document (creates new version)
    bool updateDocument(const std::string &doc_id, const std::string &new_content, const std::string &modified_by,
                        const std::string &changes) {
        auto tx_guard = store_.getStorage().beginTransaction();

        // Get current document
        std::vector<std::string> current;
        bool found = false;
        store_.getStorage().executeQuery("SELECT title, author, category, version FROM documents WHERE doc_id = '" +
                                             doc_id + "'",
                                         [&](const std::vector<std::string> &row) {
                                             if (row.size() >= 4) {
                                                 current = row;
                                                 found = true;
                                             }
                                         });

        if (!found)
            return false;

        int new_version = std::stoi(current[3]) + 1;

        // Create updated document
        Document updated_doc;
        updated_doc.id = doc_id;
        updated_doc.title = current[0];
        updated_doc.author = current[1];
        updated_doc.content = new_content;
        updated_doc.category = current[2];
        updated_doc.created_at = blockit::storage::currentTimestamp();
        updated_doc.version = new_version;

        // Update main table
        auto hash = blockit::storage::computeSHA256(updated_doc.toBytes());
        std::string hash_hex = blockit::storage::hashToHex(hash);

        std::ostringstream sql;
        sql << "UPDATE documents SET content = '" << escapeSQL(new_content) << "', version = " << new_version
            << ", content_hash = x'" << hash_hex << "' WHERE doc_id = '" << doc_id << "'";

        if (!store_.getStorage().executeSql(sql.str())) {
            return false;
        }

        // Update FTS
        std::ostringstream fts_sql;
        fts_sql << "UPDATE documents_fts SET content = '" << escapeSQL(new_content) << "' WHERE doc_id = '" << doc_id
                << "'";
        store_.getStorage().executeSql(fts_sql.str());

        // Add to history
        std::ostringstream hist_sql;
        hist_sql << "INSERT INTO document_history (doc_id, version, title, modified_by, modified_at, changes) "
                 << "VALUES ('" << doc_id << "', " << new_version << ", '" << escapeSQL(updated_doc.title) << "', '"
                 << modified_by << "', " << updated_doc.created_at << ", '" << escapeSQL(changes) << "')";

        if (!store_.getStorage().executeSql(hist_sql.str())) {
            return false;
        }

        // Create new blockchain transaction for the update
        std::string tx_id = "tx_" + doc_id + "_v" + std::to_string(new_version);
        if (!store_.createTransaction(tx_id, updated_doc, doc_id, updated_doc.toBytes(), 100)) {
            return false;
        }

        tx_guard->commit();
        return true;
    }

    // Commit pending transactions to blockchain
    bool commitToBlockchain() {
        // Gather all pending transactions
        std::vector<blockit::ledger::Transaction<Document>> transactions;

        // This is simplified - in practice you'd track pending transactions
        // For this demo, we'll just show the pattern

        // Add block commits blockchain + creates all anchors atomically
        return store_.addBlock(transactions);
    }

    // Full-text search
    std::vector<std::string> searchDocuments(const std::string &query) {
        std::vector<std::string> results;
        std::string sql = "SELECT doc_id FROM documents_fts WHERE documents_fts MATCH '" + query + "' LIMIT 10";

        store_.getStorage().executeQuery(sql, [&](const std::vector<std::string> &row) {
            if (!row.empty()) {
                results.push_back(row[0]);
            }
        });

        return results;
    }

    // Filter by category
    std::vector<std::string> getDocumentsByCategory(const std::string &category) {
        std::vector<std::string> results;
        std::string sql = "SELECT doc_id FROM documents WHERE category = '" + category + "' ORDER BY created_at DESC";

        store_.getStorage().executeQuery(sql, [&](const std::vector<std::string> &row) {
            if (!row.empty()) {
                results.push_back(row[0]);
            }
        });

        return results;
    }

    // Get documents by author
    std::vector<std::string> getDocumentsByAuthor(const std::string &author) {
        std::vector<std::string> results;
        std::string sql =
            "SELECT doc_id FROM documents WHERE author = '" + escapeSQL(author) + "' ORDER BY created_at DESC";

        store_.getStorage().executeQuery(sql, [&](const std::vector<std::string> &row) {
            if (!row.empty()) {
                results.push_back(row[0]);
            }
        });

        return results;
    }

    // Get document history
    void printDocumentHistory(const std::string &doc_id) {
        std::cout << "\nDocument History for " << doc_id << ":\n";

        std::string sql = "SELECT version, modified_by, modified_at, changes FROM document_history "
                          "WHERE doc_id = '" +
                          doc_id + "' ORDER BY version";

        store_.getStorage().executeQuery(sql, [&](const std::vector<std::string> &row) {
            if (row.size() >= 4) {
                std::cout << "  v" << row[0] << " by " << row[1] << " at " << row[2] << " - " << row[3] << "\n";
            }
        });
    }

    // Verify document integrity against blockchain
    bool verifyDocument(const std::string &doc_id) {
        // Get current document data
        std::vector<std::string> doc_data;
        bool found = false;

        std::string sql =
            "SELECT title, author, content, category, created_at, version FROM documents WHERE doc_id = '" + doc_id +
            "'";

        store_.getStorage().executeQuery(sql, [&](const std::vector<std::string> &row) {
            if (row.size() >= 6) {
                doc_data = row;
                found = true;
            }
        });

        if (!found)
            return false;

        // Reconstruct document
        Document doc;
        doc.id = doc_id;
        doc.title = doc_data[0];
        doc.author = doc_data[1];
        doc.content = doc_data[2];
        doc.category = doc_data[3];
        doc.created_at = std::stoll(doc_data[4]);
        doc.version = std::stoi(doc_data[5]);

        // Verify against blockchain anchor
        return store_.verifyContent(doc_id, doc.toBytes());
    }

  private:
    std::string escapeSQL(const std::string &str) {
        std::string escaped;
        for (char c : str) {
            if (c == '\'')
                escaped += "''";
            else
                escaped += c;
        }
        return escaped;
    }
};

// ===========================================
// 4. Main Application
// ===========================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║     Blockit Complete Stack Demo                     ║\n";
    std::cout << "║     Document Management System                      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";

    // Initialize the complete stack
    blockit::Blockit<Document> store;
    auto crypto = std::make_shared<blockit::ledger::Crypto>("doc_system_key");

    Document genesis("genesis", "Genesis Document", "System", "Initial document", "system");

    std::cout << "📦 Initializing storage...\n";
    if (!store.initialize("documents.db", "DocChain", "genesis_tx", genesis, crypto)) {
        std::cerr << "❌ Failed to initialize\n";
        return 1;
    }
    std::cout << "   ✓ Database opened\n";
    std::cout << "   ✓ Blockchain initialized\n\n";

    // Register schema
    std::cout << "📋 Registering document schema...\n";
    DocumentSchema schema;
    if (!store.registerSchema(schema)) {
        std::cerr << "❌ Failed to register schema\n";
        return 1;
    }
    std::cout << "   ✓ Tables created\n";
    std::cout << "   ✓ Indexes created\n";
    std::cout << "   ✓ Full-text search enabled\n\n";

    // Create document manager
    DocumentManager doc_mgr(store, crypto);

    // Create some documents
    std::cout << "📝 Creating documents...\n";

    Document doc1("DOC001", "Introduction to Blockchain", "Alice Smith",
                  "Blockchain technology enables decentralized and secure data storage. "
                  "It uses cryptographic hashing to ensure data integrity.",
                  "technology");

    if (doc_mgr.createDocument(doc1)) {
        std::cout << "   ✓ Created: " << doc1.title << "\n";
    }

    Document doc2("DOC002", "Database Performance Tips", "Bob Johnson",
                  "Optimize your database queries with proper indexing. "
                  "Use indexes on frequently queried columns for better performance.",
                  "database");

    if (doc_mgr.createDocument(doc2)) {
        std::cout << "   ✓ Created: " << doc2.title << "\n";
    }

    Document doc3("DOC003", "Cryptographic Anchoring", "Alice Smith",
                  "Cryptographic anchoring links off-chain data to blockchain transactions. "
                  "This enables fast queries while maintaining blockchain security.",
                  "technology");

    if (doc_mgr.createDocument(doc3)) {
        std::cout << "   ✓ Created: " << doc3.title << "\n";
    }

    // Create blockchain transactions and commit
    std::cout << "\n⛓️  Committing to blockchain...\n";
    std::vector<blockit::ledger::Transaction<Document>> transactions;

    for (const auto &doc : {doc1, doc2, doc3}) {
        blockit::ledger::Transaction<Document> tx("tx_" + doc.id, doc, 100);
        tx.signTransaction(crypto);
        transactions.push_back(tx);
    }

    if (store.addBlock(transactions)) {
        std::cout << "   ✓ Block added\n";
        std::cout << "   ✓ Transactions stored\n";
        std::cout << "   ✓ Anchors created\n";
    }

    // Update a document
    std::cout << "\n✏️  Updating document...\n";
    if (doc_mgr.updateDocument("DOC001",
                               "Blockchain technology enables decentralized and secure data storage. "
                               "Updated with more details about consensus mechanisms.",
                               "Alice Smith", "Added consensus details")) {
        std::cout << "   ✓ DOC001 updated to v2\n";
    }

    // Commit the update
    blockit::ledger::Transaction<Document> update_tx("tx_DOC001_v2", doc1, 100);
    update_tx.signTransaction(crypto);
    if (store.addBlock({update_tx})) {
        std::cout << "   ✓ Update anchored to blockchain\n";
    }

    // Search documents
    std::cout << "\n🔍 Full-text search for 'blockchain':\n";
    auto search_results = doc_mgr.searchDocuments("blockchain");
    for (const auto &doc_id : search_results) {
        std::cout << "   - Found: " << doc_id << "\n";
    }

    // Filter by category
    std::cout << "\n📁 Documents in 'technology' category:\n";
    auto tech_docs = doc_mgr.getDocumentsByCategory("technology");
    for (const auto &doc_id : tech_docs) {
        std::cout << "   - " << doc_id << "\n";
    }

    // Filter by author
    std::cout << "\n👤 Documents by Alice Smith:\n";
    auto alice_docs = doc_mgr.getDocumentsByAuthor("Alice Smith");
    for (const auto &doc_id : alice_docs) {
        std::cout << "   - " << doc_id << "\n";
    }

    // Show document history
    doc_mgr.printDocumentHistory("DOC001");

    // Verify documents
    std::cout << "\n🔐 Verifying document integrity...\n";
    for (const auto &doc_id : {"DOC001", "DOC002", "DOC003"}) {
        if (doc_mgr.verifyDocument(doc_id)) {
            std::cout << "   ✓ " << doc_id << " verified against blockchain\n";
        } else {
            std::cout << "   ✗ " << doc_id << " verification failed\n";
        }
    }

    // Statistics
    std::cout << "\n📊 System Statistics:\n";
    std::cout << "   Blocks: " << store.getBlockCount() << "\n";
    std::cout << "   Transactions: " << store.getTransactionCount() << "\n";
    std::cout << "   Anchors: " << store.getAnchorCount() << "\n";

    // Verify consistency
    std::cout << "\n🔗 System Consistency Check:\n";
    if (store.verifyConsistency()) {
        std::cout << "   ✓ Blockchain and database are synchronized\n";
    } else {
        std::cout << "   ✗ Inconsistency detected\n";
    }

    // Query anchor info
    std::cout << "\n⚓ Anchor Information:\n";
    auto anchor = store.getAnchor("DOC001");
    if (anchor) {
        std::cout << "   Document: DOC001\n";
        std::cout << "   Transaction: " << anchor->tx_id << "\n";
        std::cout << "   Block Height: " << anchor->block_height << "\n";
        std::cout << "   Anchored At: " << anchor->anchored_at << "\n";
    }

    std::cout << "\n╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║     ✓ Demo Completed Successfully                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";

    return 0;
}
