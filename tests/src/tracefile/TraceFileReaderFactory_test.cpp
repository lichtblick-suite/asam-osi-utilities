//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include <gtest/gtest.h>

#include <filesystem>

#include "../TestUtilities.h"
#include "osi-utilities/tracefile/Reader.h"
#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"
#include "osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h"
#include "osi-utilities/tracefile/reader/TXTHTraceFileReader.h"
#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"
#include "osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h"
#include "osi-utilities/tracefile/writer/TXTHTraceFileWriter.h"
#include "osi_groundtruth.pb.h"

class TraceFileReaderFactoryTest : public ::testing::Test {};

TEST_F(TraceFileReaderFactoryTest, CreateOsiReader) {
    auto reader = osi3::TraceFileReaderFactory::createReader("trace_gt_.osi");
    ASSERT_NE(reader, nullptr);
    EXPECT_NE(dynamic_cast<osi3::SingleChannelBinaryTraceFileReader*>(reader.get()), nullptr);
}

TEST_F(TraceFileReaderFactoryTest, CreateMcapReader) {
    auto reader = osi3::TraceFileReaderFactory::createReader("trace.mcap");
    ASSERT_NE(reader, nullptr);
    EXPECT_NE(dynamic_cast<osi3::MCAPTraceFileReader*>(reader.get()), nullptr);
}

TEST_F(TraceFileReaderFactoryTest, CreateTxthReader) {
    auto reader = osi3::TraceFileReaderFactory::createReader("trace_gt_.txth");
    ASSERT_NE(reader, nullptr);
    EXPECT_NE(dynamic_cast<osi3::TXTHTraceFileReader*>(reader.get()), nullptr);
}

TEST_F(TraceFileReaderFactoryTest, ThrowOnUnsupportedExtension) { EXPECT_THROW(osi3::TraceFileReaderFactory::createReader("trace.xyz"), std::invalid_argument); }

TEST_F(TraceFileReaderFactoryTest, ThrowOnNoExtension) { EXPECT_THROW(osi3::TraceFileReaderFactory::createReader("noext"), std::invalid_argument); }

TEST_F(TraceFileReaderFactoryTest, ThrowOnEmptyPath) { EXPECT_THROW(osi3::TraceFileReaderFactory::createReader(""), std::invalid_argument); }

TEST_F(TraceFileReaderFactoryTest, FactoryThenOpenAndReadBinary) {
    // Write a test file
    const auto file_path = osi3::testing::MakeTempPath("factory_gt", osi3::testing::FileExtensions::kOsi);
    osi3::SingleChannelBinaryTraceFileWriter writer;
    ASSERT_TRUE(writer.Open(file_path));
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(99);
    ASSERT_TRUE(writer.WriteMessage(gt));
    writer.Close();

    // Use factory to create reader
    auto reader = osi3::TraceFileReaderFactory::createReader(file_path);
    ASSERT_NE(reader, nullptr);
    auto* binary_reader = dynamic_cast<osi3::SingleChannelBinaryTraceFileReader*>(reader.get());
    ASSERT_NE(binary_reader, nullptr);
    ASSERT_TRUE(binary_reader->Open(file_path));
    ASSERT_TRUE(binary_reader->HasNext());

    const auto result = binary_reader->ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);

    binary_reader->Close();
    osi3::testing::SafeRemoveTestFile(file_path);
}

TEST_F(TraceFileReaderFactoryTest, FactoryThenOpenAndReadMcap) {
    const auto file_path = osi3::testing::MakeTempPath("factory", osi3::testing::FileExtensions::kMcap);
    osi3::MCAPTraceFileWriter writer;
    ASSERT_TRUE(writer.Open(file_path));
    writer.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
    writer.AddChannel("gt", osi3::GroundTruth::descriptor());
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(99);
    ASSERT_TRUE(writer.WriteMessage(gt, "gt"));
    writer.Close();

    auto reader = osi3::TraceFileReaderFactory::createReader(file_path);
    ASSERT_NE(reader, nullptr);
    ASSERT_TRUE(reader->Open(file_path));
    ASSERT_TRUE(reader->HasNext());

    const auto result = reader->ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);

    reader->Close();
    osi3::testing::SafeRemoveTestFile(file_path);
}

TEST_F(TraceFileReaderFactoryTest, FactoryThenOpenAndReadTxth) {
    const auto file_path = osi3::testing::MakeTempPath("factory_gt", osi3::testing::FileExtensions::kTxth);
    osi3::TXTHTraceFileWriter writer;
    ASSERT_TRUE(writer.Open(file_path));
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(99);
    ASSERT_TRUE(writer.WriteMessage(gt));
    writer.Close();

    auto reader = osi3::TraceFileReaderFactory::createReader(file_path);
    ASSERT_NE(reader, nullptr);
    ASSERT_TRUE(reader->Open(file_path));
    ASSERT_TRUE(reader->HasNext());

    const auto result = reader->ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);

    reader->Close();
    osi3::testing::SafeRemoveTestFile(file_path);
}
