/**
 * Complete Stack Demo - Real-world usage of Blockit
 *
 * This demonstrates how to use the complete Blockit stack:
 * - Custom data types
 * - Blockchain transactions
 * - Automatic anchoring
 * - Cryptographic verification
 *
 * Use case: Document management system with blockchain verification
 */

#include <blockit/blockit.hpp>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

using namespace datapod;

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

    // Serialize for storage and hashing
    Vector<u8> toBytes() const {
        std::ostringstream oss;
        oss << id << "|" << title << "|" << author << "|" << content << "|" << category << "|" << created_at << "|"
            << version;
        std::string data = oss.str();
        return Vector<u8>(data.begin(), data.end());
    }
};

// ===========================================
// 2. Application Layer - Document Manager
// ===========================================

class DocumentManager {
  private:
    blockit::Blockit<Document> &store_;
    std::shared_ptr<blockit::ledger::Crypto> crypto_;
    std::map<std::string, Document> documents_; // In-memory document cache

  public:
    DocumentManager(blockit::Blockit<Document> &store, std::shared_ptr<blockit::ledger::Crypto> crypto)
        : store_(store), crypto_(crypto) {}

    // Create a new document
    bool createDocument(const Document &doc) {
        // Store in memory cache
        documents_[doc.id] = doc;

        // Register for blockchain anchoring
        std::string tx_id = "tx_" + doc.id;
        auto result =
            store_.createTransaction(String(tx_id.c_str()), doc, String(doc.id.c_str()), doc.toBytes(), 100);
        return result.is_ok();
    }

    // Update document (creates new version)
    bool updateDocument(const std::string &doc_id, const std::string &new_content, const std::string &modified_by,
                        const std::string &changes) {
        auto it = documents_.find(doc_id);
        if (it == documents_.end()) {
            return false;
        }

        // Create updated document
        Document &doc = it->second;
        doc.content = new_content;
        doc.version++;
        doc.created_at = blockit::storage::currentTimestamp();

        // Create new blockchain transaction for the update
        std::string tx_id = "tx_" + doc_id + "_v" + std::to_string(doc.version);
        auto result =
            store_.createTransaction(String(tx_id.c_str()), doc, String(doc_id.c_str()), doc.toBytes(), 100);
        return result.is_ok();
    }

    // Get documents by category
    std::vector<std::string> getDocumentsByCategory(const std::string &category) {
        std::vector<std::string> results;
        for (const auto &[id, doc] : documents_) {
            if (doc.category == category) {
                results.push_back(id);
            }
        }
        return results;
    }

    // Get documents by author
    std::vector<std::string> getDocumentsByAuthor(const std::string &author) {
        std::vector<std::string> results;
        for (const auto &[id, doc] : documents_) {
            if (doc.author == author) {
                results.push_back(id);
            }
        }
        return results;
    }

    // Search documents (simple substring search)
    std::vector<std::string> searchDocuments(const std::string &query) {
        std::vector<std::string> results;
        for (const auto &[id, doc] : documents_) {
            if (doc.title.find(query) != std::string::npos || doc.content.find(query) != std::string::npos) {
                results.push_back(id);
            }
        }
        return results;
    }

    // Get document by ID
    std::optional<Document> getDocument(const std::string &doc_id) const {
        auto it = documents_.find(doc_id);
        if (it != documents_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Verify document integrity against blockchain
    bool verifyDocument(const std::string &doc_id) {
        auto it = documents_.find(doc_id);
        if (it == documents_.end()) {
            return false;
        }
        auto result = store_.verifyContent(String(doc_id.c_str()), it->second.toBytes());
        return result.is_ok() && result.value();
    }
};

// ===========================================
// 3. Main Application
// ===========================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  Blockit Complete Stack Demo\n";
    std::cout << "  Document Management System\n";
    std::cout << "========================================\n\n";

    // Cleanup previous run
    const std::string storage_path = "documents_store";
    if (std::filesystem::exists(storage_path)) {
        std::filesystem::remove_all(storage_path);
    }

    // Initialize the complete stack
    blockit::Blockit<Document> store;
    auto crypto = std::make_shared<blockit::ledger::Crypto>("doc_system_key");

    Document genesis("genesis", "Genesis Document", "System", "Initial document", "system");

    std::cout << "[*] Initializing storage...\n";
    auto init_result = store.initialize(String(storage_path.c_str()), String("DocChain"), String("genesis_tx"), genesis,
                                        crypto);
    if (!init_result.is_ok()) {
        std::cerr << "[FAIL] Failed to initialize\n";
        return 1;
    }
    std::cout << "  [OK] Storage opened\n";
    std::cout << "  [OK] Blockchain initialized\n\n";

    // Create document manager
    DocumentManager doc_mgr(store, crypto);

    // Create some documents
    std::cout << "[*] Creating documents...\n";

    Document doc1("DOC001", "Introduction to Blockchain", "Alice Smith",
                  "Blockchain technology enables decentralized and secure data storage. "
                  "It uses cryptographic hashing to ensure data integrity.",
                  "technology");

    if (doc_mgr.createDocument(doc1)) {
        std::cout << "  [OK] Created: " << doc1.title << "\n";
    }

    Document doc2("DOC002", "Database Performance Tips", "Bob Johnson",
                  "Optimize your database queries with proper indexing. "
                  "Use indexes on frequently queried columns for better performance.",
                  "database");

    if (doc_mgr.createDocument(doc2)) {
        std::cout << "  [OK] Created: " << doc2.title << "\n";
    }

    Document doc3("DOC003", "Cryptographic Anchoring", "Alice Smith",
                  "Cryptographic anchoring links off-chain data to blockchain transactions. "
                  "This enables fast queries while maintaining blockchain security.",
                  "technology");

    if (doc_mgr.createDocument(doc3)) {
        std::cout << "  [OK] Created: " << doc3.title << "\n";
    }

    // Create blockchain transactions and commit
    std::cout << "\n[*] Committing to blockchain...\n";
    std::vector<blockit::ledger::Transaction<Document>> transactions;

    for (const auto &doc : {doc1, doc2, doc3}) {
        blockit::ledger::Transaction<Document> tx("tx_" + doc.id, doc, 100);
        tx.signTransaction(crypto);
        transactions.push_back(tx);
    }

    auto add_result = store.addBlock(transactions);
    if (add_result.is_ok()) {
        std::cout << "  [OK] Block added\n";
        std::cout << "  [OK] Transactions stored\n";
        std::cout << "  [OK] Anchors created\n";
    }

    // Update a document
    std::cout << "\n[*] Updating document...\n";
    if (doc_mgr.updateDocument("DOC001",
                               "Blockchain technology enables decentralized and secure data storage. "
                               "Updated with more details about consensus mechanisms.",
                               "Alice Smith", "Added consensus details")) {
        std::cout << "  [OK] DOC001 updated to v2\n";
    }

    // Commit the update
    auto updated_doc = doc_mgr.getDocument("DOC001");
    if (updated_doc) {
        blockit::ledger::Transaction<Document> update_tx("tx_DOC001_v2", *updated_doc, 100);
        update_tx.signTransaction(crypto);
        auto update_result = store.addBlock({update_tx});
        if (update_result.is_ok()) {
            std::cout << "  [OK] Update anchored to blockchain\n";
        }
    }

    // Search documents
    std::cout << "\n[*] Search for 'blockchain':\n";
    auto search_results = doc_mgr.searchDocuments("blockchain");
    for (const auto &doc_id : search_results) {
        std::cout << "  - Found: " << doc_id << "\n";
    }

    // Filter by category
    std::cout << "\n[*] Documents in 'technology' category:\n";
    auto tech_docs = doc_mgr.getDocumentsByCategory("technology");
    for (const auto &doc_id : tech_docs) {
        std::cout << "  - " << doc_id << "\n";
    }

    // Filter by author
    std::cout << "\n[*] Documents by Alice Smith:\n";
    auto alice_docs = doc_mgr.getDocumentsByAuthor("Alice Smith");
    for (const auto &doc_id : alice_docs) {
        std::cout << "  - " << doc_id << "\n";
    }

    // Verify documents
    std::cout << "\n[*] Verifying document integrity...\n";
    for (const auto &doc_id : {"DOC001", "DOC002", "DOC003"}) {
        if (doc_mgr.verifyDocument(doc_id)) {
            std::cout << "  [OK] " << doc_id << " verified against blockchain\n";
        } else {
            std::cout << "  [FAIL] " << doc_id << " verification failed\n";
        }
    }

    // Statistics
    std::cout << "\n[*] System Statistics:\n";
    std::cout << "  Blocks: " << store.getBlockCount() << "\n";
    std::cout << "  Transactions: " << store.getTransactionCount() << "\n";
    std::cout << "  Anchors: " << store.getAnchorCount() << "\n";

    // Verify consistency
    std::cout << "\n[*] System Consistency Check:\n";
    auto consistency = store.verifyConsistency();
    if (consistency.is_ok() && consistency.value()) {
        std::cout << "  [OK] Blockchain and storage are synchronized\n";
    } else {
        std::cout << "  [FAIL] Inconsistency detected\n";
    }

    // Query anchor info
    std::cout << "\n[*] Anchor Information:\n";
    auto anchor = store.getAnchor(String("DOC001"));
    if (anchor.has_value()) {
        std::cout << "  Document: DOC001\n";
        std::cout << "  Transaction: " << anchor->tx_id.c_str() << "\n";
        std::cout << "  Block Height: " << anchor->block_height << "\n";
        std::cout << "  Anchored At: " << anchor->anchored_at << "\n";
    }

    std::cout << "\n========================================\n";
    std::cout << "  Demo Completed Successfully\n";
    std::cout << "========================================\n";

    // Cleanup
    std::filesystem::remove_all(storage_path);

    return 0;
}
