#include <gtest/gtest.h>

TEST(Hello,a0){
    EXPECT_EQ(7 * 6, 42);
}

int main(int argc,char * argv[]){
    testing::InitGoogleTest(&argc,argv);

    return RUN_ALL_TESTS();
}