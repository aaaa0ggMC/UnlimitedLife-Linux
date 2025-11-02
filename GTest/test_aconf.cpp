#include <gtest/gtest.h>
#include <alib-g3/aconf.h>
#include <print>
#include <chrono>

using namespace alib::g3;


TEST(aconf, complex_config) {
    Config cfg;
    auto err = cfg.load(
        R"(
root {
  # global settings ;
  version "1.0.0";
  name "Test Config \"Complex\"";

  server {
    host "127.0.0.1";
    port 8080;
    ssl {
      enable true;
      key_path "/etc/ssl/private/server.key";
      cert_path "/etc/ssl/certs/server.crt";
    };
  };

  database {
    type sqlite;
    path "/var/data/app.db";
    pool {
      size 8;
      timeout 30 s;
    };
  };

  routes {
    location "/api/v1/{path}" {
      method GET;
      handler proxy;
      target {
        host "localhost";
        port 9000;
      };
    };

    location "/static/{file}" {
      method GET;
      handler file;
      root "/home/user/static";
    };
  };

  # test tricky strings ;
  test_strings {
    literal "a string with spaces";
    escaped "line1\nline2\tTabbed";
    mixed "value with { and } inside";
    quoted "\"double quoted inside\"";
    path "C:\\Program Files\\Test\\";
  };

  # multiple nodes with same name ;
  worker { id 1; role "listener"; };
  worker { id 2; role "processor"; };
  worker { id 3; role "writer"; };

  # empty nodes ;
  dummy { };
};
)"
    );

    cfg.root.print_node(1);
    std::print("错误信息：{} at {}\n", (int)err.code, err.line);

    // 检查一些关键路径
    EXPECT_EQ(cfg.root.get_node_recursive_value({"root", "server", "ssl", "enable"}), "true");
    EXPECT_EQ(cfg.root.get_node_recursive_value({"root", "database", "pool", "size"}), "8");
    EXPECT_EQ(cfg.root.get_node_recursive_value({"root", "routes", "location"}, 0), "/api/v1/{path}");
    EXPECT_EQ(cfg.root.get_node_recursive_value({"root", "routes", "location"}, 1), "/static/{file}");
    EXPECT_EQ(cfg.root.get_node_recursive_value({"root", "worker", "role"}, 2), "writer");
}

TEST(aconf, multi_load_merge) {
    Config cfg;

    // 第一次加载（主配置）
    auto err1 = cfg.load(R"(
root {
  version "1.0.0";
  worker {
    id 1;
    role listener;
  };
};
)");

    // 第二次加载（扩展配置）
    auto err2 = cfg.load(R"(
root {
  worker {
    id 2;
    role processor;
  };
  database {
    type sqlite;
    path "/tmp/test.db";
  };
};
)");

    cfg.root.print_node(1);

    // 检查错误状态
    std::print("第一次错误信息：{} at {}\n", (int)err1.code, err1.line);
    std::print("第二次错误信息：{} at {}\n", (int)err2.code, err2.line);

    // 验证：两个 worker 都存在
    auto worker1 = cfg.root.get_node_recursive_value({"root", "worker", "role"}, 0);
    auto worker2 = cfg.root.get_node_recursive_value({"root", "worker", "role"}, 1);

    ASSERT_TRUE(worker1.has_value());
    ASSERT_TRUE(worker2.has_value());

    EXPECT_EQ(*worker1, "listener");
    EXPECT_EQ(*worker2, "processor");

    // 验证：database 节点成功追加
    auto dbType = cfg.root.get_node_recursive_value({"root", "database", "type"});
    auto dbPath = cfg.root.get_node_recursive_value({"root", "database", "path"});

    ASSERT_TRUE(dbType.has_value());
    ASSERT_TRUE(dbPath.has_value());

    EXPECT_EQ(*dbType, "sqlite");
    EXPECT_EQ(*dbPath, "/tmp/test.db");
}

TEST(ConfigCast, BasicTypes) {
    Config cfg;

    auto err = cfg.load(R"(
root {
    int_val "42";
    float_val "3.1415";
    bool_true "true";
    bool_false "0";
    weird_int "123abc";
};
)");
    ASSERT_EQ(err.code, ConfigLoadResult::OK);

    // int
    auto int_res = cfg.root.get_node_recursive_value_as<int>({"root", "int_val"});
    ASSERT_TRUE(int_res.value.has_value());
    EXPECT_EQ(*int_res.value, 42);
    EXPECT_EQ(int_res.errpos, -1);

    // float
    auto float_res = cfg.root.get_node_recursive_value_as<float>({"root", "float_val"});
    ASSERT_TRUE(float_res.value.has_value());
    EXPECT_FLOAT_EQ(*float_res.value, 3.1415f);
    EXPECT_EQ(float_res.errpos, -1);

    // bool
    auto bool_t = cfg.root.get_node_recursive_value_as<bool>({"root", "bool_true"});
    ASSERT_TRUE(bool_t.value.has_value());
    EXPECT_TRUE(*bool_t.value);
    EXPECT_EQ(bool_t.errpos, -1);

    auto bool_f = cfg.root.get_node_recursive_value_as<bool>({"root", "bool_false"});
    ASSERT_TRUE(bool_f.value.has_value());
    EXPECT_FALSE(*bool_f.value);
    EXPECT_EQ(bool_f.errpos, -1);

    // 部分解析整数
    auto weird = cfg.root.get_node_recursive_value_as<int>({"root", "weird_int"});
    ASSERT_TRUE(weird.value.has_value());
    EXPECT_EQ(*weird.value, 123);
    EXPECT_EQ(weird.errpos, 3); // 'a' 在索引3
}

TEST(ConfigPerfTest, LoadMillionTimes) {
    const std::string configStr = R"(
root {
    int_val "42";
    float_val "3.1415";
    bool_true "true";
    bool_false "0";
    weird_int "123abc";
};
)";

    constexpr int repetitions = 1e5;
    Config cfg;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for(int i = 0; i < repetitions; ++i){
        cfg.clear();  // 清空上一次的数据
        auto result = cfg.load(configStr);
        ASSERT_EQ(result.code, ConfigLoadResult::OK);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    double perCall = elapsed.count() / repetitions;

    std::cout << "Total time: " << elapsed.count() << " seconds\n";
    std::cout << "Time per call: " << perCall << " seconds\n";
}
