//
// Copyright (c) 2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "TestUtilities.h"

#include <gtest/gtest.h>

#include <cctype>
#include <system_error>

namespace osi3::testing {

std::filesystem::path MakeTempPath(const std::string& prefix, const std::string& extension) {
    const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string name = std::string(test_info->test_suite_name()) + "_" + test_info->name();
    for (auto& ch : name) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
            ch = '_';
        }
    }
    const std::string decorated_prefix = "_" + prefix + "_";
    return std::filesystem::temp_directory_path() / (decorated_prefix + name + "." + extension);
}

void SafeRemoveTestFile(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    // Intentionally ignore errors - file may not exist
}

}  // namespace osi3::testing
