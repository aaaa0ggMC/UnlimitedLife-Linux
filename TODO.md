# 代办
这里是一些想法：
格式
[index][proposed date][content][finished date]

1. 2025/08/31 实现鼠标控制相机旋转 
2. 2025/10/30 阴影实现 
3. 2025/10/31 (和AI聊天突然把adata的改进方向想出来了)对的，adata gen4我打算做一个tree式的架构，就基本上可以让你一直访问下去的那种。然后GDoc使用类似ECS的vector数据形式，vector<std::string> data;/*所有的字面量数据都在这里*/  Node{ opt(std::vector<int> index;/*index里的int指向的是data中的index*/,umap<std::string_view/*key值长期保存*/,int> 这里的第二个指向的是对象池std::vector<Node>}  这样节点获取数据高度依赖GDoc,好处便是我 node.child("SSS")即使不存在这个child也不影响gdoc,gdoc可以返回一个所谓NIL对象而不会崩溃啥的