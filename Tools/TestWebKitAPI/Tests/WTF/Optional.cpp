/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/Optional.h>

namespace TestWebKitAPI {

TEST(WTF_Optional, Disengaged)
{
    {
        std::optional<int> optional;

        EXPECT_FALSE(static_cast<bool>(optional));
    }

    {
        std::optional<int> optional { std::nullopt };

        EXPECT_FALSE(static_cast<bool>(optional));
    }
}

TEST(WTF_Optional, Engaged)
{
    std::optional<int> optional { 10 };

    EXPECT_TRUE(static_cast<bool>(optional));
    EXPECT_EQ(10, optional.value());
}

TEST(WTF_Optional, Destructor)
{
    static bool didCallDestructor = false;
    struct A {
        ~A()
        {
            EXPECT_FALSE(didCallDestructor);
            didCallDestructor = true;
        }
    };

    {
        std::optional<A> optional { std::in_place };

        EXPECT_TRUE(static_cast<bool>(optional));
    }

    EXPECT_TRUE(didCallDestructor);
}

TEST(WTF_Optional, Callback)
{
    bool called = false;
    std::optional<int> a;
    int result = valueOrCompute(a, [&] {
        called = true;
        return 300;
    });
    EXPECT_TRUE(called);
    EXPECT_EQ(result, 300);

    a = 250;
    called = false;
    result = valueOrCompute(a, [&] {
        called = true;
        return 300;
    });
    EXPECT_FALSE(called);
    EXPECT_EQ(result, 250);
}

TEST(WTF_Optional, Equality)
{
    std::optional<int> unengaged1;
    std::optional<int> unengaged2;

    std::optional<int> engaged1 { 1 };
    std::optional<int> engaged2 { 2 };
    std::optional<int> engagedx2 { 2 };

    EXPECT_TRUE(unengaged1 == unengaged2);
    EXPECT_FALSE(engaged1 == engaged2);
    EXPECT_FALSE(engaged1 == unengaged1);
    EXPECT_TRUE(engaged2 == engagedx2);

    EXPECT_TRUE(unengaged1 == std::nullopt);
    EXPECT_FALSE(engaged1 == std::nullopt);
    EXPECT_TRUE(std::nullopt == unengaged1);
    EXPECT_FALSE(std::nullopt == engaged1);

    EXPECT_TRUE(engaged1 == 1);
    EXPECT_TRUE(1 == engaged1);
    EXPECT_FALSE(unengaged1 == 1);
    EXPECT_FALSE(1 == unengaged1);
}

TEST(WTF_Optional, Inequality)
{
    std::optional<int> unengaged1;
    std::optional<int> unengaged2;

    std::optional<int> engaged1 { 1 };
    std::optional<int> engaged2 { 2 };
    std::optional<int> engagedx2 { 2 };

    EXPECT_FALSE(unengaged1 != unengaged2);
    EXPECT_TRUE(engaged1 != engaged2);
    EXPECT_TRUE(engaged1 != unengaged1);
    EXPECT_FALSE(engaged2 != engagedx2);

    EXPECT_FALSE(unengaged1 != std::nullopt);
    EXPECT_TRUE(engaged1 != std::nullopt);
    EXPECT_FALSE(std::nullopt != unengaged1);
    EXPECT_TRUE(std::nullopt != engaged1);

    EXPECT_FALSE(engaged1 != 1);
    EXPECT_TRUE(engaged1 != 2);
    EXPECT_FALSE(1 != engaged1);
    EXPECT_TRUE(2 != engaged1);

    EXPECT_TRUE(unengaged1 != 1);
    EXPECT_TRUE(1 != unengaged1);
}


} // namespace TestWebKitAPI
