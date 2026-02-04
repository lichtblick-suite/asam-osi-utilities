//
// Copyright (c) 2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSI_UTILITIES_TEST_UTILITIES_H_
#define OSI_UTILITIES_TEST_UTILITIES_H_

#include <filesystem>
#include <string>

namespace osi3::testing {

/**
 * @brief File extensions for different OSI trace file formats.
 */
struct FileExtensions {
    static constexpr const char* kOsi = "osi";    ///< Single channel binary format
    static constexpr const char* kTxth = "txth";  ///< Text human-readable format
    static constexpr const char* kMcap = "mcap";  ///< MCAP container format
};

/**
 * @brief Creates a unique temporary file path for test files.
 *
 * The generated path includes the test suite name, test name, and a prefix
 * to ensure uniqueness across parallel test runs. The path format is:
 * {temp_dir}/_{prefix}_{TestSuiteName}_{TestName}.{extension}
 *
 * @param prefix A prefix to identify the type of test file (e.g., "gt", "sv", "mcap")
 * @param extension The file extension without the dot (e.g., "osi", "txth", "mcap")
 * @return std::filesystem::path The generated unique temporary file path
 *
 * @note This function must be called within a Google Test context where
 *       testing::UnitTest::GetInstance()->current_test_info() is valid.
 *
 * Example usage:
 * @code
 *   auto path = MakeTempPath("gt", FileExtensions::kOsi);
 *   // Results in something like: /tmp/_gt_MyTestSuite_MyTest.osi
 * @endcode
 */
std::filesystem::path MakeTempPath(const std::string& prefix, const std::string& extension);

/**
 * @brief Safely removes a test file, ignoring errors if file doesn't exist.
 *
 * @param path The path to the file to remove
 */
void SafeRemoveTestFile(const std::filesystem::path& path);

}  // namespace osi3::testing

#endif  // OSI_UTILITIES_TEST_UTILITIES_H_
