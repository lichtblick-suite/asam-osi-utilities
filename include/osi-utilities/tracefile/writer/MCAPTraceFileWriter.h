//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILEWRITER_H_
#define OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILEWRITER_H_

#include <google/protobuf/message.h>

#include <mcap/mcap.hpp>

#include "../Writer.h"

namespace osi3 {
/**
 * @brief MCAP format implementation of the trace file writer
 *
 * Handles writing OSI messages to MCAP format files with support for
 * channels, schemas, and metadata.
 */
class MCAPTraceFileWriter final : public osi3::TraceFileWriter {
   public:
    /**
     * @brief Opens a trace file for writing using default MCAP options
     * @param file_path Path to the file to be created/opened
     * @return true if successful, false otherwise
     */
    bool Open(const std::filesystem::path& file_path) override;

    /**
     * @brief Opens a file for writing with additional options
     *
     * This alternative opening method allows specifying additional options
     * like compression and chunk size.
     *
     * @param file_path Path to the file to be created/opened
     * @param options Options for the MCAP writer
     * @return true if successful, false otherwise
     */
    bool Open(const std::filesystem::path& file_path, const mcap::McapWriterOptions& options);

    /**
     * @brief Writes a protobuf message to the file
     * @tparam T Type of the protobuf message
     * @param top_level_message The message to write
     * @param topic Optional topic name for the message
     * @return true if successful, false otherwise
     */
    template <typename T>
    bool WriteMessage(const T& top_level_message, const std::string& topic = "");

    /**
     * @brief Adds metadata for the trace file
     * @param metadata MCAP metadata object
     * @return true if successful, false otherwise
     */
    bool AddFileMetadata(const mcap::Metadata& metadata);

    /**
     * @brief Adds metadata for the trace file
     *
     * This overload allows for specification of metadata entries by name and key-value pairs
     *
     * @param name Name of the metadata entry
     * @param metadata_entries Key-value pairs of metadata
     * @return true if successful, false otherwise
     */
    bool AddFileMetadata(const std::string& name, const std::unordered_map<std::string, std::string>& metadata_entries);

    /**
     * @brief Prepares the required (by OSI spec.) metadata for the MCAP trace file.
     *
     * This function initializes and populates an `mcap::Metadata` object with information
     * specific to the OSI (Open Simulation Interface) trace format and its versioning.
     * The metadata includes details about the OSI version, protobuf version.
     *
     * @return mcap::Metadata Object containing the necessary metadata for the MCAP trace file.
     */
    static mcap::Metadata PrepareRequiredFileMetadata();

    /**
     * @brief Adds a new channel to the MCAP file
     * @param topic Name of the channel/topic
     * @param descriptor Protobuf descriptor for the message type
     * @param channel_metadata Additional metadata for the channel
     * @return Channel ID for the newly created channel
     */
    uint16_t AddChannel(const std::string& topic, const google::protobuf::Descriptor* descriptor, std::unordered_map<std::string, std::string> channel_metadata = {});

    /**
     * @brief Helper function that returns the current time as a formatted string
     * @return Current timestamp as string in required format
     *
     * This helper function is intended to be used when creating metadata entries
     * that require timestamps, particularly for the OSI-specification mandatory metadata.
     * The timestamp format follows the OSI specification for zero_time and creation_time metadata.
     */
    static std::string GetCurrentTimeAsString();

    /**
     * @brief Closes the trace file and finalizes MCAP output
     */
    void Close() override;

    /**
     * @brief Gets the underlying MCAP writer instance
     * @return Pointer to the internal McapWriter object
     *
     * This function can be useful for advanced operations
     * like adding non-OSI message which requires direct access to
     * underlying the MCAP writer.
     */
    mcap::McapWriter* GetMcapWriter() { return &mcap_writer_; }

   private:
    std::ofstream trace_file_;                            /**< Trace file stream */
    mcap::McapWriter mcap_writer_;                        /**< MCAP writer instance */
    mcap::McapWriterOptions mcap_options_{"protobuf"};    /**< MCAP writer configuration */
    std::vector<mcap::Schema> schemas_;                   /**< Registrated schemas */
    std::map<std::string, uint16_t> topic_to_channel_id_; /**< Topic to channel ID mapping */
    bool required_metadata_added_ = false;                /**< Flag to track if required metadata has been added */

    /**
     * @brief Adds standard metadata to the MCAP file
     *
     * Includes OSI version information and file creation timestamp
     */
    void AddVersionToFileMetadata();
};
}  // namespace osi3
#endif  // OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILEWRITER_H_
