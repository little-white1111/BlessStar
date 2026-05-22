#include "bs/kernel/ir/ir.h"
#include "bs/kernel/report/report.h"

#include <cstdlib>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string read_file_or_empty(const std::string& path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
    {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: bs_adapter_cli <ir_file>\n";
        return 2;
    }

    const std::string raw   = read_file_or_empty(argv[1]);
    IRInstruction*    instr = ir_instruction_create("type", argv[1]);
    if (!instr)
    {
        std::cerr << "failed to create IR instruction\n";
        return 3;
    }

    Report* rep = report_create("adapter_cli");
    report_mark_start(rep);
    report_add_info(rep, "input", raw.empty() ? "(empty file)" : "file read ok");
    report_set_next_target(rep, argv[1]);
    report_set_next_action(rep, "none");
    report_mark_end(rep);

    char* json = report_to_json(rep);
    if (json)
    {
        std::cout << json << "\n";
        std::free(json);
    }

    report_destroy(rep);
    ir_instruction_destroy(instr);
    return 0;
}
