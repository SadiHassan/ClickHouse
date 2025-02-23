#if !defined(ARCADIA_BUILD)
#include <Common/config.h>
#endif

#include <Disks/DiskFactory.h>

#if USE_AZURE_BLOB_STORAGE

#include <Disks/DiskRestartProxy.h>
#include <Disks/DiskCacheWrapper.h>
#include <Disks/RemoteDisksCommon.h>
#include <Disks/BlobStorage/DiskBlobStorage.h>
#include <Disks/BlobStorage/BlobStorageAuth.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int PATH_ACCESS_DENIED;
}

constexpr char test_file[] = "test.txt";
constexpr char test_str[] = "test";
constexpr size_t test_str_size = 4;


void checkWriteAccess(IDisk & disk)
{
    auto file = disk.writeFile(test_file, DBMS_DEFAULT_BUFFER_SIZE, WriteMode::Rewrite);
    file->write(test_str, test_str_size);
}


void checkReadAccess(IDisk & disk)
{
    auto file = disk.readFile(test_file);
    String buf(test_str_size, '0');
    file->readStrict(buf.data(), test_str_size);
    if (buf != test_str)
        throw Exception("No read access to disk", ErrorCodes::PATH_ACCESS_DENIED);
}


void checkReadWithOffset(IDisk & disk)
{
    auto file = disk.readFile(test_file);
    auto offset = 2;
    auto test_size = test_str_size - offset;
    String buf(test_size, '0');
    file->seek(offset, 0);
    file->readStrict(buf.data(), test_size);
    if (buf != test_str + offset)
        throw Exception("Failed to read file with offset", ErrorCodes::PATH_ACCESS_DENIED);
}


void checkRemoveAccess(IDisk & disk)
{
    disk.removeFile(test_file);
}


std::unique_ptr<DiskBlobStorageSettings> getSettings(const Poco::Util::AbstractConfiguration & config, const String & config_prefix, ContextPtr /*context*/)
{
    return std::make_unique<DiskBlobStorageSettings>(
        config.getUInt64(config_prefix + ".max_single_part_upload_size", 100 * 1024 * 1024),
        config.getUInt64(config_prefix + ".min_bytes_for_seek", 1024 * 1024),
        config.getInt(config_prefix + ".max_single_read_retries", 3),
        config.getInt(config_prefix + ".max_single_download_retries", 3),
        config.getInt(config_prefix + ".thread_pool_size", 16)
    );
}


void registerDiskBlobStorage(DiskFactory & factory)
{
    auto creator = [](
        const String & name,
        const Poco::Util::AbstractConfiguration & config,
        const String & config_prefix,
        ContextPtr context,
        const DisksMap & /*map*/)
    {
        auto [metadata_path, metadata_disk] = prepareForLocalMetadata(name, config, config_prefix, context);

        std::shared_ptr<IDisk> blob_storage_disk = std::make_shared<DiskBlobStorage>(
            name,
            metadata_disk,
            getBlobContainerClient(config, config_prefix),
            getSettings(config, config_prefix, context),
            getSettings
        );

        if (!config.getBool(config_prefix + ".skip_access_check", false))
        {
            checkWriteAccess(*blob_storage_disk);
            checkReadAccess(*blob_storage_disk);
            checkReadWithOffset(*blob_storage_disk);
            checkRemoveAccess(*blob_storage_disk);
        }

        blob_storage_disk->startup();

        if (config.getBool(config_prefix + ".cache_enabled", true))
        {
            String cache_path = config.getString(config_prefix + ".cache_path", context->getPath() + "disks/" + name + "/cache/");
            blob_storage_disk = wrapWithCache(blob_storage_disk, "blob-storage-cache", cache_path, metadata_path);
        }

        return std::make_shared<DiskRestartProxy>(blob_storage_disk);
    };
    factory.registerDiskType("blob_storage", creator);
}

}

#else

namespace DB
{

void registerDiskBlobStorage(DiskFactory &) {}

}

#endif
