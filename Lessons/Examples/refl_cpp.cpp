#include <alib5/alogger.h>
#include <alib5/adata.h>
#include <alib5/aperf.h>
#include <alib5/compact/manip_table.h>
#include <meta>

using namespace alib5;

namespace defaults{
    constexpr std::string_view unnamed_person = "Unnamed";
    constexpr size_t age = 18;
}

// 由于students内部都包含default value,因此可以自动生成出新的student
struct Student{
    // default value只支持传入左值
    [[=attr::schema::default_value(defaults::unnamed_person)]]
    std::string name;

    [[=attr::schema::default_value(defaults::age)]]
    size_t age;
};

struct Class{
    // 默认忽略指针/引用，不过我开启了strict mode,因此对于指针以及部分引用类型需要手动跳过
    [[=attr::skip()]]
    int * ptr;

    /// 班级名字
    [[=attr::rename<"class_name">()]]
    [[=attr::schema::validate_args<
        "valid_class_name",
        "2025_doc_version",
        "preview_doc_addition"
    >()]]
    std::string name;

    /// 班级口号
    // 一般建议omit_empty和skip成对
    // 这样 to_adata 后的数据绝对能通过校验
    // 否则如果omit了拿去校验会提示缺少变量
    [[=attr::seri::omit_empty()]] 
    [[=attr::schema::skip()]] 
    std::string motto { "" };

    /// 严格实行中班制度😱
    [[=attr::schema::range(1,50)]]
    std::vector<Student> students;

    /// 班级的代数（。。这有啥用？）
    [[=attr::schema::range(0,attr::no_range)]]
    int generation;

    [[=attr::element_attr(
        attr::schema::range(0,10),
        attr::element_attr(
            attr::schema::range(0,100)
        )
    )]]
    [[=attr::schema::range(0,10)]]
    std::vector<std::vector<std::string>> aliases;
};

int main(){
    // 生成schema
    auto schema = generate_schema<Class>();
    aout << "Schema:\n" << schema << fls;
    // 编译schema
    Validator validator;
    {
        auto errors = validator.from_adata(schema);
        if(errors.empty()){
            aout << "Fine,no errors when compiling schema." << fls;
        }else{
            aout << "Error:" << errors << fls;
        }
    }

    // 生成数据
    Class cl;
    cl.name = "Hello";
    cl.generation = -1;
    cl.motto = "";

    // 转换成 adata
    AData parsed = to_adata(cl);
    aout << "DATA:\n" << parsed << fls;
    
    // 修改数据 & 回传
    parsed["class_name"] = "Reflection";
    from_adata(cl,parsed);
    aout << "MODIFIED: " << cl.name << fls;

    // 校验数据
    aout << validator.validate(
        parsed
    ).recorded_errors << endlog;

    // 数据修复查看(如果有错误可能就不行了)
    aout << "DATA:\n" << parsed << fls;
}