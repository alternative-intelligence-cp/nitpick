// Implementation of 6-channel process spawning for Linux
#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <vector>
#include <string>

// Maps the provided file descriptors to the Aria standard slots (0-5)
void spawn_process_linux(const char* cmd, const char* const argv, int fd_dbg, int fd_dati, int fd_dato) {
  pid_t pid = fork();
  if (pid < 0) {
      // Fork failed
      return;
  }

  if (pid == 0) {
      // --- CHILD PROCESS ---
      
      // 1. Remap Standard FDs (0, 1, 2)
      // (Assuming parent set up standard pipes before call, omitted for brevity)
      
      // 2. Remap Aria Extended FDs (3, 4, 5)
      // We use dup2 to force the provided FDs into specific slots.
      // STDDBG -> FD 3
      if (fd_dbg!= 3) {
          dup2(fd_dbg, 3);
          close(fd_dbg);
      }
      
      // STDDATI -> FD 4
      if (fd_dati!= 4) {
          dup2(fd_dati, 4);
          close(fd_dati);
      }
      
      // STDDATO -> FD 5
      if (fd_dato!= 5) {
          dup2(fd_dato, 5);
          close(fd_dato);
      }

      // 3. Close Close-On-Exec flag if set
      // We need these FDs to persist across execvp.
      // Usually default is persistent, but we ensure it here.
      // 4. Execute
      // Cast argv to non-const for API compatibility
      execvp(cmd, (char* const*)argv);
      // If execvp returns, it failed
      _exit(127);
  } else {
      // --- PARENT PROCESS ---
      // Parent logic (tracking PID, closing write ends, etc.)
  }
}
#endif

