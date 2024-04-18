//
//  Archive.hpp
//
//
//
//

#ifndef Archive_hpp
#define Archive_hpp

#include <cstdio>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <optional>
#include <stdexcept>
#include <ctime>
#include <zlib.h>

namespace ECE141 {

    using Block = std::vector<uint8_t>;

    enum class ActionType {added, extracted, removed, listed, dumped, compacted};
    enum class AccessMode {AsNew, AsExisting}; //you can change values (but not names) of this enum

    struct ArchiveObserver {
        void operator()(ActionType anAction,
                        const std::string &aName, bool status);
    };

    class IDataProcessor {
    public:
        virtual std::vector<uint8_t> process(const std::vector<uint8_t>& input) = 0;
        virtual std::vector<uint8_t> reverseProcess(const std::vector<uint8_t>& input) = 0;
        virtual ~IDataProcessor() {};
    };

    /** This is new child class of data processor, use it to compress the if add asks for it*/
    class Compression : public IDataProcessor {
    public:
        std::vector<uint8_t> process(const std::vector<uint8_t>& input) override {
            // write the compress process here
            uLongf compressedSize = compressBound(input.size());

            std::vector<uint8_t> compressedData(compressedSize);

            // Perform compression
            int result = compress(compressedData.data(), &compressedSize, input.data(), input.size());
            if (result != Z_OK) {
                throw std::runtime_error("Compression failed");
            }

            // Resize the vector to fit the actual compressed size
            compressedData.resize(compressedSize);
            return compressedData;
        }

        std::vector<uint8_t> reverseProcess(const std::vector<uint8_t>& input) override {
            // write the compress process here
            uLongf decompressedSize = input.size() * 10; // Arbitrary multiplier for demonstration
            std::vector<uint8_t> decompressedData(decompressedSize);

            // Perform decompression
            int result = uncompress(decompressedData.data(), &decompressedSize, input.data(), input.size());
            if (result == Z_BUF_ERROR) {
                // Buffer error, might need a larger buffer, handle accordingly
                throw std::runtime_error("Decompression buffer was too small");
            }
            else if (result != Z_OK) {
                // Other decompression errors
                throw std::runtime_error("Decompression failed");
            }

            // Resize the vector to fit the actual decompressed size
            decompressedData.resize(decompressedSize);
            return decompressedData;
        }
        ~Compression() override = default;
    };

    enum class ArchiveErrors {
        noError=0,
        fileNotFound=1, fileExists, fileOpenError, fileReadError, fileWriteError, fileCloseError,
        fileSeekError, fileTellError, fileError, badFilename, badPath, badData, badBlock, badArchive,
        badAction, badMode, badProcessor, badBlockType, badBlockCount, badBlockIndex, badBlockData,
        badBlockHash, badBlockNumber, badBlockLength, badBlockDataLength, badBlockTypeLength
    };

    template<typename T>
    class ArchiveStatus {
    public:
        // Constructor for success case
        explicit ArchiveStatus(const T value)
                : value(value), error(ArchiveErrors::noError) {}

        // Constructor for error case
        explicit ArchiveStatus(ArchiveErrors anError)
                : value(std::nullopt), error(anError) {
            if (anError == ArchiveErrors::noError) {
                throw std::logic_error("Cannot use noError with error constructor");
            }
        }

        // Deleted copy constructor and copy assignment to make ArchiveStatus move-only
        ArchiveStatus(const ArchiveStatus&) = delete;
        ArchiveStatus& operator=(const ArchiveStatus&) = delete;

        // Default move constructor and move assignment
        ArchiveStatus(ArchiveStatus&&) noexcept = default;
        ArchiveStatus& operator=(ArchiveStatus&&) noexcept = default;

        T getValue() const {
            if (!isOK()) {
                throw std::runtime_error("Operation failed with error");
            }
            return *value;
        }

        bool isOK() const {
            return error == ArchiveErrors::noError && value.has_value();
        }

        ArchiveErrors getError() const {
            return error;
        }

    private:
        std::optional<T> value;
        ArchiveErrors error;
    };


    //--------------------------------------------------------------------------------
    //You'll need to define your own classes for Blocks, and other useful types...
    //--------------------------------------------------------------------------------

    struct FileMetadata {
        std::string filename;
        size_t fileSize;
        std::time_t dateAdded;
        size_t blockIndex; // Index to the ArrayList of blocks

        FileMetadata(const std::string& name, size_t size, std::time_t date, size_t index)
            : filename(name), fileSize(size), dateAdded(date), blockIndex(index) {}
    };

    class Archive : public std::enable_shared_from_this<Archive> {
    protected:
        std::vector<std::shared_ptr<IDataProcessor>> processors;
        std::vector<std::shared_ptr<ArchiveObserver>> observers;
        Archive(const std::string& aFullPath, AccessMode aMode) {

        }//protected on purpose

    public:

        ~Archive() {

        } // destructor

        static    ArchiveStatus<std::shared_ptr<Archive>> createArchive(const std::string &anArchiveName);
        static    ArchiveStatus<std::shared_ptr<Archive>> openArchive(const std::string &anArchiveName);

        bool fileExists(const std::string& filename);

        Archive&  addObserver(std::shared_ptr<ArchiveObserver> anObserver);

        ArchiveStatus<bool>      add(const std::string &aFilename, IDataProcessor* aProcessor = nullptr);
        ArchiveStatus<bool>      extract(const std::string &aFilename, const std::string &aFullPath);
        ArchiveStatus<bool>      remove(const std::string &aFilename);

        ArchiveStatus<size_t>    list(std::ostream &aStream);
        ArchiveStatus<size_t>    debugDump(std::ostream &aStream);

        ArchiveStatus<size_t>    compact();
        ArchiveStatus<std::string> getFullPath() const; //get archive path (including .arc extension)

        std::vector<std::string> files;

    private:
        std::vector<std::vector<Block>> fileBlocks; // ArrayList of ArrayList of blocks
        std::vector<FileMetadata> metadataList; // Separate ArrayList for metadata
        int fileCount = 0;
        std::string tracker;
        static std::shared_ptr<Archive> createInternally(const std::string& fullPath, AccessMode aMode) {
            // Directly invoke the protected constructor through a lambda passed to make_shared
            return std::shared_ptr<Archive>(new Archive(fullPath, aMode));
        }

        //STUDENT: add anything else you want here, (e.g. blocks?)...
        // Make an arraylist of arraylist of blocks, where the location is the index
        // For the actual files, make another arraylist where it holds a struct,
        // that contains the metadata of filename, fileSize, and dateAdded, as well
        // as the index of the arraylist of blocks


    };
}

#endif /* Archive_hpp */
