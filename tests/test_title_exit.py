"""The real title exit requests shell closure and waits, including on refusal."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class TitleExit(unittest.TestCase):
    def test_shell_handoff_never_returns(self):
        source = r'''
#include <cassert>
#include <csetjmp>
#include <cstring>
#include "src/title_exit.cpp"
static std::jmp_buf waited;
static int result, requests;
extern "C" int sceSystemServiceLoadExec(const char *path, const char *const *argv)
{
    assert(!std::strcmp(path, "exit") && argv == nullptr);
    ++requests;
    return result;
}
extern "C" int usleep(useconds_t delay)
{
    assert(delay == 100000 && requests > 0);
    std::longjmp(waited, 1);
}
int main()
{
    for (int status = 0; status < 2; ++status) {
        result = status ? -1 : 0;
        if (!setjmp(waited)) ps5_title_exit(status);
        assert(requests == status + 1);
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'exit.cpp'
            binary = Path(directory) / 'exit'
            path.write_text(source)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I.', str(path), '-o', str(binary)], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True, timeout=5)
