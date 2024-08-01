// Copyright (c) 2022 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef IOX_EXAMPLES_REQUEST_AND_RESPONSE_TYPES_HPP
#define IOX_EXAMPLES_REQUEST_AND_RESPONSE_TYPES_HPP

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <stdio.h>

#include <string>



#define SERVICE_FLAG "interceptservice"
#define DUMMY_INSTANCE_FLAG "dummyserver"
#define INTERCEPT_INSTANCE_FLAG "interceptserver"

#define ICEORYX "ICEORYX"

namespace intercept {
namespace internal {
//! [request]
struct AddRequest
{
    uint64_t augend{0};
    uint64_t addend{0};
};
//! [request]

//! [response]
struct AddResponse
{
    uint64_t sum{0};
};
//! [response]

struct UserRequest
{
    uint64_t pid{0};
    uint64_t threadid{0};
};

struct UserResponse
{
    uint64_t pid{0};
    uint64_t threadid{0};
};


struct Metainfo {
    int type = 0;
    int fd = 0;
    size_t count = 0;
};



struct ServiceMetaInfo {
    std::string service = "";
    std::string instance = "";
    std::string event = "";
    std::string serverType = ""; // server类型 : normal dummy
};

} // namespace internal
} // namespace intercept


#define MAX_LENGTH 2000000
// 生成随机字符，不包括 '\0'
// char randomChar() {
//     const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
//     return charset[rand() % (sizeof(charset) - 1)];
// }

// // 生成随机字符串
// char* generateRandomString(size_t length) {
//     if (length > MAX_LENGTH) {
//         fprintf(stderr, "String length is too long.\n");
//     }

//     char *str = (char*)malloc((length + 1) * sizeof(char)); // +1 为字符串的终止符 '\0' 预留空间
//     if (str == NULL) {
//         perror("malloc");
//     }

//     for (size_t i = 0; i < length; ++i) {
//         str[i] = randomChar();
//     }
//     str[length] = '\0'; // 确保字符串以空字符结尾

//     return str;
// }



#endif // IOX_EXAMPLES_REQUEST_AND_RESPONSE_TYPES_HPP
