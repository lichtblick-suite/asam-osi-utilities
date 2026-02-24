//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILECHANNEL_H_
#define OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILECHANNEL_H_

#include <google/protobuf/message.h>

#include <mcap/mcap.hpp>

namespace osi3 {

/**
 * @brief OSI channel helper for integration with external MCAP writers
 *
 * This class provides OSI-compliant channel registration and message writing
 * capabilities for use with an externally-managed mcap::McapWriter. Use this
 * when you have your own MCAP writer that handles multiple topic types
 * (both OSI and non-OSI) and you want to leverage this library's OSI
 * schema/metadata handling.
 *
 * Unlike MCAPTraceFileWriter which owns the file and MCAP writer, this class
 * operates on a caller-provided McapWriter reference. The caller is responsible
 * for the writer's lifecycle (open/close) and for writing the required OSI file
 * metadata (via PrepareRequiredFileMetadata()) before writing any messages.
 *
 * @note Thread Safety: Not thread-safe. External synchronization required for concurrent access.
 * @note Lifetime: The provided McapWriter must outlive this object.
 *
 * Example usage:
 * @code
 * std::ofstream file("mixed.mcap", std::ios::binary);
 * mcap::McapWriter writer;
 * writer.open(file, mcap::McapWriterOptions("protobuf"));
 *
 * // Add OSI file metadata (required by OSI spec)
 * writer.write(osi3::MCAPTraceFileChannel::PrepareRequiredFileMetadata());
 *
 * // Create channel helper
 * osi3::MCAPTraceFileChannel osi_channels(writer);
 * osi_channels.AddChannel("ground_truth", osi3::GroundTruth::descriptor());
 *
 * // Write OSI messages
 * osi3::GroundTruth gt;
 * osi_channels.WriteMessage(gt, "ground_truth");
 *
 * // You can also write non-OSI messages directly to writer
 * writer.close();
 * @endcode
 */
class MCAPTraceFileChannel {
   public:
    /**
     * @brief Constructs a channel helper attached to an external MCAP writer
     * @param writer Reference to an open McapWriter (caller-owned, must outlive this object)
     */
    explicit MCAPTraceFileChannel(mcap::McapWriter& writer);

    /** @brief Deleted copy constructor */
    MCAPTraceFileChannel(const MCAPTraceFileChannel&) = delete;

    /** @brief Deleted copy assignment operator */
    MCAPTraceFileChannel& operator=(const MCAPTraceFileChannel&) = delete;

    /** @brief Deleted move constructor */
    MCAPTraceFileChannel(MCAPTraceFileChannel&&) = delete;

    /** @brief Deleted move assignment operator */
    MCAPTraceFileChannel& operator=(MCAPTraceFileChannel&&) = delete;

    /**
     * @brief Writes a protobuf message to the file
     * @tparam T Type of the OSI protobuf message (must have timestamp() method)
     * @param top_level_message The message to write
     * @param topic Topic name (must match a previously added channel)
     * @return true if successful, false otherwise
     * @warning Caller must write OSI file metadata (via PrepareRequiredFileMetadata()) to the
     *          McapWriter before calling this method. Without it the resulting file is non-compliant.
     */
    template <typename T>
    bool WriteMessage(const T& top_level_message, const std::string& topic);

    /**
     * @brief Adds a new OSI channel to the MCAP file
     *
     * Registers the schema (if not already registered) and creates a channel
     * with OSI-compliant metadata (osi_version, protobuf_version).
     *
     * @param topic Name of the channel/topic
     * @param descriptor Protobuf descriptor for the OSI message type
     * @param channel_metadata Additional metadata for the channel (passed by value; OSI metadata is added in-place)
     * @return Channel ID for the newly created channel
     * @throws std::runtime_error if topic already exists with a different message type
     */
    uint16_t AddChannel(const std::string& topic, const google::protobuf::Descriptor* descriptor, std::unordered_map<std::string, std::string> channel_metadata = {});

    /**
     * @brief Prepares the required (by OSI spec) metadata for the MCAP trace file
     *
     * Call this and write the result to your McapWriter before writing OSI messages.
     * This is the same metadata that MCAPTraceFileWriter adds automatically.
     *
     * @return mcap::Metadata Object containing the necessary metadata for OSI compliance
     */
    static mcap::Metadata PrepareRequiredFileMetadata();

    /**
     * @brief Helper function that returns the current time as a formatted string
     * @return Current timestamp as string in OSI-specification format
     */
    static std::string GetCurrentTimeAsString();

   private:
    mcap::McapWriter& mcap_writer_;                           /**< Non-owning reference to external writer */
    std::unordered_map<std::string, mcap::Schema> schemas_;   /**< Registered schemas (keyed by descriptor full name) */
    std::map<std::string, uint16_t> topic_to_channel_id_;     /**< Topic to channel ID mapping */
    std::map<std::string, std::string> topic_to_schema_name_; /**< Topic to schema name mapping (for duplicate detection) */
    std::string serialize_buffer_;                            /**< Reusable serialization buffer */
};

}  // namespace osi3

#endif  // OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILECHANNEL_H_
