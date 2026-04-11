#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include "blast.h"
}

namespace fs = std::filesystem;

namespace {

struct DirectoryEntry {
    std::string name;
    uint16_t file_count;
};

#pragma pack(push, 1)
struct Header {
    uint32_t signature1;
    uint32_t signature2;
    uint16_t u1;
    uint16_t is_multivolume;
    uint16_t file_count;
    uint32_t datetime;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t u2;
    uint8_t volume_total;
    uint8_t volume_number;
    uint8_t u3;
    uint32_t split_begin_address;
    uint32_t split_end_address;
    uint32_t toc_address;
    uint32_t u4;
    uint16_t dir_count;
    uint32_t u5;
    uint32_t u6;
};
#pragma pack(pop)

struct FileEntry {
    enum Attributes : uint8_t {
        ReadOnly = 0x01,
        Hidden = 0x02,
        System = 0x04,
        Uncompressed = 0x10,
        Archive = 0x20,
    };

    std::string name;
    std::string full_path;
    uint16_t index = 0;
    uint32_t compressed_size = 0;
    uint32_t uncompressed_size = 0;
    uint32_t datetime = 0;
    uint8_t attrib = 0;
    uint32_t offset = 0;
    uint8_t is_split = 0;
    uint8_t volume_start = 0;
    uint8_t volume_end = 0;

    std::tm dos_tm() const {
        const uint16_t file_date = static_cast<uint16_t>(datetime & 0xffff);
        const uint16_t file_time = static_cast<uint16_t>((datetime >> 16) & 0xffff);
        std::tm out = {};
        out.tm_sec = (file_time & 0x1f) * 2;
        out.tm_min = (file_time >> 5) & 0x3f;
        out.tm_hour = (file_time >> 11) & 0x1f;
        out.tm_mday = file_date & 0x1f;
        out.tm_mon = ((file_date >> 5) & 0x0f) - 1;
        out.tm_year = (((file_date >> 9) & 0x7f) + 1980) - 1900;
        out.tm_isdst = -1;
        return out;
    }

    fs::path relative_path() const {
        std::string native = full_path;
        std::replace(native.begin(), native.end(), '\\', fs::path::preferred_separator);
        return fs::path(native);
    }
};

template <typename T>
T read_or_throw(std::ifstream &stream) {
    T value {};
    stream.read(reinterpret_cast<char *>(&value), sizeof(value));
    if (!stream) {
        throw std::runtime_error("unexpected EOF");
    }
    return value;
}

std::string read_string8(std::ifstream &stream) {
    const auto len = read_or_throw<uint8_t>(stream);
    std::string value(len, '\0');
    stream.read(value.data(), len);
    if (!stream) {
        throw std::runtime_error("unexpected EOF while reading string8");
    }
    return value;
}

std::string read_string16(std::ifstream &stream) {
    const auto len = read_or_throw<uint16_t>(stream);
    std::string value(len, '\0');
    stream.read(value.data(), len);
    if (!stream) {
        throw std::runtime_error("unexpected EOF while reading string16");
    }
    return value;
}

bool is_valid_name(const std::string &name) {
    return name.find("..\\") == std::string::npos && name.find("../") == std::string::npos;
}

std::string format_tm(const std::tm &tm) {
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

struct BlastInput {
    const std::vector<unsigned char> *data = nullptr;
    bool served = false;
};

unsigned blast_in_cb(void *how, unsigned char **buf) {
    auto *input = static_cast<BlastInput *>(how);
    if (input->served) {
        *buf = nullptr;
        return 0;
    }
    input->served = true;
    *buf = const_cast<unsigned char *>(input->data->data());
    return static_cast<unsigned>(input->data->size());
}

int blast_out_cb(void *how, unsigned char *buf, unsigned len) {
    auto *out = static_cast<std::vector<unsigned char> *>(how);
    out->insert(out->end(), buf, buf + len);
    return 0;
}

class Archive {
  public:
    explicit Archive(const fs::path &path) : path_(path), stream_(path, std::ios::binary) {
        if (!stream_) {
            throw std::runtime_error("cannot open archive: " + path.string());
        }

        stream_.read(reinterpret_cast<char *>(&header_), sizeof(header_));
        if (!stream_) {
            throw std::runtime_error("cannot read archive header");
        }
        if (header_.signature1 != 0x8c655d13 || header_.signature2 != 0x02013a) {
            throw std::runtime_error("unsupported archive signature");
        }

        stream_.seekg(header_.toc_address, std::ios::beg);
        if (!stream_) {
            throw std::runtime_error("cannot seek to archive TOC");
        }

        std::vector<DirectoryEntry> directories;
        directories.reserve(header_.dir_count);
        for (uint16_t i = 0; i < header_.dir_count; ++i) {
            const auto file_count = read_or_throw<uint16_t>(stream_);
            const auto chunk_size = read_or_throw<uint16_t>(stream_);
            const auto name = read_string16(stream_);
            const auto skip = static_cast<std::streamoff>(chunk_size) -
                static_cast<std::streamoff>(name.size()) - 6;
            if (skip < 0) {
                throw std::runtime_error("invalid directory chunk");
            }
            stream_.ignore(skip);
            directories.push_back({name, file_count});
        }

        for (const auto &directory : directories) {
            for (uint16_t i = 0; i < directory.file_count; ++i) {
                FileEntry file;
                file.volume_end = read_or_throw<uint8_t>(stream_);
                file.index = read_or_throw<uint16_t>(stream_);
                file.uncompressed_size = read_or_throw<uint32_t>(stream_);
                file.compressed_size = read_or_throw<uint32_t>(stream_);
                file.offset = read_or_throw<uint32_t>(stream_);
                file.datetime = read_or_throw<uint32_t>(stream_);
                (void)read_or_throw<uint32_t>(stream_);
                const auto chunk_size = read_or_throw<uint16_t>(stream_);
                file.attrib = read_or_throw<uint8_t>(stream_);
                file.is_split = read_or_throw<uint8_t>(stream_);
                (void)read_or_throw<uint8_t>(stream_);
                file.volume_start = read_or_throw<uint8_t>(stream_);
                file.name = read_string8(stream_);

                const auto skip = static_cast<std::streamoff>(chunk_size) -
                    static_cast<std::streamoff>(file.name.size()) - 30;
                if (skip < 0) {
                    throw std::runtime_error("invalid file chunk");
                }
                stream_.ignore(skip);

                file.full_path = directory.name.empty()
                    ? file.name
                    : (directory.name + "\\" + file.name);
                if (!is_valid_name(file.full_path)) {
                    throw std::runtime_error("invalid archive path: " + file.full_path);
                }
                files_.push_back(std::move(file));
            }
        }
    }

    const Header &header() const { return header_; }
    const std::vector<FileEntry> &files() const { return files_; }

    std::vector<unsigned char> extract_file(const FileEntry &file) {
        stream_.seekg(file.offset, std::ios::beg);
        if (!stream_) {
            throw std::runtime_error("cannot seek to file data");
        }

        std::vector<unsigned char> input(file.compressed_size);
        stream_.read(reinterpret_cast<char *>(input.data()),
            static_cast<std::streamsize>(input.size()));
        if (!stream_) {
            throw std::runtime_error("cannot read compressed payload");
        }

        if (file.attrib & FileEntry::Uncompressed) {
            return input;
        }

        std::vector<unsigned char> output;
        output.reserve(file.uncompressed_size);
        BlastInput blast_input {&input, false};
        unsigned left = 0;
        unsigned char *unused = nullptr;
        const int rc = blast(
            blast_in_cb,
            &blast_input,
            blast_out_cb,
            &output,
            &left,
            &unused);
        if (rc != 0) {
            std::ostringstream msg;
            msg << "blast decompression failed for " << file.full_path << " with code " << rc;
            throw std::runtime_error(msg.str());
        }
        return output;
    }

  private:
    fs::path path_;
    std::ifstream stream_;
    Header header_ {};
    std::vector<FileEntry> files_;
};

void cmd_info(const fs::path &archive_path) {
    Archive archive(archive_path);
    const auto &header = archive.header();
    std::cout << "Archive: " << archive_path << "\n";
    std::cout << "File count: " << header.file_count << "\n";
    std::cout << "Compressed size: " << header.compressed_size << "\n";
    std::cout << "Uncompressed size: " << header.uncompressed_size << "\n";
    std::cout << "TOC address: " << header.toc_address << "\n";
    std::cout << "Directory count: " << header.dir_count << "\n";
}

void cmd_list(const fs::path &archive_path, bool verbose) {
    Archive archive(archive_path);
    if (!verbose) {
        for (const auto &file : archive.files()) {
            std::cout << file.full_path << "\n";
        }
        return;
    }

    for (const auto &file : archive.files()) {
        const auto tm = file.dos_tm();
        std::cout << std::left << std::setw(24) << file.full_path
                  << " size=" << std::setw(8) << file.uncompressed_size
                  << " csize=" << std::setw(8) << file.compressed_size
                  << " off=" << std::setw(8) << file.offset
                  << " date=" << format_tm(tm)
                  << " attr=0x" << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(file.attrib)
                  << std::dec << std::setfill(' ') << "\n";
    }
}

void cmd_extract(const fs::path &archive_path, const fs::path &destination) {
    Archive archive(archive_path);
    fs::create_directories(destination);

    for (const auto &file : archive.files()) {
        auto contents = archive.extract_file(file);
        const auto output_path = destination / file.relative_path();
        fs::create_directories(output_path.parent_path());
        std::ofstream out(output_path, std::ios::binary);
        if (!out) {
            throw std::runtime_error("cannot create output file: " + output_path.string());
        }
        out.write(reinterpret_cast<const char *>(contents.data()),
            static_cast<std::streamsize>(contents.size()));
        if (!out) {
            throw std::runtime_error("cannot write output file: " + output_path.string());
        }
        std::cout << output_path << "\n";
    }
}

void usage(const char *argv0) {
    std::cerr
        << "usage:\n"
        << "  " << argv0 << " info ARCHIVE\n"
        << "  " << argv0 << " list [-v] ARCHIVE\n"
        << "  " << argv0 << " extract ARCHIVE DESTDIR\n";
}

}  // namespace

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            usage(argv[0]);
            return 1;
        }

        const std::string command = argv[1];
        if (command == "info" && argc == 3) {
            cmd_info(argv[2]);
            return 0;
        }
        if (command == "list" && argc == 3) {
            cmd_list(argv[2], false);
            return 0;
        }
        if (command == "list" && argc == 4 && std::string(argv[2]) == "-v") {
            cmd_list(argv[3], true);
            return 0;
        }
        if (command == "extract" && argc == 4) {
            cmd_extract(argv[2], argv[3]);
            return 0;
        }

        usage(argv[0]);
        return 1;
    } catch (const std::exception &ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
