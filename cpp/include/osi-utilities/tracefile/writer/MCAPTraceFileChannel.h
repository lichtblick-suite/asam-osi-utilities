//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILECHANNEL_H_
#define OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILECHANNEL_H_

#include <google/protobuf/message.h>

#include <array>
#include <cstdint>
#include <map>
#include <mcap/mcap.hpp>
#include <optional>
#include <string>
#include <unordered_map>

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
 * metadata (via PrepareRequiredFileMetadata()).
 *
 * Every OSI channel carries an OSI version label (``net.asam.osi.trace.channel.osi_version``):
 * the caller-provided value, else the message's ``InterfaceVersion`` for channels
 * auto-registered by WriteMessage(), else the linked OSI library version. Use
 * FillOsiVersionRange() to make the file's ``min/max_osi_version`` span exactly these labels.
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
 * // Create channel helper and register the OSI channels
 * osi3::MCAPTraceFileChannel osi_channels(writer);
 * osi_channels.AddChannel("ground_truth", osi3::GroundTruth::descriptor());
 *
 * // Add OSI file metadata (required by OSI spec), with the OSI version range of the channels
 * auto metadata = osi3::MCAPTraceFileChannel::PrepareRequiredFileMetadata();
 * osi_channels.FillOsiVersionRange(metadata);
 * writer.write(metadata);
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
     * @brief Writes a protobuf message to the file (type-erased)
     *
     * Uses protobuf reflection to serialize the message and extract its timestamp.
     * If the topic has not been registered via AddChannel(), it is auto-registered
     * using the message's protobuf descriptor, labelled with the message's embedded
     * OSI ``InterfaceVersion`` (or the linked OSI library version if it carries none).
     * A warning is logged (once per channel) if a message's ``InterfaceVersion``
     * differs from its channel's OSI version label.
     *
     * @param message The protobuf message to write
     * @param topic Topic name (must not be empty)
     * @return true if successful, false otherwise
     */
    bool WriteMessage(const google::protobuf::Message& message, const std::string& topic);

    /**
     * @brief Writes a protobuf message to the file (compile-time typed)
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
     * with OSI-compliant metadata (osi_version, protobuf_version). Pass
     * ``net.asam.osi.trace.channel.osi_version`` in @p channel_metadata to declare the OSI
     * version of the channel's data; it defaults to the linked OSI library version.
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
     * Write the result to your McapWriter. ``min/max_osi_version`` default to the linked OSI
     * library version; call FillOsiVersionRange() to narrow them to the registered channels.
     * This is the same metadata that MCAPTraceFileWriter adds automatically.
     *
     * @return mcap::Metadata Object containing the necessary metadata for OSI compliance
     */
    static mcap::Metadata PrepareRequiredFileMetadata();

    /**
     * @brief Sets ``min_osi_version``/``max_osi_version`` to the range of the registered channels' OSI versions
     *
     * Leaves @p metadata unchanged if no channel has a parseable OSI version label.
     *
     * @param metadata File metadata (typically from PrepareRequiredFileMetadata()) to update in-place
     */
    void FillOsiVersionRange(mcap::Metadata& metadata) const;

    /**
     * @brief Forgets all registered schemas and channels
     *
     * Call when the attached McapWriter is closed and reopened for a new file.
     */
    void Reset();

    /**
     * @brief Helper function that returns the current time as a formatted string
     * @return Current timestamp as string in OSI-specification format
     */
    static std::string GetCurrentTimeAsString();

   private:
    /** @brief Per-topic channel state */
    struct ChannelInfo {
        uint16_t id = 0;                                    /**< MCAP channel ID */
        std::string schema_name;                            /**< Schema name (for duplicate detection) */
        std::optional<std::array<uint32_t, 3>> osi_version; /**< Parsed channel osi_version label */
        bool osi_version_mismatch_reported = false;         /**< Whether a label/message mismatch was already logged */
    };

    /** @brief Logs (once per channel) if the message's InterfaceVersion differs from the channel label */
    void CheckMessageOsiVersion(ChannelInfo& channel, const google::protobuf::Message& message, const std::string& topic);

    /** @brief Serializes and writes a message with the given log time to an already registered channel */
    bool WriteToChannel(ChannelInfo& channel, const google::protobuf::Message& message, const std::string& topic, uint64_t log_time);

    mcap::McapWriter& mcap_writer_;                         /**< Non-owning reference to external writer */
    std::unordered_map<std::string, mcap::Schema> schemas_; /**< Registered schemas (keyed by descriptor full name) */
    std::map<std::string, ChannelInfo> channels_;           /**< Registered channels, keyed by topic */
    std::string serialize_buffer_;                          /**< Reusable serialization buffer */
};

}  // namespace osi3

#endif  // OSIUTILITIES_TRACEFILE_WRITER_MCAPTRACEFILECHANNEL_H_
