#ifndef __CMDLINE_H__
#define __CMDLINE_H__

void		cmdl_init(int argc, char **argv);

const char	*cmdl_get_string(const char *cmd);
int		cmdl_get_long(const char *cmd, long *value);
int		cmdl_get_float(const char *cmd, float *value);

#endif //__CMDLINE_H__