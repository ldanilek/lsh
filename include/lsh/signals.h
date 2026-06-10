#ifndef SIGNALS_H
#define SIGNALS_H

#include <lsh/shell.h>

void signals_setup(Shell *sh);
void signals_set_foreground(Shell *sh, pid_t pgid);
void signals_restore_terminal(Shell *sh);
void signals_save_terminal(Shell *sh);
void signals_reset_for_exec(void);
void signals_child_prepare_tty(void);
void signals_handoff_foreground(Shell *sh, pid_t pgid);
void signals_reclaim_foreground(Shell *sh);

#endif
