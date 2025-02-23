#pragma once

#if !defined(ARCADIA_BUILD)
#include <Common/config.h>
#endif

#if USE_AZURE_BLOB_STORAGE

#include <memory>

#include <IO/BufferWithOwnMemory.h>
#include <IO/WriteBuffer.h>
#include <azure/storage/blobs.hpp>
#include <azure/core/io/body_stream.hpp>


namespace DB
{

class WriteBufferFromBlobStorage : public BufferWithOwnMemory<WriteBuffer>
{
public:

    explicit WriteBufferFromBlobStorage(
        std::shared_ptr<Azure::Storage::Blobs::BlobContainerClient> blob_container_client_,
        const String & blob_path_,
        size_t max_single_part_upload_size_,
        size_t buf_size_);

    ~WriteBufferFromBlobStorage() override;

    void nextImpl() override;

private:

    void finalizeImpl() override;

    std::vector<std::string> block_ids;
    std::shared_ptr<Azure::Storage::Blobs::BlobContainerClient> blob_container_client;
    size_t max_single_part_upload_size;
    const String blob_path;
};

}

#endif
