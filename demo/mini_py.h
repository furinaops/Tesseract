#ifndef MINI_PY_H
#define MINI_PY_H

void mini_py_set_output(void (*fn)(char));
int mini_py_exec(const char *source);

#endif
