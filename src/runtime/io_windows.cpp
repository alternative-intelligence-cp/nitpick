#ifdef _WIN32
#include <windows.h>
#include <vector>

// Spawns a child process passing 6 distinct I/O channels via undocumented lpReserved2
void spawn_process_with_6_channels(const char* cmd, HANDLE hDbg, HANDLE hDati, HANDLE hDato) {
   STARTUPINFOA si;
   ZeroMemory(&si, sizeof(si));
   si.cb = sizeof(si);

   // Gather all 6 handles
   std::vector<HANDLE> handles = {
       GetStdHandle(STD_INPUT_HANDLE), 
       GetStdHandle(STD_OUTPUT_HANDLE), 
       GetStdHandle(STD_ERROR_HANDLE),
       hDbg, hDati, hDato
   };
   // Construct undocumented lpReserved2 buffer
   // Layout: [Count (4 bytes)][Flags (Count bytes)][Handles (Count * 4 bytes)]
   DWORD count = handles.size();
   DWORD cbReserved2 = 4 + count + (count * sizeof(HANDLE));
   // Allocate buffer in Local Heap (required by CreateProcess mechanics)
   LPBYTE lpReserved2 = (LPBYTE)LocalAlloc(LPTR, cbReserved2);
   // Write Count
   *((DWORD*)lpReserved2) = count;
   
   // Pointers to sections in the buffer
   LPBYTE flags_ptr = lpReserved2 + 4;
   UNALIGNED HANDLE* handle_ptr = (UNALIGNED HANDLE*)(lpReserved2 + 4 + count);
   // Set Flags (0x01 = FOPEN) and Copy Handles
   for(int i=0; i<count; i++) {
       flags_ptr[i] = 0x01;
       // Mark as Open File/Pipe
       handle_ptr[i] = handles[i];
       // Critical: Handles must be marked inheritable for child to use them
       SetHandleInformation(handles[i], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
   }

   si.cbReserved2 = (WORD)cbReserved2;
   si.lpReserved2 = lpReserved2;
   // Flag to tell Windows to check std handles (and by extension reserved2)
   si.dwFlags |= STARTF_USESTDHANDLES;
   PROCESS_INFORMATION pi;
   if (CreateProcessA(NULL, (LPSTR)cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
       // Success - close handles in parent if necessary
       CloseHandle(pi.hProcess);
       CloseHandle(pi.hThread);
   }
   LocalFree(lpReserved2);
}
#endif
