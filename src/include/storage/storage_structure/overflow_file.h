#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "common/constants.h"
#include "common/types/types.h"
#include "storage//file_handle.h"
#include "storage/index/hash_index_utils.h"
#include "storage/storage_structure/in_mem_page.h"
#include "storage/storage_utils.h"
#include "storage/wal/shadow_file.h"
#include "storage/wal/wal.h"
#include "transaction/transaction.h"

namespace kuzu {
namespace storage {

class OverflowFile;

class OverflowFileHandle {

public:
    OverflowFileHandle(OverflowFile& overflowFile, PageCursor& nextPosToWriteTo)
        : nextPosToWriteTo(nextPosToWriteTo), overflowFile{overflowFile} {}
    // The OverflowFile stores the handles and returns pointers to them.
    // Moving the handle would invalidate those pointers
    OverflowFileHandle(OverflowFileHandle&& other) = delete;

    std::string readString(transaction::TransactionType trxType, const common::ku_string_t& str);

    bool equals(transaction::TransactionType trxType, std::string_view keyToLookup,
        const common::ku_string_t& keyInEntry) const;

    common::ku_string_t writeString(std::string_view rawString);
    common::ku_string_t writeString(const char* rawString) {
        return writeString(std::string_view(rawString));
    }

    void checkpoint();
    void checkpointInMemory() { pageWriteCache.clear(); }
    void rollbackInMemory(PageCursor nextPosToWriteTo_) {
        pageWriteCache.clear();
        this->nextPosToWriteTo = nextPosToWriteTo_;
    }

private:
    uint8_t* addANewPage();
    void setStringOverflow(const char* inMemSrcStr, uint64_t len,
        common::ku_string_t& diskDstString);

    void read(transaction::TransactionType trxType, common::page_idx_t pageIdx,
        const std::function<void(uint8_t*)>& func) const;

private:
    static constexpr common::page_idx_t END_OF_PAGE =
        common::KUZU_PAGE_SIZE - sizeof(common::page_idx_t);
    // This is the index of the last free byte to which we can write.
    PageCursor& nextPosToWriteTo;
    OverflowFile& overflowFile;
    // Cached pages which have been written in the current transaction
    std::unordered_map<common::page_idx_t, std::unique_ptr<InMemPage>> pageWriteCache;
};

// Stores the current state of the overflow file
// The number of pages in use are stored here so that we can write new pages directly
// to the overflow file, and in the case of an interruption and rollback the header will
// still record the correct place in the file to allocate new pages
struct StringOverflowFileHeader {
    common::page_idx_t pages;
    PageCursor cursors[NUM_HASH_INDEXES];

    // pages starts at one to reserve space for this header
    StringOverflowFileHeader() : pages{1}, cursors{} {}
};
static_assert(std::has_unique_object_representations_v<StringOverflowFileHeader>);

class OverflowFile {
    friend class OverflowFileHandle;

public:
    // For reading an existing overflow file
    OverflowFile(const DBFileIDAndName& dbFileIdAndName, BufferManager* bufferManager,
        ShadowFile* shadowFile, bool readOnly, common::VirtualFileSystem* vfs,
        main::ClientContext* context);

    virtual ~OverflowFile() = default;

    // For creating an overflow file from scratch
    static void createEmptyFiles(const std::string& fName, common::VirtualFileSystem* vfs,
        main::ClientContext* context);

    // Handles contain a reference to the overflow file
    OverflowFile(OverflowFile&& other) = delete;

    void rollbackInMemory();
    void checkpoint();
    void checkpointInMemory();

    OverflowFileHandle* addHandle() {
        KU_ASSERT(handles.size() < NUM_HASH_INDEXES);
        handles.emplace_back(
            std::make_unique<OverflowFileHandle>(*this, header.cursors[handles.size()]));
        return handles.back().get();
    }

    FileHandle* getFileHandle() const {
        KU_ASSERT(fileHandle);
        return fileHandle;
    }

protected:
    explicit OverflowFile(const DBFileIDAndName& dbFileIdAndName);

    common::page_idx_t getNewPageIdx() {
        // If this isn't the first call reserving the page header, then the header flag must be set
        // prior to this
        KU_ASSERT(pageCounter == HEADER_PAGE_IDX || headerChanged);
        return pageCounter.fetch_add(1);
    }

private:
    void readFromDisk(transaction::TransactionType trxType, common::page_idx_t pageIdx,
        const std::function<void(uint8_t*)>& func) const;

    // Writes new pages directly to the file and existing pages to the WAL
    void writePageToDisk(common::page_idx_t pageIdx, uint8_t* data) const;

protected:
    static constexpr uint64_t HEADER_PAGE_IDX = 0;

    std::vector<std::unique_ptr<OverflowFileHandle>> handles;
    StringOverflowFileHeader header;
    common::page_idx_t numPagesOnDisk;
    DBFileID dbFileID;
    FileHandle* fileHandle;
    ShadowFile* shadowFile;
    std::atomic<common::page_idx_t> pageCounter;
    std::atomic<bool> headerChanged;
};

class InMemOverflowFile final : public OverflowFile {
public:
    explicit InMemOverflowFile(const DBFileIDAndName& dbFileIDAndName)
        : OverflowFile{dbFileIDAndName} {}
};

} // namespace storage
} // namespace kuzu
