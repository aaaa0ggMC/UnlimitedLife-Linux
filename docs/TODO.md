# 代办
这里是一些想法：
格式
[index][proposed date][content][finished date]

2. 2025/10/30 阴影实现 
3. 2025/10/31 (和AI聊天突然把adata的改进方向想出来了)对的，adata gen4我打算做一个tree式的架构，就基本上可以让你一直访问下去的那种。然后GDoc使用类似ECS的vector数据形式，vector<std::string> data;/*所有的字面量数据都在这里*/  Node{ opt(std::vector<int> index;/*index里的int指向的是data中的index*/,umap<std::string_view/*key值长期保存*/,int> 这里的第二个指向的是对象池std::vector<Node>}  这样节点获取数据高度依赖GDoc,好处便是我 node.child("SSS")即使不存在这个child也不影响gdoc,gdoc可以返回一个所谓NIL对象而不会崩溃啥的


# 做完的
2025/10/31  1. 2025/08/31 实现鼠标控制相机旋转 
2025/11/2   4. 2025/11/2 把bingyan-Mininginx中的日志DSL搬到alib里面，优化一下后可以作为一个轻量级的配置引擎，核心代码140行，进行富功能化后估计500行左右
2026/09/29  5. 2026/09/29 fog示例：全局Linear Fog（雾色/start/end可由配置与按键实时调节，fog-vert/fog-frag在fragment内按视空间距离mix雾色，清屏色同步为雾色）
2026/09/29  6. 2026/09/29 从AGE移植ModelData概念为ave.shape（Shape纯数据+Prefab程序化生成box/cube/plane/sphere/torus，不与renderer联动，绑定归用户）；完整Mesh/Model规划在ave.model
2026/09/29  7. 2026/09/29 ave.shape 增加 ShapeLoader（OBJ/STL ASCII+Binary 加载，自动识别，相对AGE修复了STL法线错位/支持n-gon/binary越界保护）；tests/ave_shape_loader_test 覆盖全部路径