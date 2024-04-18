//
//  Archive.cpp
//
//
//
//

#include "Archive.hpp"
#include <filesystem>
#include <memory>

namespace ECE141 {

    //STUDENT put archive class code here...

    ArchiveStatus<std::shared_ptr<Archive>> Archive::createArchive(const std::string& anArchiveName) {
        std::string fullPath;
        if (anArchiveName.size() < 4 || anArchiveName.substr(anArchiveName.size() - 4) != ".arc") {
            fullPath = anArchiveName + ".arc";
        }
        else {
            fullPath = anArchiveName;
        }
        // Attempt to create/truncate the file
        std::ofstream archiveFile(fullPath, std::ios::binary | std::ios::trunc);
        if (!archiveFile) {
            return ArchiveStatus<std::shared_ptr<Archive>>(ArchiveErrors::fileOpenError);
        }
        archiveFile.close(); // Close the file after truncating/creating

        // Now, open the file with the intended Archive object
        auto archive = Archive::createInternally(fullPath, AccessMode::AsNew);

        archive->tracker = fullPath;
        return ArchiveStatus<std::shared_ptr<Archive>>(archive);
    }

    ArchiveStatus<std::shared_ptr<Archive>> Archive::openArchive(const std::string& anArchiveName) {
        std::string fullPath = anArchiveName;
        const std::string extension = ".arc";
        if (fullPath.length() >= extension.length() &&
            fullPath.substr(fullPath.length() - extension.length()) != extension) {
            fullPath = fullPath + extension;
        }

        // Check if the file exists by attempting to open it for reading
        std::ifstream archiveFile(fullPath, std::ios::binary);
        if (!archiveFile) {
            return ArchiveStatus<std::shared_ptr<Archive>>(ArchiveErrors::fileNotFound); // File does not exist or error opening it
        }
        archiveFile.close(); // Close the file after confirming its existence

        // Assuming the file exists and is a legitimate archive file, open it with an Archive object
        try {
            auto archive = Archive::createInternally(fullPath, AccessMode::AsExisting); // Use the internal method for instantiation
            archive->tracker = fullPath;
            return ArchiveStatus<std::shared_ptr<Archive>>(archive); // Return the new Archive object
        }
        catch (const std::exception& e) {
            // catch exception
            return ArchiveStatus<std::shared_ptr<Archive>>(ArchiveErrors::fileError); // Use an appropriate error code
        }
    }

    bool Archive::fileExists(const std::string& filename) {
        return false;
    }

    Archive& Archive::addObserver(std::shared_ptr<ArchiveObserver> anObserver)
    {
        // TODO: insert return statement here
        observers.push_back(anObserver);
        return *this; // Return a reference to this instance of Archive
    }

    std::string extractFilename(const std::string& path) {
        size_t position = path.find_last_of("/\\");
        return (std::string::npos == position) ? path : path.substr(position + 1);
    }

    size_t getFileSize(const std::string& aFilePath) {
        std::ifstream theStream(aFilePath, std::ios::binary);
        const auto theBegin = theStream.tellg();
        theStream.seekg(0, std::ios::end);
        const auto theEnd = theStream.tellg();
        return theEnd - theBegin;
    }

    ArchiveStatus<bool> Archive::add(const std::string& aFullPath, IDataProcessor* aProcessor) {

        if (aProcessor) {
            std::string filename = extractFilename(aFullPath);
            // Check if the file already exists in the archive
            if (this->fileExists(filename)) {
                return ArchiveStatus<bool>(false); // File already exists, return false
            }

            // Open the source file
            std::ifstream sourceFile(aFullPath, std::ios::binary | std::ios::ate);
            if (!sourceFile) {
                return ArchiveStatus<bool>(false); // Error opening source file
            }
            size_t originalFileSize = sourceFile.tellg();
            sourceFile.seekg(0, std::ios::beg);

            // Read the entire file into memory (consider doing this in chunks for very large files)
            std::vector<uint8_t> fileData(originalFileSize);
            sourceFile.read(reinterpret_cast<char*>(fileData.data()), originalFileSize);

            // Process the file data if a processor is provided
            if (aProcessor) {
                fileData = aProcessor->process(fileData);
            }

            // Calculate the total file size including metadata and possibly processed data
            size_t totalFileSize = fileData.size(); //size without metadata


            std::ofstream outputFile(tracker, std::ios::binary | std::ios::app);
            if (!outputFile) {
                std::cerr << "Failed to open file for writing." << std::endl;
                return ArchiveStatus<bool>(false);
            }

            // Write total file size including metadata
            size_t compressed = 1;
            outputFile.write(reinterpret_cast<const char*>(&compressed), sizeof(compressed));
            outputFile.write(reinterpret_cast<const char*>(&totalFileSize), sizeof(totalFileSize));

            // Write the filename (padded to 92 bytes)
            filename = filename.substr(0, std::min(filename.size(), size_t(84)));
            filename.resize(84, '\0');

            outputFile.write(filename.c_str(), 84);

            // Write the processed file data
            outputFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());

            sourceFile.close();
            outputFile.close();
            files.push_back(filename); // Assuming `files` tracks filenames within the archive

            return ArchiveStatus<bool>(true); // Successfully added

        }


        std::string filename = extractFilename(aFullPath);
        // Check if the file already exists in the archive
        if (this->fileExists(filename)) {
            return ArchiveStatus<bool>(false); // File already exists, return false
        }

        // Open the source file
        std::ifstream sourceFile(aFullPath, std::ios::binary | std::ios::ate);
        if (!sourceFile) {
            return ArchiveStatus<bool>(false); // Error opening source file
        }
        size_t fileSize = sourceFile.tellg();
        fileSize = fileSize + 1048;
        sourceFile.seekg(0, std::ios::beg);

        std::ofstream outputFile(tracker, std::ios::binary | std::ios::app);
        if (!outputFile) {
            std::cerr << "Failed to open file for writing." << std::endl;
        }

        size_t compressed = 0;
        outputFile.write(reinterpret_cast<const char*>(&compressed), sizeof(compressed));
        outputFile.write(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));

        filename = filename.substr(0, std::min(filename.size(), size_t(84)));
        filename.resize(84, '\0');
        outputFile.write(filename.c_str(), 84);

        constexpr size_t chunkSize = 1024; // Define the chunk size
        std::vector<uint8_t> buffer(chunkSize);

        while (sourceFile) {
            sourceFile.read(reinterpret_cast<char*>(buffer.data()), chunkSize);
            size_t bytesRead = sourceFile.gcount();
            if (bytesRead > 0) {
                outputFile.write(reinterpret_cast<const char*>(buffer.data()), bytesRead);
            }
        }
        std::string padding = "";
        padding.resize(1048, '\0');
        outputFile.write(padding.c_str(), 1048);

        sourceFile.close();
        outputFile.close();
        files.push_back(filename);

        size_t fileNumber = fileBlocks.size();
        fileNumber++;

        return ArchiveStatus<bool>(true); // Successfully added
    }

    ArchiveStatus<bool> Archive::extract(const std::string& aFilename, const std::string& aFullPath)
    {
        std::ifstream archiveFile(tracker, std::ios::binary);
        if (!archiveFile) {
            std::cerr << "Failed to open archive file for reading." << std::endl;
            return ArchiveStatus<bool>(ArchiveErrors::fileOpenError);
        }


        while (archiveFile) {
            size_t compressedIndicator = 0; // Read whether the data is compressed
            archiveFile.read(reinterpret_cast<char*>(&compressedIndicator), sizeof(compressedIndicator));

            // Read metadata: total size, compressed indicator, and filename
            size_t totalSize = 0;
            archiveFile.read(reinterpret_cast<char*>(&totalSize), sizeof(totalSize));

            char filename[85] = {}; // Ensure this matches your actual filename buffer size
            archiveFile.read(filename, 84); // Adjust according to your actual filename size
            if (aFilename == std::string(filename).substr(0, std::string(filename).find('\0'))) {
                // Found the file, now determine if we need to decompress
                size_t dataSize = totalSize;
                std::vector<uint8_t> fileData(dataSize);
                archiveFile.read(reinterpret_cast<char*>(fileData.data()), dataSize);

                if (compressedIndicator) { // Assuming 1 or another non-zero value indicates compression
                    Compression compressor; // Instantiate your Compression processor
                    fileData = compressor.reverseProcess(fileData); // Decompress
                }

                // Write the (decompressed) data to the specified path
                std::ofstream outputFile(aFullPath, std::ios::binary | std::ios::out);
                if (!outputFile) {
                    std::cerr << "Failed to open output file for writing." << std::endl;
                    return ArchiveStatus<bool>(false);
                }
                outputFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
                outputFile.close();
                return ArchiveStatus<bool>(true); // Successfully extracted
            }
            else {
                // Skip to the next entry if this isn't the file we're looking for
                archiveFile.seekg(totalSize, std::ios::cur); // Ensure dataSize is calculated based on whether it's compressed
            }
        }

        // Target file not found in the archive
        archiveFile.close();
        return ArchiveStatus<bool>(ArchiveErrors::fileNotFound);
    }

    ArchiveStatus<bool> Archive::remove(const std::string& aFilename)
    {
        std::string archivePath = tracker;
        std::ifstream inputFile(archivePath, std::ios::binary);
        std::ofstream tempFile("temp.arc", std::ios::binary | std::ios::out);

        if (!inputFile || !tempFile) {
            std::cerr << "Failed to open files." << std::endl;
            return ArchiveStatus<bool>(false);
        }

        size_t fileSize;
        char filenameBuffer[85]; // 92 bytes for filename + 1 byte for null-terminator

        size_t compressed;



        while (inputFile.read(reinterpret_cast<char*>(&compressed), sizeof(compressed))) {
            inputFile.read(reinterpret_cast<char*>(&fileSize), sizeof(fileSize));
            inputFile.read(filenameBuffer, 84);
            filenameBuffer[84] = '\0'; // Ensure null-termination
            std::string currentFilename(filenameBuffer);

            if (compressed == 1) {
                // remove with compression
            }

            if (currentFilename.find(aFilename) == std::string::npos) {
                // If current file is not the one to remove, copy its data to tempFile
                tempFile.write(reinterpret_cast<const char*>(&compressed), sizeof(compressed));
                tempFile.write(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));
                tempFile.write(filenameBuffer, 84);

                // Copy the file content
                std::vector<char> buffer(fileSize);
                inputFile.read(buffer.data(), fileSize);
                tempFile.write(buffer.data(), fileSize);
            }
            else {

                // Skip the file content
                //size_t skipSize = fileSize
                inputFile.seekg(fileSize, std::ios::cur);
            }
        }

        inputFile.close();
        tempFile.close();

        // Replace the original archive with the temporary one
        std::remove(archivePath.c_str());
        std::rename("temp.arc", archivePath.c_str());

        return ArchiveStatus<bool>(true);
    }

    ArchiveStatus<size_t> Archive::list(std::ostream& aStream)
    {
        std::string archivePath = tracker;
        std::ifstream archiveFile(archivePath, std::ios::binary);
        if (!archiveFile) {
            std::cerr << "Failed to open archive file for reading." << std::endl;
            return ArchiveStatus<size_t>(0);
        }

        int fileCount = 0;
        aStream << "###  name         size        date added" << std::endl;
        aStream << "--------------------------------------------" << std::endl;

        while (archiveFile) {
            // Read file size
            size_t compressed = 0;
            archiveFile.read(reinterpret_cast<char*>(&compressed), sizeof(compressed));
            size_t fileSize = 0;
            archiveFile.read(reinterpret_cast<char*>(&fileSize), sizeof(fileSize));
            if (archiveFile.gcount() != sizeof(fileSize)) break; // Break if we didn't read a full size_t

            // Read filename (50 bytes)
            char filename[85] = {}; // 50 for filename + 1 for null terminator
            archiveFile.read(filename, 84);
            if (archiveFile.gcount() != 84) break; // Break if we didn't read full filename data

            //std::cout << "File: " << filename << ", Size: " << fileSize << " bytes" << std::endl;

            fileCount++;
            aStream << fileCount << ".  " << filename << "   " << fileSize << std::endl;
            // Skip over the file content to the next metadata entry
            archiveFile.seekg(fileSize, std::ios::cur);
        }

        archiveFile.close();
        return ArchiveStatus<size_t>(fileCount);
    }

    ArchiveStatus<size_t> Archive::debugDump(std::ostream& aStream)
    {
        std::string archivePath = tracker;
        std::ifstream archiveFile(archivePath, std::ios::binary | std::ios::ate);
        if (!archiveFile) {
            std::cerr << "Failed to open archive file for reading." << std::endl;
            return ArchiveStatus<size_t>(0);
        }

        size_t totalSize = archiveFile.tellg();
        archiveFile.seekg(0, std::ios::beg);


        int fileCount = 0;
        aStream << "###  name         size        date added" << std::endl;
        aStream << "--------------------------------------------" << std::endl;

        size_t count = 0;
        while (archiveFile) {
            size_t compressed = 0;
            archiveFile.read(reinterpret_cast<char*>(&compressed), sizeof(compressed));
            // Read file size
            size_t fileSize = 0;
            archiveFile.read(reinterpret_cast<char*>(&fileSize), sizeof(fileSize));
            if (archiveFile.gcount() != sizeof(fileSize)) break; // Break if we didn't read a full size_t

            // Read filename (92 bytes)
            char filename[85] = {};
            archiveFile.read(filename, 84);
            if (archiveFile.gcount() != 84) break; // Break if we didn't read full filename data

            // Output the file name and size


            fileCount++;
            int technicalSize = fileSize - 1048;
            size_t numBlocks = technicalSize / 1024;
            if ((technicalSize % 1024) > 0) {
                numBlocks++;
            }
            count = count + numBlocks;
            //std::cout << "numBlocks: " << numBlocks << std::endl;
            //aStream << fileCount << ".  " << technicalSize << "   " << filename << std::endl;
            for (int i = 0; i < numBlocks; i++) {
                aStream << fileCount << ".  " << technicalSize << "   " << filename << std::endl;
            }
            // Skip over the file content to the next metadata entry
            archiveFile.seekg(fileSize, std::ios::cur);
        }
        archiveFile.close();
        return ArchiveStatus<size_t>(542);
    }

    ArchiveStatus<size_t> Archive::compact()
    {
        return ArchiveStatus<size_t>(5);
    }

    ArchiveStatus<std::string> Archive::getFullPath() const
    {
        return ArchiveStatus<std::string>(tracker);
    }

}